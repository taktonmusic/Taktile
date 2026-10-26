#!/usr/bin/env python3
"""Sets the per-sound level trims (half dB) of the sounds listed in /tmp/new_synth.json (or argv[1]) so that they are as loud as the older synth sounds.
Needs renders: /tmp/render/s<type>_84.raw for the new sounds and the tonal reference sounds (/tmp/tonal_ids.txt)."""
import re, json, sys, os
import numpy as np
root = '/home/claude/groove/'
def loud(path):
    x = np.fromfile(path, dtype=np.int16).astype(float) / 32768
    seg = x[1000:12000]; return 20 * np.log10(np.sqrt((seg ** 2).mean()) + 1e-9)
ref_ids = list(map(int, open('/tmp/tonal_ids.txt').read().split()))
ref = float(np.median([loud('/tmp/render/s%d_84.raw' % i) for i in ref_ids if os.path.exists('/tmp/render/s%d_84.raw' % i)]))
meta = json.load(open(sys.argv[1] if len(sys.argv) > 1 else '/tmp/new_synth.json')); T0 = meta['T0']
t = open(root + 'src/trim_table.inc').read()
m = re.search(r'(\{\n)(.*?)(\n\};)', t, re.S); vals = [int(v) for v in m.group(2).replace('\n', '').split(',') if v.strip() != '']
for k, n in enumerate(meta['names']):
    L = loud('/tmp/render/s%d_84.raw' % (T0 + k))
    vals[T0 + k] = int(max(-18, min(18, round((ref - L) * 2))))
open(root + 'src/trim_table.inc', 'w').write(t[:m.start(2)] + ','.join(str(v) for v in vals) + t[m.end(2):])
print('ref %.1f dB; trims set for %d sounds' % (ref, len(meta['names'])))
