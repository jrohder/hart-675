#!/usr/bin/env python3
"""Generate device profile JSON from a DD .zip using ddparser.js (requires Node.js)."""
import json
import os
import subprocess
import sys
import zipfile

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
PARSER = os.path.join(ROOT, 'tools', 'dd-profiler', 'ddparser.js')
RUNNER = os.path.join(ROOT, 'tools', 'dd-profiler', 'run_parser.mjs')


def extract_zip(path):
    entries = []
    with zipfile.ZipFile(path) as zf:
        for info in zf.infolist():
            if info.is_dir():
                continue
            name = info.filename.replace('\\', '/')
            raw = zf.read(info)
            text = ''
            encrypted = False
            try:
                text = raw.decode('utf-8', errors='strict')
            except UnicodeDecodeError:
                encrypted = True
            entries.append({'name': os.path.basename(name), 'text': text, 'encrypted': encrypted})
    return entries


def main():
    if len(sys.argv) < 2:
        print('Usage: generate_profile.py <package.zip> [out.json]', file=sys.stderr)
        return 1
    zpath = sys.argv[1]
    out = sys.argv[2] if len(sys.argv) > 2 else None

    entries = extract_zip(zpath)
    payload = json.dumps(entries)
    env = os.environ.copy()
    env['DD_ENTRIES'] = payload

    if not os.path.isfile(RUNNER):
        print('Missing run_parser.mjs', file=sys.stderr)
        return 1

    proc = subprocess.run(
        ['node', RUNNER],
        cwd=os.path.dirname(PARSER),
        env=env,
        capture_output=True,
        text=True,
    )
    if proc.returncode != 0:
        print(proc.stderr or proc.stdout, file=sys.stderr)
        return proc.returncode
    result = json.loads(proc.stdout)
    if not result.get('ok'):
        print('Parse failed:', result.get('warnings'), file=sys.stderr)
        return 1
    profile = result['profile']
    if out:
        with open(out, 'w', encoding='utf-8') as f:
            json.dump(profile, f, separators=(',', ':'))
        print(f'Wrote {out} tier={result.get("tier")} pages={len(profile.get("pages", []))}')
    else:
        print(json.dumps(profile))
    return 0


if __name__ == '__main__':
    sys.exit(main())
