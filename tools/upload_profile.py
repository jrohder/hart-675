#!/usr/bin/env python3
"""Upload a profile JSON to Hart 675 via multipart POST."""
import sys
import uuid
import urllib.request


def upload(host, path):
    with open(path, 'rb') as f:
        body = f.read()
    fn = path.replace('\\', '/').split('/')[-1]
    boundary = '----WebKitFormBoundary' + uuid.uuid4().hex
    data = (
        f'--{boundary}\r\n'
        f'Content-Disposition: form-data; name="profile"; filename="{fn}"\r\n'
        f'Content-Type: application/json\r\n\r\n'
    ).encode() + body + f'\r\n--{boundary}--\r\n'.encode()
    req = urllib.request.Request(
        f'http://{host}/api/profile/upload',
        data=data,
        method='POST',
        headers={'Content-Type': f'multipart/form-data; boundary={boundary}'},
    )
    with urllib.request.urlopen(req, timeout=60) as r:
        print(r.read().decode())


if __name__ == '__main__':
    host = sys.argv[2] if len(sys.argv) > 2 else '192.168.4.1'
    upload(host, sys.argv[1])
