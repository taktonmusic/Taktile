#!/usr/bin/env python3
"""Per-kind loudness for all drum-synth sounds (engine 5): trim (half dB) so the first 250 ms sit at a target level per kind; peak never above -1.5 dBFS.
Needs renders /tmp/render/d<type>_84.raw (rend <t0> <t1> 84 1)."""
import re, json
import numpy as np
root = '/home/claude/groove/'
u = open(root + 'ui.js').read()
names = eval('[' + re.search(r"const TYPE_NAME = \[(.*?)\];", u).group(1) + ']')
TG = [(r'^DKick', -19), (r'^DSnr', -25), (r'^DHat(Op|OpSf|Op2|Half)', -30), (r'^DHat', -36), (r'^DClap', -27), (r'^DTom', -20), (r'^D(Rim|Clave|Wood)', -26),
      (r'^D(Cow|Met|RideBel)', -21), (r'^D(Crash|Cym)', -29), (r'^DRide', -31), (r'^DShk|^DShaker', -30), (r'^D(Cong|Bongo)', -20), (r'^DZap', -20)]
t = open(root + 'src/trim_table.inc').read()
m = re.search(r'(\{\n)(.*?)(\n\};)', t, re.S); vals = [int(v) for v in m.group(2).replace('\n', '').split(',') if v.strip() != '']
done = 0
for ty, nm in enumerate(names):
    if ty < 1064 or not re.match(r'^D[A-Z]', nm) or re.match(r'^(DKickInit)$', nm) and False: continue
    tg = next((v for r, v in TG if re.match(r, nm)), None)
    if tg is None: continue
    try: x = np.fromfile('/tmp/render/d%d_84.raw' % ty, dtype=np.int16).astype(float) / 32768
    except Exception: continue
    seg = x[:11000]; rms = 20 * np.log10(np.sqrt((seg ** 2).mean()) + 1e-9); pk = 20 * np.log10(np.abs(x).max() + 1e-9)
    cur = vals[ty]; d = (tg - rms)                       # dB still needed relative to the current render (which already contains the current trim)
    new = int(round(cur + d * 2)); new = max(-24, min(24, new))
    over = pk + (new - cur) * 0.5                        # resulting peak
    if over > -1.5: new = int(cur + np.floor((-1.5 - pk) * 2))
    vals[ty] = max(-24, min(24, new)); done += 1
open(root + 'src/trim_table.inc', 'w').write(t[:m.start(2)] + ','.join(str(v) for v in vals) + t[m.end(2):])
print('trims set for', done, 'drum sounds')
