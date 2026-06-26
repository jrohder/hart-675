#!/usr/bin/env python3
"""Extract a HART DD ZIP and emit a JSON manifest of {name,text,encrypted}.

Used by the JavaScriptCore-based validation harness (run_jsc.sh) so the same
ddparser.js the browser uses can be exercised against real vendor packages on
a machine without Node.js.
"""
import io
import json
import sys
import zipfile

TEXT_EXT = ('.dd', '.ddl', '.xml', '.cfg', '.ini', '.h', '.txt')
ENCRYPTED_MARK = b'SIMATIC PDM - Encrypted File'


def build(zip_path):
    files = []
    with zipfile.ZipFile(zip_path) as z:
        for info in z.infolist():
            if info.is_dir():
                continue
            name = info.filename.replace('\\', '/')
            lower = name.lower()
            text, encrypted = '', True
            if lower.endswith(TEXT_EXT):
                try:
                    raw = z.read(info)
                    if raw[:64].find(ENCRYPTED_MARK) >= 0:
                        encrypted = True
                    else:
                        text = raw.decode('utf-8', errors='replace')
                        encrypted = False
                except Exception:
                    encrypted = True
            files.append({'name': name, 'text': text, 'encrypted': encrypted})
    return files


if __name__ == '__main__':
    if len(sys.argv) < 2:
        sys.stderr.write('usage: mkmanifest.py <pkg.zip>\n')
        sys.exit(1)
    out = build(sys.argv[1])
    sys.stdout.write(json.dumps(out))
