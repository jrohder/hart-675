#!/usr/bin/env python3
"""Validate Hart 675 device: HART cmd route, Vega cmd 152, profile status."""
import json
import struct
import sys
import time
import urllib.parse
import urllib.request

HOST = sys.argv[1] if len(sys.argv) > 1 else '192.168.4.1'
BASE = f'http://{HOST}'


def get(path):
    return json.loads(urllib.request.urlopen(BASE + path, timeout=15).read())


def hart_cmd(cmd, data=''):
    body = urllib.parse.urlencode({'command': cmd, 'data': data}).encode()
    req = urllib.request.Request(BASE + '/api/hart/cmd', data=body, method='POST')
    j = json.loads(urllib.request.urlopen(req, timeout=15).read())
    cid = j.get('id')
    if not cid:
        return {'state': 'error', 'detail': j}
    for _ in range(50):
        time.sleep(0.2)
        res = json.loads(urllib.request.urlopen(
            BASE + f'/api/hart/cmd?id={cid}', timeout=15).read())
        if res.get('state') in ('done', 'error', 'unknown'):
            return res
        if res.get('enabled') is not None and res.get('id') is None:
            return {'state': 'route_bug'}
    return {'state': 'timeout'}


def hartmon_payload_after_cmd152(t0):
    for _ in range(40):
        time.sleep(0.25)
        frames = json.loads(urllib.request.urlopen(
            BASE + '/api/hartmon', timeout=10).read())
        for f in reversed(frames):
            if f['dir'] != 'RX' or f['t'] <= t0:
                continue
            b = bytes(int(x, 16) for x in f['hex'].split())
            i = 0
            while i < len(b) and b[i] == 0xFF:
                i += 1
            if i >= len(b):
                continue
            lng = bool(b[i] & 0x80)
            al = 5 if lng else 1
            if i + 1 + al + 2 > len(b):
                continue
            cmd = b[i + 1 + al]
            bc = b[i + 1 + al + 1]
            pl = b[i + 1 + al + 2:i + 1 + al + 2 + bc]
            if cmd == 152 and len(pl) >= 2:
                return pl[2:]
    return None


def main():
    ok = True
    st = get('/api/status')
    print('fwVersion:', st.get('fwVersion'))
    h = get('/api/hart')
    print('device:', h.get('manufacturerId'), h.get('deviceType'),
          'valid=', h.get('valid'))
    print('pv:', h.get('pv'))

    poll = json.loads(urllib.request.urlopen(
        BASE + '/api/hart/cmd?id=1', timeout=10).read())
    route_ok = not (poll.get('enabled') is not None and poll.get('id') is None)
    print('OK: /api/hart/cmd GET route' if route_ok else
          'WARN: route bug on firmware (SPA hartmon fallback handles this)')

    mon = get('/api/hartmon')
    t0 = mon[-1]['t'] if mon else 0
    res = hart_cmd(152, '0103FC00AB0B')
    payload = None
    if res.get('state') == 'done' and res.get('data'):
        payload = bytes.fromhex(res['data'])
        print('cmd152 scaling: done', res.get('data', '')[:40])
    elif res.get('state') == 'route_bug':
        payload = hartmon_payload_after_cmd152(t0)
        print('cmd152 scaling: via hartmon fallback')
    else:
        print('cmd152 scaling:', res.get('state'))
        ok = False

    if payload and len(payload) >= 14:
        lrv = struct.unpack('>f', payload[6:10])[0]
        urv = struct.unpack('>f', payload[10:14])[0]
        print('  scaling LRV=%.3f URV=%.3f' % (lrv, urv))
    elif ok:
        print('FAIL: cmd 152 payload too short')
        ok = False

    prof = get('/api/profile/active')
    print('active profile pages:', len(prof.get('pages', [])))
    wired = sum(1 for pg in prof.get('pages', [])
                for w in pg.get('widgets', [])
                if w.get('read') or w.get('write'))
    print('wired widgets:', wired)
    if wired < 10:
        print('WARN: few wired widgets in active profile')

    return 0 if ok else 1


if __name__ == '__main__':
    sys.exit(main())
