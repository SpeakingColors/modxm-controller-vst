#!/usr/bin/env python3
"""Reads the MIDI PARAMETER CHANGE TABLES out of the Yamaha MODX M Data List PDF (En_B0, pages 225 to 256)
and writes a CSV: section, address (with p/n/s/e/k placeholders as printed), size, min, max, name, description, default.
The PDF is Yamaha's document and is NOT in this repo; download it from Yamaha.
Usage: python3 tools/gen_sysex_map.py MODX-M_data_list_En_B0.pdf out.csv      (needs poppler's pdftotext)
Every row is 'official doc only, not verified on hardware'."""
import csv, re, subprocess, sys

FIRST, LAST = 225, 256
H2 = r'[0-9A-F]{2}'
ADDR_TOK = r'[0-9A-F][0-9A-Za-z]'
ROW = re.compile(r'^(?:\S+(?: \S+){0,2}\s{3,})?\s*((?:%s )(?:%s )(?:%s )%s|%s)\s+(\d+)\s{2,}(.*)$' % (ADDR_TOK, ADDR_TOK, ADDR_TOK, ADDR_TOK, ADDR_TOK))
SECTIONS = [('(SYSTEM)', 'System'), ('(CURVE)', 'Curve'), ('(LIVE SET)', 'Live Set'), ('(PERFORMANCE COMMON)', 'Performance Common'),
            ('(PERFORMANCE PART)', 'Performance Part'), ('(NORMAL PART ELEMENT)', 'Normal Part Element'), ('(DRUM VOICE KEY)', 'Drum Key'),
            ('(FM-X PART COMMON)', 'FM-X Part'), ('(AN-X PART)', 'AN-X Part')]

def page_text(pdf, n):
    return subprocess.run(['pdftotext', '-f', str(n), '-l', str(n), '-layout', pdf, '-'], capture_output=True, text=True).stdout

def split_halves(text):
    lines = text.splitlines()
    cols = sorted(set(m.start() for ln in lines for m in re.finditer(r'Address', ln)))
    if not cols: return [lines]
    far = [c for c in cols if c - cols[0] > 50]
    if not far: return [lines]
    cut = far[0] - 6
    return [[l[:cut] for l in lines], [l[cut:] for l in lines]]

def parse_range(rest, size):
    """returns (min, max, remainder, ok). Range is hex bytes; for size 2 the second half is on the next line."""
    if size == 1:
        m = re.match(r'^(%s(?:, %s)*)(?: – (%s))?\s*(.*)$' % (H2, H2, H2), rest)
        if not m: return None
        first = m.group(1).split(', '); lo = int(first[0] if len(first) == 1 else first[0], 16)
        hi = int(m.group(2), 16) if m.group(2) else int(first[-1], 16)
        if len(first) > 1 and not m.group(2): hi = int(first[-1], 16)
        if len(first) > 1 and m.group(2): lo = int(first[1], 16)   # lists like "00, 20 – 7E" are an off value plus a range
        return lo, hi, m.group(3), 'list' if len(first) > 1 else 'ok'
    m = re.match(r'^(%s) (%s) – (.*)$' % (H2, H2), rest)
    if m: return int(m.group(1), 16) * 128 + int(m.group(2), 16), None, m.group(3), 'two'
    return None

totals = []
last_base = [None]

def parse_half(lines, section, out):
    cur = None; name_col = None
    global last_base
    for ln in lines:
        if re.search(r'MODX M Data List|MIDI Data Table|Group Number|TOTAL SIZE', ln):
            if 'TOTAL SIZE' in ln:
                mt = re.search(r'TOTAL SIZE = (\d+)', ln)
                if mt and out and out[-1].get('_base_ref'): totals.append((out[-1]['_base_ref'], int(mt.group(1))))
                cur = None
            continue
        if re.search(r'\bAddress\b', ln): cur = None; continue
        if cur is not None and cur.get('two') and cur['hi'] is None and cur.get('range_col') is not None:
            m2 = re.match(r'^(%s) (%s)\b\s*(.*)$' % (H2, H2), ln.strip())
            if m2 and abs((len(ln) - len(ln.lstrip())) - cur['range_col']) <= 1:
                cur['hi'] = int(m2.group(1), 16) * 128 + int(m2.group(2), 16)
                if m2.group(3): cur['name'] += ' ' + re.split(r'\s{2,}', m2.group(3))[0]
                continue
        m = ROW.match(ln)
        if m:
            a, size, rest = m.group(1), int(m.group(2)), m.group(3)
            pr = parse_range(rest, size) if size <= 2 else None
            if pr is None:                       # rows we do not model (names/paths/blobs of size >2)
                cur = dict(section=section, addr=a, size=size, lo=None, hi=None, name=rest.split('  ')[0], desc='', dflt='', raw=rest, two=False)
            else:
                lo, hi, remainder, kind = pr
                chunks = [c for c in re.split(r'\s{2,}', remainder.strip()) if c]
                name = chunks[0] if chunks else ''
                desc = ''; dflt = ''
                n = size
                for c in chunks[1:]:
                    if re.fullmatch(r'(?:%s ?){%d}' % (H2, n), c): dflt = c.strip(); break
                    desc += (' ' if desc else '') + c
                if not dflt and len(chunks) > 1:
                    t = re.search(r'((?:%s )?%s)$' % (H2, H2), chunks[1])
                    if t and len(t.group(1).split()) == n: dflt = t.group(1); desc = chunks[1][:t.start()].strip()
                cur = dict(section=section, addr=a, size=size, lo=lo, hi=hi, name=name, desc=desc, dflt=dflt, raw=rest, two=(kind == 'two'))
                cur['name_col'] = ln.find(name) if name else None
                rc = ln.find(rest[:2], ln.find(str(size), 0) + 1)
                cur['range_col'] = m.start(3)
            if re.fullmatch(r'(?:%s ){3}%s' % (ADDR_TOK, ADDR_TOK), a): cur['_base'] = a.split()
            if len(a.split()) == 4: last_base[0] = a.split()
            cur['_base_ref'] = ' '.join(last_base[0][:3]) if last_base[0] else None
            out.append(cur); continue
        if cur is None or not ln.strip(): continue
        s = ln.strip()
        if cur.get('two') and cur['hi'] is None:
            m2 = re.match(r'^(%s) (%s)\b\s*(.*)$' % (H2, H2), s)
            if m2:
                cur['hi'] = int(m2.group(1), 16) * 128 + int(m2.group(2), 16)
                rest = m2.group(3)
                if rest: cur['name'] += ' ' + re.split(r'\s{2,}', rest)[0]
                continue
        nc = cur.get('name_col')
        col = len(ln) - len(ln.lstrip())
        if nc is not None and abs(col - nc) <= 2:
            cur['name'] += ' ' + re.split(r'\s{2,}', s)[0]

def main(pdf, outp):
    rows = []; section = ''
    for n in range(FIRST, LAST + 1):
        t = page_text(pdf, n); flat = ' '.join(t.split())
        for key, val in SECTIONS:
            if 'MIDI PARAMETER CHANGE TABLE ' + key in flat or 'MIDI PARAMETER CHANGE TABLE ' + key.replace('(', '').replace(')', '') in flat or key + ' Group Number' in flat: section = val
        for half in split_halves(t): parse_half(half, section, rows)
    # fill in full addresses: a row with one byte keeps the block's first three bytes
    base = None; res = []
    for r in rows:
        parts = r['addr'].split()
        if len(parts) == 4: base = parts
        elif base: parts = base[:3] + [parts[0]]
        else: continue
        r['full'] = ' '.join(parts); res.append(r)
    # cross-check with the 'TOTAL SIZE = n' printed under every block
    bad = 0; docsize = {}
    for ref, tot in totals: docsize.setdefault(ref, set()).add(tot)
    certified = set()
    for ref, tots in docsize.items():
        got = sum(r['size'] for r in res if r.get('_base_ref') == ref)
        if len(tots) == 1 and got == next(iter(tots)): certified.add(ref)
        else: bad += 1; print('NOT CERTIFIED block', ref, 'rows sum', got, 'doc says', sorted(tots))
    print(len(docsize), 'blocks checked,', len(certified), 'certified,', bad, 'not certified')
    with open(outp, 'w', newline='') as f:
        w = csv.writer(f); w.writerow(['section', 'address', 'size', 'min', 'max', 'name', 'description', 'default', 'certified'])
        for r in res:
            w.writerow([r['section'], r['full'], r['size'], '' if r['lo'] is None else r['lo'], '' if r['hi'] is None else r['hi'], ' '.join(r['name'].split()), r['desc'], r['dflt'], 1 if r.get('_base_ref') in certified else 0])
    print(len(res), 'rows')


# ---- second step: pick the rows the plugin uses -> map/sysex_params.csv ----------------------------------------
# python3 tools/gen_sysex_map.py list.pdf raw.csv && python3 tools/gen_sysex_map.py --select raw.csv map/sysex_params.csv
KEEP = {'06 00 01': 'Common', '06 00 02': 'Common', '06 00 0A': 'Master EQ', '06 00 0C': 'Motion Seq', '06 00 0D': 'Super Knob',
        '1p 00 01': 'Part', '1p 00 02': 'Part', '1p 00 06': 'Part Arp'}
def select(raw, outp):
    n = 0
    with open(raw) as f, open(outp, 'w', newline='') as o:
        w = csv.writer(o); w.writerow(['id', 'group', 'name', 'address', 'size', 'min', 'max', 'def', 'hw'])
        for r in csv.DictReader(f):
            blk = ' '.join(r['address'].split()[:3])
            if blk not in KEEP or r['certified'] != '1' or int(r['size']) > 2: continue
            name = ' '.join(r['name'].split())
            if not name or name.lower().startswith('reserved') or r['min'] == '' or r['max'] == '' or not r['default']: continue
            d = r['default'].split(); dv = int(d[0], 16) if len(d) == 1 else int(d[0], 16) * 128 + int(d[1], 16)
            if not (int(r['min']) <= dv <= int(r['max'])): continue          # a default outside the range means a misread row: drop it
            idv = 's_' + r['address'].replace(' ', '').lower()
            w.writerow([idv, KEEP[blk], name, r['address'], r['size'], r['min'], r['max'], dv, 0]); n += 1
    print(n, 'parameters selected')
if len(sys.argv) > 1 and sys.argv[1] == '--select': select(sys.argv[2], sys.argv[3]); sys.exit()

if __name__ == '__main__': main(sys.argv[1], sys.argv[2])
