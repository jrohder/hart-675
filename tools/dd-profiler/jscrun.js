// JavaScriptCore (osascript -l JavaScript) validation runner.
// Reads the parser source + a files-manifest JSON (paths via env DD_PARSER /
// DD_MANIFEST) and prints generated-profile diagnostics. Lets us validate the
// exact browser parser on real packages without Node.js.
function readFile(p) {
  var s = $.NSString.stringWithContentsOfFileEncodingError(
    $(p), $.NSUTF8StringEncoding, null);
  return ObjC.unwrap(s) || '';
}
function env(k) {
  var v = $.NSProcessInfo.processInfo.environment.objectForKey(k);
  return v ? ObjC.unwrap(v) : '';
}

var parserSrc = readFile(env('DD_PARSER'));
var DDParser;
(function () {
  var module = { exports: {} };
  eval(parserSrc);
  DDParser = module.exports;
})();

var files = JSON.parse(readFile(env('DD_MANIFEST')));
var textCount = files.filter(function (f) { return f.text; }).length;
var encCount = files.filter(function (f) { return f.encrypted; }).length;

var res = DDParser.parsePackage(files);
var lines = [];
lines.push('  files: ' + files.length + ' (text:' + textCount + ' enc:' + encCount + ')');
lines.push('  tier: ' + res.tier + '  ok: ' + res.ok);
lines.push('  identity: ' + JSON.stringify(res.identity));
if (res.profile) {
  var p = res.profile;
  lines.push('  name: ' + p.manufacturer + ' / ' + p.device + ' rev' + p.revision);
  lines.push('  match: ' + JSON.stringify(p.match) +
    '  device_type: 0x' + (p.device_type >>> 0).toString(16).toUpperCase());
  lines.push('  pages: ' + p.pages.length +
    (p.variables_count != null ? ('  vars:' + p.variables_count + ' menus:' + p.menus_count) : ''));
  lines.push('  filename: ' + DDParser.suggestFilename(p));
  p.pages.slice(0, 8).forEach(function (pg) {
    lines.push('    PAGE: ' + pg.title + ' (' + pg.widgets.length + ' widgets)');
    pg.widgets.slice(0, 5).forEach(function (w) {
      lines.push('       - ' + w.type + ' : ' + (w.label || ''));
    });
  });
  lines.push('  profile JSON size: ' + JSON.stringify(p).length + ' bytes');
}
if (res.warnings.length) {
  lines.push('  warnings:');
  res.warnings.forEach(function (w) { lines.push('    * ' + w); });
}
lines.join('\n');
