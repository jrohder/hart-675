#!/usr/bin/env python3
"""Upload firmware.bin to Hart 675 over WiFi (HTTP OTA)."""
import argparse
import sys
import urllib.request


def main():
    ap = argparse.ArgumentParser(description='WiFi OTA upload for Hart 675')
    ap.add_argument('firmware', help='Path to firmware.bin')
    ap.add_argument('--host', default='192.168.4.1')
    ap.add_argument('--timeout', type=int, default=180)
    args = ap.parse_args()

    with open(args.firmware, 'rb') as f:
        data = f.read()

    url = f'http://{args.host}/api/firmware/upload'
    req = urllib.request.Request(
        url, data=data, method='POST',
        headers={'Content-Type': 'application/octet-stream',
                 'Content-Disposition': 'attachment; filename=firmware.bin'})
    print(f'Uploading {len(data)} bytes to {url} ...')
    try:
        with urllib.request.urlopen(req, timeout=args.timeout) as r:
            print(r.read().decode())
    except Exception as e:
        print('Upload failed:', e, file=sys.stderr)
        return 1

    reboot = urllib.request.Request(
        f'http://{args.host}/api/reboot', data=b'', method='POST')
    try:
        urllib.request.urlopen(reboot, timeout=10)
        print('Reboot requested.')
    except Exception as e:
        print('Reboot request:', e)
    return 0


if __name__ == '__main__':
    sys.exit(main())
