#!/usr/bin/env python3
"""Validate ddparser.js against a vendor DD ZIP (no Node.js required)."""
import json
import os
import subprocess
import sys
import tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
MK = os.path.join(os.path.dirname(__file__), 'mkmanifest.py')
PARSER = os.path.join(os.path.dirname(__file__), 'ddparser.js')
OUT = os.path.join(ROOT, 'examples', 'profiles')


def find_node():
    for p in [
        os.path.join(os.environ.get('TEMP', '/tmp'), 'node', 'node.exe'),
        'node',
    ]:
        try:
            subprocess.run([p, '--version'], capture_output=True, check=True)
            return p
        except Exception:
            continue
    return None


def main():
    if len(sys.argv) < 2:
        sys.stderr.write('usage: validate.py <pkg.zip> [out.json]\n')
        sys.exit(1)
    zip_path = sys.argv[1]
    manifest = subprocess.check_output([sys.executable, MK, zip_path], text=True)
    files = json.loads(manifest)
    node = find_node()
    if not node:
        sys.stderr.write('Node.js not found; install Node or portable node in %TEMP%\\node\\\n')
        sys.exit(1)
    script = f"""
const DDParser = require({json.dumps(PARSER)});
const files = {manifest};
const res = DDParser.parsePackage(files);
console.log(JSON.stringify(res, null, 2));
"""
    with tempfile.NamedTemporaryFile('w', suffix='.js', delete=False) as tf:
        tf.write(script)
        tf_path = tf.name
    try:
        out = subprocess.check_output([node, tf_path], text=True, cwd=os.path.dirname(PARSER))
    finally:
        os.unlink(tf_path)
    res = json.loads(out)
    print('tier:', res.get('tier'), 'ok:', res.get('ok'))
    print('identity:', json.dumps(res.get('identity')))
    if res.get('profile'):
        p = res['profile']
        print('device:', p.get('manufacturer'), '/', p.get('device'))
        print('pages:', len(p.get('pages', [])), 'wired:', p.get('commands_wired'))
        wired = 0
        info = 0
        for pg in p.get('pages', []):
            for w in pg.get('widgets', []):
                if w.get('read') or w.get('write'):
                    wired += 1
                elif w.get('info'):
                    info += 1
        print('widgets with read/write:', wired, 'info-only:', info)
        print('JSON size:', len(json.dumps(p)), 'bytes')
        if len(sys.argv) >= 3:
            out_path = sys.argv[2]
        else:
            os.makedirs(OUT, exist_ok=True)
            slug = DDParser_suggest = res['profile']
            out_path = os.path.join(OUT, 'generated_vegapuls_6x.json')
        with open(out_path, 'w', encoding='utf-8') as f:
            json.dump(p, f, indent=2)
            f.write('\n')
        print('wrote', out_path)
    for w in res.get('warnings', []):
        print('warning:', w)


if __name__ == '__main__':
    main()
