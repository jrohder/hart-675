#!/usr/bin/env python3
"""Quick Tier-C sanity check against a DD ZIP (Python-only, no Node)."""
import json
import re
import subprocess
import sys

MK = __file__.replace('validate_tierc.py', 'mkmanifest.py')


def preprocess(src):
    src = re.sub(r'/\*[\s\S]*?\*/', ' ', src)
    src = re.sub(r'(^|[^:])//[^\n\r]*', r'\1', src)
    return src


def parse_variables(src):
    vars_ = {}
    for m in re.finditer(r'\bVARIABLE\s+(var[A-Za-z_]\w*)\s*\{', src):
        name = m.group(1)
        body = src[m.end():m.end() + 800]
        hand = re.search(r'HANDLING\s+([^;\n]+);', body)
        typ = re.search(r'TYPE\s+([A-Za-z_]+)', body)
        handling = (hand.group(1) if hand else '').upper()
        vars_[name] = {
            'read': 'READ' in handling,
            'write': 'WRITE' in handling,
            'type': typ.group(1).upper() if typ else '',
        }
    return vars_


def var_size(v):
    t = v.get('type', '')
    if 'FLOAT' in t or 'DOUBLE' in t:
        return 4
    if 'ENUM' in t or 'enum' in t:
        return 1
    return 4


def layout(tokens, vars_):
    off = 0
    fields = []
    for tk in tokens:
        if re.fullmatch(r'0x[0-9a-f]+|\d+', tk, re.I):
            off += 1
            continue
        if tk.startswith('var') and tk in vars_:
            sz = var_size(vars_[tk])
            fields.append((tk, off, sz))
            off += sz
    return fields


def tokenize_template(raw):
    return [t for t in re.findall(r'0x[0-9a-f]+|\d+|var[A-Za-z_]\w*', raw, re.I)
            if t not in ('response_code', 'device_status')]


def parse_access(src, vars_):
    access = {}
    for cm in re.finditer(r'\bCOMMAND\s+\w+\s*\{', src):
        body = src[cm.end():]
        depth = 1
        i = 0
        while i < len(body) and depth:
            if body[i] == '{':
                depth += 1
            elif body[i] == '}':
                depth -= 1
            i += 1
        block = body[:i - 1]
        num = re.search(r'\bNUMBER\s+(\d+)\s*;', block)
        if not num:
            continue
        cmd = int(num.group(1))
        op = re.search(r'\bOPERATION\s+(READ|WRITE|COMMAND)\s*;', block)
        operation = op.group(1) if op else 'READ'
        for tm in re.finditer(r'\bTRANSACTION\s+(\d+)\s*\{', block):
            tr = block[tm.end():]
            d = 1
            j = 0
            while j < len(tr) and d:
                if tr[j] == '{':
                    d += 1
                elif tr[j] == '}':
                    d -= 1
                j += 1
            trb = tr[:j - 1]
            req = re.search(r'REQUEST\s*\{([^}]*)\}', trb)
            rep = re.search(r'REPLY\s*\{([^}]*)\}', trb)
            req_toks = tokenize_template(req.group(1)) if req else []
            rep_toks = tokenize_template(rep.group(1)) if rep else []
            if operation in ('READ', 'COMMAND') and rep:
                for name, off, sz in layout(rep_toks, vars_):
                    v = vars_.get(name)
                    if not v or not v['read']:
                        continue
                    access.setdefault(name, {})['read'] = (cmd, off)
            if operation in ('WRITE', 'COMMAND') and req:
                for name, off, sz in layout(req_toks, vars_):
                    v = vars_.get(name)
                    if not v or not v['write']:
                        continue
                    access.setdefault(name, {})['write'] = (cmd, off)
    return access


def main():
    zip_path = sys.argv[1]
    manifest = subprocess.check_output([sys.executable, MK, zip_path], text=True)
    files = json.loads(manifest)
    text = '\n'.join(f['text'] for f in files if f.get('text') and f['name'].lower().endswith('.dd'))
    pp = preprocess(text)
    vars_ = parse_variables(pp)
    access = parse_access(pp, vars_)
    print('variables:', len(vars_))
    print('wired variables:', len(access))
    samples = [
        'varLevel_Scaling_MinScaledValue',
        'varLevel_Scaling_MaxScaledValue',
        'varLevel_IntegrationTime_ResponseTime',
        'varLevel_PhysicalDistanceAndHeightValues_Value1',
        'varLevel_ProportionalValueAndLinValues_Value1',
    ]
    for s in samples:
        if s in access:
            print(' ', s, access[s])


if __name__ == '__main__':
    main()
