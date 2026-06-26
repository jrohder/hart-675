#!/usr/bin/env python3
"""Regenerate src/DDParserJS.h from tools/dd-profiler/ddparser.js.

The .js file is the single source of truth (validated offline against real
vendor packages by run_jsc). The firmware serves a verbatim copy at
/ddparser.js, so run this whenever ddparser.js changes:

    python3 tools/dd-profiler/gen_header.py
"""
import os

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
SRC = os.path.join(ROOT, 'tools', 'dd-profiler', 'ddparser.js')
OUT = os.path.join(ROOT, 'src', 'DDParserJS.h')


def main():
    src = open(SRC, 'r', encoding='utf-8').read()
    if ')JS' in src:
        raise SystemExit('ERROR: ddparser.js contains the raw-string delimiter )JS')
    hdr = (
        '#ifndef DD_PARSER_JS_H\n'
        '#define DD_PARSER_JS_H\n\n'
        '#include <Arduino.h>\n\n'
        '// Auto-generated from tools/dd-profiler/ddparser.js (single source of truth,\n'
        '// validated offline against real vendor DD packages). Served at /ddparser.js\n'
        '// and consumed by the browser DD profiler. Do not hand-edit; regenerate with:\n'
        '//   python3 tools/dd-profiler/gen_header.py\n\n'
        'static const char DDPARSER_JS[] PROGMEM = R"JS(\n'
        + src +
        '\n)JS";\n\n'
        '#endif  // DD_PARSER_JS_H\n'
    )
    open(OUT, 'w', encoding='utf-8').write(hdr)
    print('wrote', OUT, len(hdr), 'bytes')


if __name__ == '__main__':
    main()
