#!/usr/bin/env python3
"""Appends the new synth presets (clean INIT per role, soft leads, techno bass, muted plucks, dark pads, brass ...) to the preset tables and ui.js.
Run once on the current files. Levels are set afterwards by tools/set_trims.py from a render."""
import re, json, os, sys
root = '/home/claude/groove/'
P = []   # (name, ub, eng, f0, oct, tau1, tau2, att, sus, p[10])
def add(name, ub, eng, p, oct=8.0, tau1=.3, tau2=.01, att=.004, sus=1, f0=0.0):
    p = list(p) + [0.0] * (10 - len(p)); assert len(p) == 10 and len(name) <= 9, name
    P.append((name, ub, eng, f0, oct, tau1, tau2, att, sus, p))
def an(name, ub, wa, wb, det, sub, sync, pw, mix, drv, oct=8, tau1=.3, att=.003, sus=1):
    add(name, ub, 6, [wa, wb, det, sub, sync, pw, mix, drv], oct=oct, tau1=tau1, tau2=.01, att=att, sus=sus)
def wt(name, ub, t0, span, pos, sw, warp, uni, sub, oct=8, tau1=.3, tau2=.5, att=.004, sus=1):
    add(name, ub, 7, [t0, span, pos, sw, warp, uni, sub], oct=oct, tau1=tau1, tau2=tau2, att=att, sus=sus)
def f4(name, ub, r2, r3, r4, ix, fb, al, vel=.5, oct=8, tau1=.8, tau2=.4, att=.003, sus=0):
    add(name, ub, 8, [r2, r3, r4, ix, fb, al, 0, vel], oct=oct, tau1=tau1, tau2=tau2, att=att, sus=sus)
def ad(name, ub, tilt, oe, st, sh, td=0, tr=5, oct=8, tau1=.5, tau2=1.0, att=.004, sus=1):
    add(name, ub, 9, [tilt, oe, st, sh, td, tr], oct=oct, tau1=tau1, tau2=tau2, att=att, sus=sus)
def pd(name, ub, sp, mr, md, pm, body, oct=4, tau1=1.0, att=.5, tau2=.01, sc=.6):
    add(name, ub, 10, [sp, mr, md, pm, body, 0, sc], oct=oct, tau1=tau1, tau2=tau2, att=att, sus=1)
# ---- clean INIT per role: ONE plain oscillator, no detune, no drive (the old AnaInit was two detuned saws = "a choir")
an('AnaInitBs', 74, 0, 0, 0, 0, 0, .5, 0, 0, oct=1, tau1=.25, att=.003)
an('AnaInitPl', 23, 0, 0, 0, 0, 0, .5, 0, 0, oct=8, tau1=.35, att=.002, sus=0)
an('AnaInitPd', 25, 0, 0, 0, 0, 0, .5, 0, 0, oct=4, tau1=.9, att=.35)
an('AnaInitKy', 76, 2, 2, 0, 0, 0, .5, 0, 0, oct=8, tau1=.5, att=.004, sus=0)
an('AnaInitBr', 73, 0, 0, 0, 0, 0, .5, 0, 0, oct=4, tau1=.2, att=.06)
# ---- BASS: techno + house
an('TchRoll', 74, 0, 1, 4, .4, 0, .5, .5, .15, oct=1, tau1=.14, att=.001, sus=0)
an('TchPluck', 74, 1, 0, 3, .5, 0, .38, .5, .1, oct=2, tau1=.12, att=.001, sus=0)
an('HseBass', 74, 0, 1, 6, .4, 0, .5, .6, .1, oct=1, tau1=.26, att=.002, sus=0)
an('HseOff', 74, 1, 3, 0, .5, 0, .45, .5, 0, oct=1, tau1=.16, att=.002, sus=0)
an('TchDrive', 74, 0, 0, 8, .3, 0, .5, .7, .3, oct=1, tau1=.2, att=.001, sus=0)
an('TchSub', 19, 3, 2, 2, .6, 0, .5, .3, 0, oct=1, tau1=.3, att=.003, sus=0)
an('HseWarm', 74, 2, 0, 5, .5, 0, .5, .4, 0, oct=1, tau1=.3, att=.004, sus=0)
wt('WtBsRoll', 74, 40, 120, .3, .4, 0, 3, .35, oct=1, tau1=.15, tau2=.25, att=.002, sus=0)
wt('WtBsSoft', 74, 20, 90, .25, .2, 0, 4, .45, oct=1, tau1=.3, tau2=.4, att=.004)
# ---- LEAD: soft, acid, sync
an('SoftTri', 26, 2, 3, 5, 0, 0, .5, .45, 0, oct=8, tau1=.3, att=.012)
an('SoftSaw', 26, 0, 2, 6, .1, 0, .5, .5, 0, oct=8, tau1=.3, att=.02)
an('Mellow', 26, 2, 2, 7, .15, 0, .5, .6, 0, oct=8, tau1=.35, att=.03)
an('SoftPls', 26, 1, 2, 4, .1, 0, .34, .5, 0, oct=8, tau1=.3, att=.015)
an('Whistle', 26, 3, 3, 1, 0, 0, .5, .2, 0, oct=8, tau1=.3, att=.05)
an('SilkLead', 26, 0, 3, 3, .2, 0, .5, .6, 0, oct=8, tau1=.35, att=.03)
wt('WtSoftLd', 26, 100, 160, .2, .1, 0, 4, 0, oct=8, tau1=.3, tau2=.5, att=.015)
wt('WtAirLd', 26, 160, 200, .35, .2, 0, 6, 0, oct=8, tau1=.35, tau2=.8, att=.03)
an('AcidLd1', 26, 0, 0, 0, .2, 0, .5, 0, .5, oct=8, tau1=.15, att=.001)
an('AcidLd2', 26, 1, 0, 0, .1, 0, .3, 0, .45, oct=8, tau1=.12, att=.001)
an('AcidLd3', 26, 0, 1, 5, .3, 0, .4, .5, .35, oct=4, tau1=.18, att=.001)
an('AcidLd4', 26, 0, 0, 0, .4, 0, .5, 0, .6, oct=4, tau1=.1, att=.001)
an('SyncSoft', 26, 0, 0, 0, .1, .7, .5, 1.0, 0, oct=8, tau1=.3, att=.01)
an('SyncMid', 26, 0, 0, 0, .1, 1.2, .5, 1.0, .1, oct=8, tau1=.3, att=.008)
an('SyncDeep', 26, 0, 0, 0, .3, .5, .5, 1.0, .1, oct=4, tau1=.3, att=.008)
an('SyncPls', 26, 1, 1, 0, .1, 1.0, .4, .9, 0, oct=8, tau1=.2, att=.004)
an('SyncPlk', 23, 0, 0, 0, .1, 1.6, .5, 1.0, .1, oct=8, tau1=.3, att=.002, sus=0)
# ---- PLUCK + STABS: muted, soft plucks, soft stabs
an('MuteSoft', 23, 2, 3, 3, .1, 0, .5, .4, 0, oct=8, tau1=.09, att=.002, sus=0)
an('PalmMute', 23, 0, 1, 5, .2, 0, .4, .5, .1, oct=4, tau1=.08, att=.002, sus=0)
an('MuteTri', 23, 2, 2, 4, 0, 0, .5, .5, 0, oct=8, tau1=.11, att=.002, sus=0)
an('MuteDeep', 23, 1, 3, 0, .5, 0, .45, .4, 0, oct=2, tau1=.1, att=.002, sus=0)
an('MutePwm', 23, 1, 1, 6, .2, 0, .3, .7, 0, oct=8, tau1=.1, att=.002, sus=0)
an('PlkSoft', 23, 2, 3, 4, 0, 0, .5, .5, 0, oct=8, tau1=.35, att=.002, sus=0)
an('PlkSaw', 23, 0, 0, 7, .2, 0, .5, .7, .1, oct=8, tau1=.3, att=.002, sus=0)
an('PlkOct', 23, 3, 3, 1200, 0, 0, .5, .4, 0, oct=8, tau1=.4, att=.002, sus=0)
an('StbSoft', 24, 0, 2, 9, .1, 0, .5, .7, 0, oct=8, tau1=.2, att=.004, sus=0)
an('StbHse', 24, 0, 1, 7, .2, 0, .4, .8, .1, oct=8, tau1=.16, att=.003, sus=0)
an('StbDeep', 24, 1, 0, 5, .3, 0, .4, .7, .1, oct=4, tau1=.25, att=.003, sus=0)
an('StbOct', 24, 0, 0, 1200, .1, 0, .5, .6, 0, oct=4, tau1=.2, att=.003, sus=0)
# ---- PADS: dark, soft, glass
an('DarkPad1', 25, 0, 0, 12, .4, 0, .5, 1.0, 0, oct=2, tau1=1.5, att=.9)
an('DarkPad2', 25, 1, 0, 9, .5, 0, .4, .8, 0, oct=2, tau1=2.0, att=1.2)
an('DarkPad3', 25, 2, 0, 14, .3, 0, .5, .9, 0, oct=1, tau1=2.5, att=1.4)
an('DarkSwll', 25, 0, 0, 18, .2, 0, .5, 1.0, 0, oct=2, tau1=1.8, att=1.6)
wt('WtDarkPd', 25, 0, 200, .3, .3, 0, 10, .2, oct=2, tau1=1.5, tau2=4.0, att=1.0)
an('PadSaw', 25, 0, 0, 11, .1, 0, .5, 1.0, 0, oct=4, tau1=1.2, att=.5)
an('PadTri', 25, 2, 2, 8, .1, 0, .5, .8, 0, oct=4, tau1=1.4, att=.6)
an('PadSine', 25, 3, 3, 6, 0, 0, .5, .6, 0, oct=4, tau1=1.6, att=.7)
pd('PadChill', 25, 10, .18, .25, 0, 3.5, oct=4, tau1=1.2, att=.8)
pd('PadHaze', 25, 16, .12, .5, 0, 4, oct=4, tau1=1.6, att=1.2)
wt('WtGlsPd', 25, 330, 200, .5, .3, 0, 12, 0, oct=8, tau1=1.5, tau2=4.0, att=.8)
ad('AddGlsPd', 25, .5, .2, .4, .9, 0, 5, oct=8, tau1=1.5, tau2=3.0, att=.7)
# ---- KEYS + BRASS
an('KeyTri', 76, 2, 3, 2, 0, 0, .5, .3, 0, oct=8, tau1=.5, att=.004, sus=0)
an('KeySine', 76, 3, 3, 0, 0, 0, .5, 0, 0, oct=8, tau1=.7, att=.004, sus=0)
an('KeyPls', 76, 1, 2, 3, .1, 0, .3, .4, 0, oct=8, tau1=.45, att=.004, sus=0)
an('KeySoft', 76, 2, 3, 6, .1, 0, .5, .5, 0, oct=8, tau1=.8, att=.01, sus=0)
an('BrsSoft', 73, 0, 0, 5, .1, 0, .5, 1.0, 0, oct=4, tau1=.2, att=.09)
an('BrsSwell', 73, 0, 0, 6, .2, 0, .5, 1.0, .1, oct=4, tau1=.3, att=.25)
an('BrsStab', 73, 0, 0, 4, .2, 0, .5, 1.0, .15, oct=4, tau1=.18, att=.02, sus=0)
an('BrsSqr', 73, 1, 0, 5, .1, 0, .4, .8, 0, oct=4, tau1=.2, att=.06)
f4('Fm4BrsSf', 73, 1, 1, 1, 1.0, .1, 0, oct=4, tau1=.3, tau2=.3, att=.1, sus=1)
# ---------------------------------------------------------------- write
names = [x[0] for x in P]; assert len(set(names)) == len(names)
u = open(root + 'ui.js').read()
tn = eval('[' + re.search(r"const TYPE_NAME = \[(.*?)\];", u).group(1) + ']'); assert not set(names) & set(tn), set(names) & set(tn)
def f(x): return repr(float(x)) + 'f'
n_old = int(re.search(r'N_V4 (\d+)', open(root + 'src/variants4_n.h').read()).group(1))
T0 = 1064 + n_old
order = sorted(range(len(P)), key=lambda i: (P[i][2], i)); P = [P[i] for i in order]; names = [x[0] for x in P]      # grouped by engine so every engine gets one contiguous range
rows = ['    { %d, %s, %s, %s, %s, %s, %s, %d, { %s } },  /* %s */' % (eng, f(f0), f(oc), f(t1), f(t2), f(at), f(1.0), sus, ', '.join(f(x) for x in p), n) for (n, ub, eng, f0, oc, t1, t2, at, sus, p) in P]
_t = open(root + 'src/variants4.inc').read().rstrip('\n'); open(root + 'src/variants4.inc', 'w').write(_t + '\n' + '\n'.join(rows) + '\n')
open(root + 'src/variants4_n.h', 'w').write('#define N_V4 %d\n' % (n_old + len(P)))
_t = open(root + 'src/variants4_names.inc').read().rstrip('\n'); open(root + 'src/variants4_names.inc', 'w').write(_t + ',\n    ' + ','.join('"%s"' % n for n in names) + '\n')
def extend(fn, vals):
    t = open(root + fn).read(); i = t.rindex('\n};'); t = t[:i] + ',' + ','.join(str(v) for v in vals) + t[i:]; open(root + fn, 'w').write(t)
extend('src/trim_table.inc', [0] * len(P))
extend('src/type_map.inc', [T0 + i for i in range(len(P))])
m = re.search(r"const TYPE_NAME = \[(.*?)\];", u); u = u.replace(m.group(0), "const TYPE_NAME = [" + m.group(1) + "," + ",".join("'%s'" % x for x in names) + "];", 1)
m = re.search(r"const BASE_T = \[(.*?)\];", u); u = u.replace(m.group(0), "const BASE_T = [" + m.group(1) + "," + ",".join(str(x[1]) for x in P) + "];", 1)
m = re.search(r"const TYPE_MAP = \[(.*?)\];", u); u = u.replace(m.group(0), "const TYPE_MAP = [" + m.group(1) + "," + ",".join(str(T0 + i) for i in range(len(P))) + "];", 1)
m = re.search(r"/\*NXEBEGIN\*/const NX_ENG = (\[.*?\]);/\*NXEEND\*/", u); ne = json.loads(m.group(1))
i = 0
while i < len(P):
    j = i
    while j + 1 < len(P) and P[j + 1][2] == P[i][2]: j += 1
    ne.append([T0 + i, T0 + j, P[i][2]]); i = j + 1
u = u.replace(m.group(0), "/*NXEBEGIN*/const NX_ENG = " + json.dumps(ne, separators=(',', ':')) + ";/*NXEEND*/", 1)
open(root + 'ui.js', 'w').write(u)
json.dump({'T0': T0, 'names': names}, open('/tmp/new_synth.json', 'w'))
print(len(P), 'new synth presets, first type', T0)
