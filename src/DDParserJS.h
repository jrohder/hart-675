#ifndef DD_PARSER_JS_H
#define DD_PARSER_JS_H

#include <Arduino.h>

// Auto-generated from tools/dd-profiler/ddparser.js (single source of truth,
// validated offline against real vendor DD packages). Served at /ddparser.js
// and consumed by the browser DD profiler. Do not hand-edit; regenerate with:
//   python3 tools/dd-profiler/gen_header.py

static const char DDPARSER_JS[] PROGMEM = R"JS(
/*
 * Hart 675 DD Profiler - core parsing engine (pure JS).
 *
 * This is the single source of truth for turning a HART DD/EDD package into a
 * Hart 675 profile JSON. It is written to run unchanged in:
 *   - Node.js (for offline validation against real packages: tools/dd-profiler/run.js)
 *   - the browser (embedded verbatim into src/WebPages.h, fed by a ZIP reader)
 *
 * It NEVER throws to its caller: every stage is defensive and degrades to the
 * lower tier. Tiers:
 *   Tier A (always): identity extraction -> auto-match + generic pages.
 *   Tier B (readable text DDL, e.g. VEGA): MENU/VARIABLE extraction ->
 *           device-specific pages with resolved labels.
 *   Tier C (COMMAND/TRANSACTION blocks): resolved read/write HART command
 *           mappings wired into profile widgets (cmd 152/153 etc.).
 *
 * Input to parsePackage(files): an array of { name, text, encrypted, bytes }
 *   name      - path inside the ZIP (forward slashes)
 *   text      - decoded UTF-8 text (or "" when binary/encrypted)
 *   encrypted - true for "SIMATIC PDM - Encrypted File" or non-text blobs
 *   bytes     - optional Uint8Array (unused by the parser; reserved)
 */
(function (root, factory) {
  if (typeof module === 'object' && module.exports) {
    module.exports = factory();
  } else {
    root.DDParser = factory();
  }
})(typeof self !== 'undefined' ? self : this, function () {
  'use strict';

  var MAX_PAGES = 24;          // cap generated pages (LittleFS / ArduinoJson safe)
  var MAX_WIDGETS_PER_PAGE = 40;
  var MAX_PROFILE_BYTES = 28000;

  // ---- small helpers -------------------------------------------------------
  function baseName(p) {
    var s = String(p || '').replace(/\\/g, '/');
    var i = s.lastIndexOf('/');
    return i >= 0 ? s.slice(i + 1) : s;
  }
  function stripComments(src) {
    // remove /* */ and // comments without eating string contents on // ...
    var out = src.replace(/\/\*[\s\S]*?\*\//g, ' ');
    out = out.replace(/(^|[^:])\/\/[^\n\r]*/g, '$1'); // keep http:// safe-ish
    return out;
  }
  function joinContinuations(src) {
    return src.replace(/\\[ \t]*\r?\n/g, ' ');
  }
  // VEGA/EDD dictionary strings look like:  "English|de|Deutsch|fr|..."  or
  // a leading-tag form  "|en|English".  Pull the English text.
  function dictText(raw) {
    if (raw == null) return '';
    var s = String(raw);
    var m = s.match(/\|en\|([^|]*)/i);
    if (m) return m[1].trim();
    var bar = s.indexOf('|');
    return (bar >= 0 ? s.slice(0, bar) : s).trim();
  }
  function toInt(v) {
    if (v == null) return NaN;
    var s = String(v).trim();
    if (/^0x[0-9a-f]+$/i.test(s)) return parseInt(s, 16);
    if (/^-?\d+$/.test(s)) return parseInt(s, 10);
    return NaN;
  }
  // "_VEGAPULS_64" -> "VEGAPULS 64", "VEGA" -> "VEGA"
  function humanize(sym) {
    return String(sym || '').replace(/^_+/, '').replace(/_+/g, ' ').trim();
  }
  // Fallback manufacturer names by HART manufacturer-ID code.
  var MFR_NAMES = {
    6: 'Emerson', 17: 'Endress+Hauser', 19: 'Fisher', 24: 'Foxboro',
    26: 'ABB', 31: 'Micro Motion', 38: 'Rosemount', 39: 'Yokogawa',
    56: 'Krohne', 58: 'VEGA Grieshaber KG', 96: 'Magnetrol',
    98: 'VEGA Grieshaber KG', 74: 'Micro Motion'
  };

  // ---- identity ------------------------------------------------------------
  // Pulls manufacturer/device/revision from any readable manifest. Works on
  // both VEGA (#define-based .ddl) and Rosemount (devices.xml + filename hex),
  // even when the menu/variable .dd files are encrypted.
  function extractIdentity(files) {
    var id = {
      manufacturerId: null, deviceType: null, deviceRevision: null,
      ddRevision: null, manufacturer: '', device: '',
      legacyDeviceType: null
    };

    // 1) VEGA / EDD #define + MANUFACTURER statement (readable .ddl)
    files.forEach(function (f) {
      if (!f.text || !/\.(ddl|dd)$/i.test(f.name)) return;
      var t = f.text;
      var grab = function (re) { var m = t.match(re); return m ? m[1] : null; };

      var mfrId = grab(/#define\s+_MANUFACTURER_ID\s+(\d+)/);
      var devId = grab(/#define\s+_DEVICE_TYPE_ID\s+(\d+)/);
      var devRev = grab(/#define\s+_CURRENT_DEVICE_REVISION\s+(\d+)/);
      var ddRev = grab(/#define\s+_CURRENT_DD_REVISION\s+(\d+)/);
      var devName = grab(/#define\s+_DEVICE_TYPE_NAME\s+"([^"]*)"/);
      var mfrName = grab(/#define\s+_MANUFACTURER_NAME\s+"([^"]*)"/);

      if (mfrId != null && id.manufacturerId == null) id.manufacturerId = toInt(mfrId);
      if (devId != null && id.deviceType == null) id.deviceType = toInt(devId);
      if (devRev != null && id.deviceRevision == null) id.deviceRevision = toInt(devRev);
      if (ddRev != null && id.ddRevision == null) id.ddRevision = toInt(ddRev);
      if (devName && !id.device) id.device = dictText(devName);
      if (mfrName && !id.manufacturer) id.manufacturer = dictText(mfrName);

      // canonical "MANUFACTURER 98, DEVICE_TYPE 187, DEVICE_REVISION 1, DD_REVISION 2"
      var canon = t.match(/MANUFACTURER\s+(\d+)\s*,\s*DEVICE_TYPE\s+(\d+)\s*,\s*DEVICE_REVISION\s+(\d+)\s*,\s*DD_REVISION\s+(\d+)/);
      if (canon) {
        if (id.manufacturerId == null) id.manufacturerId = toInt(canon[1]);
        if (id.legacyDeviceType == null) id.legacyDeviceType = toInt(canon[2]);
        if (id.deviceRevision == null) id.deviceRevision = toInt(canon[3]);
        if (id.ddRevision == null) id.ddRevision = toInt(canon[4]);
      }
      // symbolic "MANUFACTURER VEGA, DEVICE_TYPE _VEGAPULS_64, DEVICE_REVISION 3"
      var sym = t.match(/MANUFACTURER\s+([A-Za-z_]\w*)\s*,\s*DEVICE_TYPE\s+(_?[A-Za-z]\w*)/);
      if (sym) {
        if (!id.manufacturer) id.manufacturer = humanize(sym[1]);
        if (!id.device) id.device = humanize(sym[2]);
      }
    });

    // 2) Rosemount / FDI devices.xml COMPONENT element
    files.forEach(function (f) {
      if (!f.text || !/devices?\.xml$/i.test(f.name)) return;
      var t = f.text;
      var grabHex = function (re) {
        var m = t.match(re); return m ? toInt(m[1]) : null;
      };
      if (id.deviceRevision == null) {
        id.deviceRevision = grabHex(/<DEVICE_REVISION>\s*(0x[0-9a-f]+|\d+)\s*<\/DEVICE_REVISION>/i);
      }
      if (id.ddRevision == null) {
        id.ddRevision = grabHex(/<DD_REVISION>\s*(0x[0-9a-f]+|\d+)\s*<\/DD_REVISION>/i);
      }
      if (!id.manufacturer) {
        var mm = t.match(/<MANUFACTURER_EXT>\s*([^<]+?)\s*<\/MANUFACTURER_EXT>/i);
        if (mm) id.manufacturer = mm[1].trim();
      }
      if (!id.device) {
        var lm = t.match(/<LABEL>\s*<string>\s*([^<]+?)\s*<\/string>/i);
        if (lm) id.device = lm[1].trim();
      }
    });

    // 3) Filename hex prefix: MM TT [RR DD] (e.g. 267A0107 = mfr26 type7A rev01 dd07
    //    or 6-hex 265104 = mfr26 type51 ddRev04). Also folder _00MMMM_TTTT_RR_.
    files.forEach(function (f) {
      var bn = baseName(f.name);
      var hx = bn.match(/^([0-9a-f]{2})([0-9a-f]{2})([0-9a-f]{2})([0-9a-f]{2})_/i);
      if (hx) {
        if (id.manufacturerId == null) id.manufacturerId = parseInt(hx[1], 16);
        if (id.legacyDeviceType == null) id.legacyDeviceType = parseInt(hx[2], 16);
        if (id.deviceRevision == null) id.deviceRevision = parseInt(hx[3], 16);
        if (id.ddRevision == null) id.ddRevision = parseInt(hx[4], 16);
      }
      // Emerson FDI style: "000026_268c_01.ddl" (mfr_type_rev), with or
      // without surrounding underscores/extension.
      var fm = bn.match(/(?:^|[_/])0*([0-9a-f]{2,6})_([0-9a-f]{3,4})_([0-9a-f]{2})(?:[._]|$)/i);
      if (fm) {
        if (id.manufacturerId == null) id.manufacturerId = parseInt(fm[1], 16);
        if (id.legacyDeviceType == null) id.legacyDeviceType = parseInt(fm[2], 16);
        if (id.deviceRevision == null) id.deviceRevision = parseInt(fm[3], 16);
      }
    });

    // 3b) Manufacturer name fallback from the HART manufacturer-ID code.
    if (!id.manufacturer && id.manufacturerId != null && MFR_NAMES[id.manufacturerId]) {
      id.manufacturer = MFR_NAMES[id.manufacturerId];
    }

    // 4) Reconcile the expanded device type that HART Command 0 reports.
    //    Firmware reads expanded type = (byte1<<8 | byte2). For these vendors
    //    byte1 == manufacturer code, byte2 == legacy device type. If we only
    //    have a legacy type, synthesize the expanded value.
    if (id.deviceType == null && id.manufacturerId != null &&
        id.legacyDeviceType != null) {
      id.deviceType = ((id.manufacturerId & 0xFF) << 8) | (id.legacyDeviceType & 0xFF);
    }
    return id;
  }

  // ---- preprocessor (Tier B) ----------------------------------------------
  // A small, forgiving DDL preprocessor. Resolves object-like #define macros
  // and evaluates #ifdef/#ifndef/#else/#endif against a fixed symbol set that
  // matches how field tools build a HART (non-FF, non-PDM) profile.
  function preprocess(src) {
    var defined = {
      ENHANCED_FEATURES_ENABLED: '1', _HART_COMMUNICATION: '1',
      _UTF8_SUPPORT: '1', _CURRENT_DEVICE_GENERATION_P3: '1',
      DEFAULT_VALUES_ENABLED: '1'
    };
    var undef = { _FF_COMMUNICATION: 1, _PDM_: 1, _PROFIBUS: 1, V4: 1, _FOUNDATION_FIELDBUS: 1 };

    var text = joinContinuations(stripComments(src));
    var lines = text.split(/\r?\n/);

    // collect object-like macros first (single pass; good enough for labels)
    var macros = {};
    lines.forEach(function (ln) {
      var m = ln.match(/^\s*#define\s+([A-Za-z_]\w*)\s+(.+?)\s*$/);
      if (m && !/\(/.test(m[1])) macros[m[1]] = m[2];
    });

    // conditional inclusion
    var out = [];
    var stack = [{ active: true, taken: true }];
    function curActive() { return stack.every(function (s) { return s.active; }); }
    lines.forEach(function (ln) {
      var mIf = ln.match(/^\s*#(ifdef|ifndef)\s+([A-Za-z_]\w*)/);
      var mElse = ln.match(/^\s*#else\b/);
      var mEnd = ln.match(/^\s*#endif\b/);
      var mIf2 = ln.match(/^\s*#if\b(.*)$/);
      if (mIf) {
        var sym = mIf[2];
        var has = (sym in defined) || (sym in macros);
        var cond = (mIf[1] === 'ifdef') ? has : !has && !(sym in defined);
        if (sym in undef && mIf[1] === 'ifndef') cond = true;
        if (sym in undef && mIf[1] === 'ifdef') cond = false;
        stack.push({ active: cond, taken: cond });
        return;
      }
      if (mIf2) { // #if <expr> : assume true unless obviously about excluded syms
        var exprFalse = /_FF_COMMUNICATION|_PDM_|_PROFIBUS|\bV4\b/.test(mIf2[1]);
        stack.push({ active: !exprFalse, taken: !exprFalse });
        return;
      }
      if (mElse) {
        if (stack.length > 1) { var top = stack[stack.length - 1]; top.active = !top.taken; top.taken = true; }
        return;
      }
      if (mEnd) { if (stack.length > 1) stack.pop(); return; }
      if (/^\s*#(define|undef|include|error|pragma|warning)\b/.test(ln)) return;
      if (curActive()) out.push(ln);
    });

    var joined = out.join('\n');
    // expand simple macros used in identifiers/labels (bounded passes)
    for (var pass = 0; pass < 3; pass++) {
      joined = joined.replace(/\b([A-Za-z_]\w*)\b/g, function (whole, name) {
        if (macros[name] != null && /^[\w".]+$/.test(macros[name])) return macros[name];
        return whole;
      });
    }
    return joined;
  }

  // brace-matched block body starting at index of '{'
  function blockBody(s, open) {
    var depth = 0, i = open;
    for (; i < s.length; i++) {
      if (s[i] === '{') depth++;
      else if (s[i] === '}') { depth--; if (depth === 0) return s.slice(open + 1, i); }
    }
    return s.slice(open + 1);
  }

  function buildDictionary(files) {
    var dict = {};
    files.forEach(function (f) {
      if (!f.text || !/dictionary/i.test(f.name)) return;
      var re = /#define\s+(ID\d+)\s+"([^"]*)"/g, m;
      while ((m = re.exec(f.text))) dict[m[1]] = dictText(m[2]);
    });
    return dict;
  }

  function resolveLabel(raw, dict) {
    if (raw == null) return '';
    var s = String(raw).trim().replace(/;$/, '').trim();
    if (/^".*"$/.test(s)) return dictText(s.slice(1, -1));
    var idm = s.match(/\b(ID\d+)\b/);
    if (idm && dict[idm[1]] != null) return dict[idm[1]];
    return '';
  }

  // Stage C (partial): common enum lists used across VEGA DD packages.
  var ENUM_PRESETS = {
    enumyesnolist: [{ v: 0, t: 'No' }, { v: 1, t: 'Yes' }],
    enumyesnolistinverted: [{ v: 0, t: 'Yes' }, { v: 1, t: 'No' }]
  };

  function parseVarEnumOptions(typeName, body, dict) {
    var key = String(typeName || '').toLowerCase();
    if (ENUM_PRESETS[key]) return ENUM_PRESETS[key];
    // inline ENUMERATED { VALUE 0 { LABEL ... } ... }
    var opts = [];
    var re = /VALUE\s+(\d+)\s*\{[^}]*LABEL\s+([^;\n]+);/g, m;
    while ((m = re.exec(body))) {
      opts.push({ v: parseInt(m[1], 10), t: resolveLabel(m[2], dict) || m[1] });
    }
    return opts.length ? opts : null;
  }

  // VARIABLE <name> { LABEL ..; CLASS ..; TYPE ..(..); HANDLING READ[& WRITE]; }
  function parseVariables(src, dict) {
    var vars = {};
    var re = /\bVARIABLE\s+([A-Za-z_]\w*)\s*\{/g, m;
    while ((m = re.exec(src))) {
      var name = m[1];
      var body = blockBody(src, m.index + m[0].length - 1);
      var lab = body.match(/LABEL\s+([^\n;]+);/);
      var typ = body.match(/TYPE\s+([A-Za-z_]+)\s*(?:\(\s*(\d+)\s*\))?/);
      var hand = body.match(/HANDLING\s+([^\n;]+);/);
      var help = body.match(/HELP\s+([^\n;]+);/);
      var label = resolveLabel(lab && lab[1], dict);
      var handling = hand ? hand[1].toUpperCase() : '';
      vars[name] = {
        name: name,
        label: label,
        help: resolveLabel(help && help[1], dict),
        type: typ ? typ[1].toUpperCase() : '',
        size: typ && typ[2] ? parseInt(typ[2], 10) : null,
        read: /READ/.test(handling),
        write: /WRITE/.test(handling),
        hasEnum: /ENUMERATED/.test(typ ? typ[1] : '') || /ENUMERATED/.test(body) ||
                 /^enum/i.test(typ ? typ[1] : ''),
        options: parseVarEnumOptions(typ ? typ[1] : '', body, dict)
      };
    }
    return vars;
  }

  // MENU <name> { LABEL ..; STYLE ..; ITEMS { a, b, IF(x){c}, .. } }
  function parseMenus(src, dict) {
    var menus = {};
    var re = /\bMENU\s+([A-Za-z_]\w*)\s*\{/g, m;
    while ((m = re.exec(src))) {
      var name = m[1];
      var body = blockBody(src, m.index + m[0].length - 1);
      var lab = body.match(/LABEL\s+([^\n;]+);/);
      var style = body.match(/STYLE\s+([A-Za-z_]+)/);
      var items = [];
      var im = body.match(/ITEMS\s*\{/);
      if (im) {
        var itemsBody = blockBody(body, body.indexOf('{', im.index));
        // strip IF(...) wrappers, keep referenced identifiers
        var cleaned = itemsBody.replace(/IF\s*\([^)]*\)\s*\{/g, ' ').replace(/\}/g, ' ');
        var toks = cleaned.split(/[\s,]+/);
        toks.forEach(function (tk) {
          var id = tk.trim();
          if (/^[A-Za-z_]\w*$/.test(id)) items.push(id);
        });
      }
      menus[name] = {
        name: name,
        label: resolveLabel(lab && lab[1], dict),
        style: style ? style[1].toUpperCase() : '',
        items: items
      };
    }
    return menus;
  }

  // ---- Tier C: COMMAND / TRANSACTION command mapper -----------------------
  function parseNumToken(tk) {
    if (tk == null) return null;
    var s = String(tk).trim();
    if (/^0x[0-9a-f]+$/i.test(s)) return parseInt(s, 16) & 0xFF;
    if (/^\d+$/.test(s)) return parseInt(s, 10) & 0xFF;
    return null;
  }

  function varByteSize(v) {
    if (!v) return 4;
    var t = v.type || '';
    if (/FLOAT|DOUBLE/.test(t)) return 4;
    if (/INTEGER|INDEX|SIGNED|UNSIGNED|LONG/.test(t)) return v.size || 4;
    if (/ENUM|enum/.test(t)) return 1;
    if (/ASCII|STRING|PACKED/.test(t)) return v.size || 4;
    return 4;
  }

  function typeToDecode(v) {
    if (!v) return 'float';
    var t = v.type || '';
    if (/FLOAT|DOUBLE/.test(t)) return 'float';
    if (/INTEGER|INDEX|SIGNED|UNSIGNED|LONG/.test(t)) return 'u32';
    if (/ENUM|enum/.test(t)) return 'u8';
    if (/ASCII|STRING/.test(t)) return 'ascii';
    if (/PACKED/.test(t)) return 'packed';
    return 'float';
  }

  function typeToEncode(v) {
    var d = typeToDecode(v);
    if (d === 'u32') return 'u32';
    if (d === 'u8') return 'u8';
    if (d === 'ascii' || d === 'packed') return 'ascii';
    return 'float';
  }

  function bytesToHex(bytes) {
    var h = '';
    for (var i = 0; i < bytes.length; i++) {
      var b = bytes[i] & 0xFF;
      h += (b < 16 ? '0' : '') + b.toString(16);
    }
    return h;
  }

  function tokenizeTemplate(raw) {
    var out = [];
    if (!raw) return out;
    var re = /response_code|device_status|0x[0-9a-f]+|\d+|var[A-Za-z_]\w*/gi;
    var m;
    while ((m = re.exec(raw))) {
      var tk = m[0];
      if (tk === 'response_code' || tk === 'device_status') continue;
      out.push(tk);
    }
    return out;
  }

  // Walk a REQUEST/REPLY template. Literals become bytes; variables are fields.
  function layoutTemplate(tokens, vars) {
    var bytes = [];
    var fields = [];
    for (var i = 0; i < tokens.length; i++) {
      var tk = tokens[i];
      var n = parseNumToken(tk);
      if (n != null) {
        bytes.push(n);
        continue;
      }
      if (/^var/i.test(tk)) {
        var v = vars[tk];
        var size = varByteSize(v);
        fields.push({
          name: tk,
          offset: bytes.length,
          size: size,
          decode: typeToDecode(v),
          encode: typeToEncode(v)
        });
        for (var z = 0; z < size; z++) bytes.push(0);
      }
    }
    return { bytes: bytes, fields: fields, hex: bytesToHex(bytes) };
  }

  // Parse COMMAND blocks and map variables to read/write HART access specs.
  function parseCommandAccess(src, vars) {
    var access = {};
    var re = /\bCOMMAND\s+(\w+)\s*\{/g, m;
    while ((m = re.exec(src))) {
      var body = blockBody(src, m.index + m[0].length - 1);
      var numM = body.match(/\bNUMBER\s+(\d+)\s*;/);
      if (!numM) continue;
      var cmdNum = parseInt(numM[1], 10);
      var opM = body.match(/\bOPERATION\s+(READ|WRITE|COMMAND)\s*;/);
      var op = opM ? opM[1] : 'READ';
      var trRe = /\bTRANSACTION\s+(\d+)\s*\{/g, tm;
      while ((tm = trRe.exec(body))) {
        var trBody = blockBody(body, tm.index + tm[0].length - 1);
        var reqM = trBody.match(/REQUEST\s*\{([^}]*)\}/);
        var repM = trBody.match(/REPLY\s*\{([^}]*)\}/);
        var reqLayout = reqM ? layoutTemplate(tokenizeTemplate(reqM[1]), vars) : null;
        var repLayout = repM ? layoutTemplate(tokenizeTemplate(repM[1]), vars) : null;
        if (op === 'WRITE' || op === 'COMMAND') {
          if (reqLayout) {
            reqLayout.fields.forEach(function (f) {
              var v = vars[f.name];
              if (!v || !v.write) return;
              if (!access[f.name]) access[f.name] = {};
              access[f.name].write = {
                command: cmdNum,
                data: reqLayout.hex,
                dataPrefix: bytesToHex(reqLayout.bytes.slice(0, f.offset)),
                offset: f.offset,
                encode: f.encode,
                size: f.size
              };
            });
          }
        }
        if (op === 'READ' || op === 'COMMAND') {
          if (repLayout) {
            repLayout.fields.forEach(function (f) {
              var v = vars[f.name];
              if (!v || !v.read) return;
              if (!access[f.name]) access[f.name] = {};
              access[f.name].read = {
                command: cmdNum,
                data: reqLayout ? reqLayout.hex : '',
                offset: f.offset,
                decode: f.decode,
                dec: /FLOAT|DOUBLE/.test(v.type || '') ? 3 : undefined
              };
              if (v.size && (f.decode === 'ascii' || f.decode === 'packed')) {
                access[f.name].read.len = v.size;
              }
            });
          }
        }
      }
    }
    return access;
  }

  function countWired(access) {
    var n = 0;
    Object.keys(access).forEach(function (k) {
      if (access[k].read || access[k].write) n++;
    });
    return n;
  }

  function buildUniversalPages() {
    return [
      {
        title: 'Live Process Values',
        widgets: [
          { type: 'section', label: 'Dynamic variables (HART cmd 3)' },
          { type: 'read', label: 'Primary Variable (PV)', units: '',
            read: { command: 3, decode: 'float', offset: 0 } },
          { type: 'read', label: 'Secondary Variable (SV)', units: '',
            read: { command: 3, decode: 'float', offset: 5 } },
          { type: 'read', label: 'Tertiary Variable (TV)', units: '',
            read: { command: 3, decode: 'float', offset: 10 } },
          { type: 'read', label: 'Quaternary Variable (QV)', units: '',
            read: { command: 3, decode: 'float', offset: 15 } },
          { type: 'read', label: 'Loop Current', units: 'mA',
            read: { command: 3, decode: 'float', offset: 20 } },
          { type: 'read', label: 'Percent Range', units: '%',
            read: { command: 3, decode: 'float', offset: 24 } }
        ]
      },
      {
        title: 'Device Identity',
        widgets: [
          { type: 'read', label: 'Tag', units: '',
            read: { command: 13, decode: 'packed', offset: 0, len: 6 } },
          { type: 'read', label: 'Descriptor', units: '',
            read: { command: 12, decode: 'packed', offset: 0, len: 12 } },
          { type: 'read', label: 'Message', units: '',
            read: { command: 17, decode: 'packed', offset: 0, len: 24 } },
          { type: 'read', label: 'Date', units: '',
            read: { command: 17, decode: 'packed', offset: 0, len: 24 } }
        ]
      }
    ];
  }

  // ---- page generation (Tier B/C) -----------------------------------------
  function typeToWidget(v) {
    var t = v.type || '';
    if ((v.options && v.options.length) || v.hasEnum) return 'enum';
    if (/FLOAT|DOUBLE/.test(t)) return 'float';
    if (/INTEGER|INDEX/.test(t)) return 'number';
    if (/ASCII|STRING|PACKED/.test(t)) return 'text';
    if (/ENUM/.test(t)) return 'enum';
    return 'read';
  }

  function generatePages(menus, vars, dict, access) {
    var pages = [];
    if (!menus) return pages;
    access = access || {};

    var names = Object.keys(menus);
    var roots = names.filter(function (n) {
      var mu = menus[n];
      return mu.items && mu.items.length && mu.label;
    });
    roots.sort(function (a, b) { return menus[b].items.length - menus[a].items.length; });

    var seen = {};
    for (var i = 0; i < roots.length && pages.length < MAX_PAGES; i++) {
      var mu = menus[roots[i]];
      if (seen[mu.label]) continue;
      var widgets = [];
      var items = mu.items.slice();
      items.sort(function (a, b) {
        var aw = access[a] ? 1 : 0;
        var bw = access[b] ? 1 : 0;
        return bw - aw;
      });
      items.forEach(function (it) {
        if (widgets.length >= MAX_WIDGETS_PER_PAGE) return;
        var v = vars[it];
        if (v && v.label) {
          var wtype = v.write ? typeToWidget(v) : 'read';
          if (v.write && /FLOAT|INTEGER|DOUBLE/.test(v.type || '')) wtype = 'number';
          var w = { type: wtype, label: v.label };
          if (v.help) w.help = v.help;
          var a = access[v.name];
          if (a && a.read) w.read = a.read;
          if (a && a.write && v.write) w.write = a.write;
          if (v.options && v.options.length) w.options = v.options;
          if (!w.read && !w.write) w.info = true;
          widgets.push(w);
        } else if (menus[it] && menus[it].label) {
          widgets.push({ type: 'section', label: menus[it].label });
        }
      });
      if (widgets.length) {
        seen[mu.label] = 1;
        pages.push({ title: mu.label, widgets: widgets });
      }
    }
    return pages;
  }

  // ---- top level -----------------------------------------------------------
  function parsePackage(files) {
    var result = {
      ok: true, tier: 'A', warnings: [], identity: null, profile: null
    };
    try {
      var id = extractIdentity(files);
      result.identity = id;

      var readable = files.filter(function (f) {
        return f.text && !f.encrypted && /\.(dd|ddl)$/i.test(f.name);
      });
      var encryptedCount = files.filter(function (f) { return f.encrypted; }).length;

      var profile = {
        manufacturer: id.manufacturer || (id.manufacturerId != null ? ('Mfr ' + id.manufacturerId) : 'Unknown'),
        device: id.device || (id.deviceType != null ? ('Type 0x' + (id.deviceType >>> 0).toString(16).toUpperCase()) : 'HART Device'),
        revision: id.deviceRevision != null ? id.deviceRevision : 0,
        version: '1.0',
        author: 'Hart 675 DD Profiler',
        manufacturer_id: id.manufacturerId,
        device_type: id.deviceType,
        dd_revision: id.ddRevision,
        match: {},
        pages: []
      };
      if (id.manufacturerId != null) profile.match.manufacturerId = id.manufacturerId;
      if (id.deviceType != null) profile.match.deviceType = id.deviceType;
      if (id.deviceRevision != null) profile.match.revision = id.deviceRevision;

      // Tier B/C: readable DDL -> menus/variables/pages + command wiring
      if (readable.length) {
        try {
          var concat = readable.map(function (f) { return f.text; }).join('\n');
          var pp = preprocess(concat);
          var dict = buildDictionary(files);
          var vars = parseVariables(pp, dict);
          var menus = parseMenus(pp, dict);
          var access = parseCommandAccess(pp, vars);
          var pages = generatePages(menus, vars, dict, access);
          var wired = countWired(access);
          var nVars = Object.keys(vars).length, nMenus = Object.keys(menus).length;
          if (pages.length) {
            result.tier = wired > 0 ? 'C' : 'B';
            profile.pages = pages;
            profile.variables_count = nVars;
            profile.menus_count = nMenus;
            profile.commands_wired = wired;
            // Prepend universal pages when we have device-specific command maps.
            if (wired > 0) {
              var uni = buildUniversalPages();
              profile.pages = uni.concat(profile.pages);
              if (profile.pages.length > MAX_PAGES) {
                profile.pages = profile.pages.slice(0, MAX_PAGES);
              }
            }
          } else {
            result.warnings.push('Readable DDL found but no renderable menus were extracted; using generic pages.');
          }
          if (wired === 0 && nVars > 0) {
            result.warnings.push('Variables/menus parsed but no COMMAND/TRANSACTION mappings resolved; widgets are display-only.');
          }
        } catch (e) {
          result.warnings.push('DDL parse error (' + (e && e.message) + '); using identity-only profile.');
        }
      }
      if (encryptedCount && !profile.pages.length) {
        result.warnings.push(encryptedCount + ' encrypted DD file(s) present (e.g. SIMATIC PDM). Menus cannot be extracted; the profile provides identity + generic pages.');
      }

      // size guard
      var json = JSON.stringify(profile);
      if (json.length > MAX_PROFILE_BYTES) {
        // trim pages until under cap
        while (profile.pages.length && JSON.stringify(profile).length > MAX_PROFILE_BYTES) {
          profile.pages.pop();
        }
        result.warnings.push('Profile trimmed to fit device storage (' + MAX_PROFILE_BYTES + ' bytes).');
      }

      if (id.manufacturerId == null || id.deviceType == null) {
        result.warnings.push('Could not determine manufacturer/device-type from this package; auto-match may not work. Set them manually before saving.');
      }
      result.profile = profile;
    } catch (e) {
      result.ok = false;
      result.warnings.push('Fatal parse error: ' + (e && e.message));
    }
    return result;
  }

  function suggestFilename(profile) {
    function slug(s) {
      return String(s || '').toLowerCase().replace(/[^a-z0-9]+/g, '_')
        .replace(/^_+|_+$/g, '').slice(0, 24) || 'device';
    }
    var rev = (profile && profile.revision != null) ? ('_rev' + profile.revision) : '';
    return slug(profile && profile.manufacturer) + '_' + slug(profile && profile.device) + rev + '.json';
  }

  return {
    parsePackage: parsePackage,
    extractIdentity: extractIdentity,
    suggestFilename: suggestFilename,
    _internal: {
      preprocess: preprocess, buildDictionary: buildDictionary,
      parseVariables: parseVariables, parseMenus: parseMenus,
      generatePages: generatePages, parseCommandAccess: parseCommandAccess,
      dictText: dictText
    }
  };
});

)JS";

#endif  // DD_PARSER_JS_H
