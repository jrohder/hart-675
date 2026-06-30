import { readFileSync } from 'fs';
import { fileURLToPath } from 'url';
import { dirname, join } from 'path';

const __dirname = dirname(fileURLToPath(import.meta.url));
const src = readFileSync(join(__dirname, 'ddparser.js'), 'utf8');
const wrapped = src + '\n;globalThis.DDParser=DDParser;';
const fn = new Function(wrapped + '; return DDParser;');
const DDParser = fn();
const entries = JSON.parse(process.env.DD_ENTRIES || '[]');
const res = DDParser.parsePackage(entries);
process.stdout.write(JSON.stringify(res));
