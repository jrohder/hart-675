#!/usr/bin/env node
/*
 * Offline validation harness for the DD profiler engine.
 *
 *   node run.js <package.zip> [package2.zip ...]
 *
 * Extracts each ZIP to a temp dir (using the system `unzip`), feeds the files
 * to the same ddparser.js that the browser uses, and prints the generated
 * profile + diagnostics. This lets us prove the parser on real vendor packages
 * before embedding it in firmware.
 */
const fs = require('fs');
const os = require('os');
const path = require('path');
const { execFileSync } = require('child_process');
const DDParser = require('./ddparser.js');

const TEXT_EXT = /\.(dd|ddl|xml|cfg|ini|h|txt)$/i;
const ENCRYPTED_MARK = 'SIMATIC PDM - Encrypted File';

function readPackage(zipPath) {
  const tmp = fs.mkdtempSync(path.join(os.tmpdir(), 'ddrun-'));
  execFileSync('unzip', ['-o', '-qq', zipPath, '-d', tmp]);
  const files = [];
  (function walk(dir, rel) {
    for (const ent of fs.readdirSync(dir, { withFileTypes: true })) {
      const abs = path.join(dir, ent.name);
      const r = rel ? rel + '/' + ent.name : ent.name;
      if (ent.isDirectory()) walk(abs, r);
      else {
        let text = '', encrypted = false;
        if (TEXT_EXT.test(ent.name)) {
          try {
            const buf = fs.readFileSync(abs);
            const head = buf.slice(0, 64).toString('latin1');
            if (head.includes(ENCRYPTED_MARK)) { encrypted = true; }
            else { text = buf.toString('utf8'); }
          } catch (e) { encrypted = true; }
        } else { encrypted = true; }
        files.push({ name: r, text, encrypted });
      }
    }
  })(tmp, '');
  fs.rmSync(tmp, { recursive: true, force: true });
  return files;
}

function main() {
  const zips = process.argv.slice(2);
  if (!zips.length) { console.error('usage: node run.js <pkg.zip> ...'); process.exit(1); }
  for (const zip of zips) {
    console.log('\n================================================================');
    console.log('PACKAGE:', path.basename(zip));
    console.log('================================================================');
    let files;
    try { files = readPackage(zip); }
    catch (e) { console.error('  extract failed:', e.message); continue; }
    const textCount = files.filter(f => f.text).length;
    const encCount = files.filter(f => f.encrypted).length;
    console.log(`  files: ${files.length}  (readable text: ${textCount}, binary/encrypted: ${encCount})`);

    const res = DDParser.parsePackage(files);
    console.log('  tier:', res.tier, ' ok:', res.ok);
    console.log('  identity:', JSON.stringify(res.identity));
    if (res.profile) {
      const p = res.profile;
      console.log('  match:', JSON.stringify(p.match),
        ' name:', p.manufacturer + ' / ' + p.device + ' rev' + p.revision);
      console.log('  device_type: 0x' + (p.device_type >>> 0).toString(16).toUpperCase(),
        ' pages:', p.pages.length,
        (p.variables_count != null ? ` vars:${p.variables_count} menus:${p.menus_count}` : ''));
      console.log('  filename:', DDParser.suggestFilename(p));
      p.pages.slice(0, 6).forEach(pg => {
        console.log('    PAGE:', pg.title, '(' + pg.widgets.length + ' widgets)');
        pg.widgets.slice(0, 6).forEach(w =>
          console.log('       -', w.type, ':', w.label || ''));
      });
      const json = JSON.stringify(p);
      console.log('  profile JSON size:', json.length, 'bytes');
    }
    if (res.warnings.length) {
      console.log('  warnings:');
      res.warnings.forEach(w => console.log('    *', w));
    }
  }
}
main();
