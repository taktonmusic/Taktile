#!/usr/bin/env python3
"""Appends the sounds of the three newest engines (16 ACID 303, 17 SWARM, 18 DX 6-OP FM) to the preset tables and to ui.js.
Runs on the files as they were before (copies in /tmp/pre_add) so it can be repeated. Trims (half dB) come from tools/new_trim.json."""
import re, json, os, shutil
root = '/home/claude/groove/'; pre = '/tmp/pre_add/'
for f in ['ui.js', 'src/variants4.inc', 'src/variants4_n.h', 'src/variants4_names.inc', 'src/trim_table.inc', 'src/type_map.inc']:
    shutil.copy(pre + f, root + f)
P = []     # (name, group, ui base, eng, f0, oct, tau1, tau2, att, sus, p[10])
def add(name, group, ub, eng, p, oct, tau1, tau2, att, sus=1):
    p = list(p) + [0.0] * (10 - len(p)); assert len(name) <= 9, name
    P.append((name, group, ub, eng, 0.0, oct, tau1, tau2, att, sus, p))
# ---- ACID 303: p = wave (0 saw .. 1 square), cutoff, resonance, env mod, slide ms, accent depth, drive, sub
G = 'ACID 303'
def ac(name, wave, cut, res, env, sl, acc, drv, sub, oct=2, rel=.08, fdec=.3, ub=21):
    add(name, G, ub, 16, [wave, cut, res, env, sl, acc, drv, sub], oct, rel, fdec, .001)
ac('AcdInit', 0, .40, .55, .55, 60, .6, .0, 0)
ac('Acd303', 0, .35, .65, .60, 60, .7, .1, 0, fdec=.3)
ac('AcdSqr', 1, .35, .60, .60, 60, .7, .1, 0)
ac('AcdSquel', 0, .25, .88, .80, 40, .8, .15, 0, fdec=.22)
ac('AcdDeep', 0, .15, .60, .50, 80, .5, .2, .5, fdec=.45, ub=74)
ac('AcdRez', 0, .50, .95, .30, 50, .6, .1, 0, oct=4)
ac('AcdDrive', 0, .40, .60, .60, 50, .7, .75, .2)
ac('AcdBurp', 0, .10, .80, 1.0, 30, .9, .2, 0, fdec=.12)
ac('AcdTranc', 0, .45, .70, .70, 100, .6, .2, .3, oct=4, rel=.1, fdec=.5, ub=26)
ac('AcdGoa', .3, .40, .85, .90, 70, .8, .3, .3, oct=4, fdec=.25, ub=26)
# ---- SWARM: p = spread, density, size, sine mix
G = 'SWARM'
def sw(name, spr, den, siz, mix, oct, att=.01, rel=.4, ub=26, sus=1):
    add(name, G, ub, 17, [spr, den, siz, mix], oct, rel, .5, att, sus)
sw('SwmInit', .25, .40, .50, .15, 4, att=.02)
sw('SwmFat', .13, .30, .30, .0, 4, att=.01, ub=25)
sw('SwmDrift', .15, .30, .60, .10, 4, att=.03)
sw('SwmCloud', .60, .60, .30, .0, 8)
sw('SwmSwirl', .80, .45, .90, .30, 8, att=.08)
sw('SwmPad', .25, .25, .40, .25, 4, att=.50, rel=1.2, ub=25)
sw('SwmBass', .10, .30, .20, .0, 2, att=.004, rel=.15, ub=74)
sw('SwmLead', .18, .35, .30, .0, 8, att=.004, rel=.25)
sw('SwmChoir', .20, .50, .50, .60, 4, att=.30, rel=.8, ub=25)
sw('SwmStab', .15, .30, .30, .0, 8, att=.002, rel=.25, ub=24, sus=1)
# ---- DX: p = patch, index, feedback add, decay scale (0 = 1), detune cents
G = 'DX 6-OP FM'
def dx(name, patch, oct, idx=1.0, fb=0.0, dsc=0.0, det=0.0, rel=.3, ub=27):
    add(name, G, ub, 18, [patch, idx, fb, dsc, det], oct, rel, .1, .001)
dx('DxInit', 9, 8, ub=26)
dx('DxEPiano', 0, 8)
dx('DxBass', 2, 2, ub=74)
dx('DxBrass', 4, 4, ub=73)
dx('DxBell', 5, 16)
dx('DxMarimba', 7, 8, ub=23)
dx('DxOrgan', 9, 8, fb=4, ub=24)
dx('DxPad', 10, 4, ub=25)
dx('DxLead', 11, 8, ub=26)
dx('DxReese', 13, 2, ub=74)
names = [x[0] for x in P]; assert len(set(names)) == len(names)
trim = json.load(open(root + 'tools/new_trim.json')) if os.path.exists(root + 'tools/new_trim.json') else {}
def f(x): return repr(float(x)) + 'f'
n_old = int(re.search(r'N_V4 (\d+)', open(root + 'src/variants4_n.h').read()).group(1))
T0 = 1064 + n_old                                   # first new type number
rows = []
for (n, g, ub, eng, f0, oc, t1, t2, at, sus, p) in P:
    rows.append('    { %d, %s, %s, %s, %s, %s, %s, %d, { %s } },  /* %s */' % (eng, f(f0), f(oc), f(t1), f(t2), f(at), f(1.0), sus, ', '.join(f(x) for x in p), n))
inc = open(root + 'src/variants4.inc').read().rstrip('\n') + '\n' + '\n'.join(rows) + '\n'
open(root + 'src/variants4.inc', 'w').write(inc)
open(root + 'src/variants4_n.h', 'w').write('#define N_V4 %d\n' % (n_old + len(P)))
nm = open(root + 'src/variants4_names.inc').read().rstrip('\n') + ',\n    ' + ',\n    '.join(','.join('"%s"' % n for n in names[i:i + 6]) for i in range(0, len(names), 6)) + '\n'
open(root + 'src/variants4_names.inc', 'w').write(nm)
def extend(fn, vals):
    t = open(root + fn).read(); i = t.rindex('\n};'); t = t[:i] + ',' + ','.join(str(v) for v in vals) + t[i:]; open(root + fn, 'w').write(t)
extend('src/trim_table.inc', [int(trim.get(n, 0)) for n in names])
extend('src/type_map.inc', [T0 + i for i in range(len(P))])
u = open(root + 'ui.js').read()
m = re.search(r"const TYPE_NAME = \[(.*?)\];", u); u = u.replace(m.group(0), "const TYPE_NAME = [" + m.group(1) + "," + ",".join("'%s'" % x for x in names) + "];", 1)
m = re.search(r"const BASE_T = \[(.*?)\];", u); u = u.replace(m.group(0), "const BASE_T = [" + m.group(1) + "," + ",".join(str(x[2]) for x in P) + "];", 1)
m = re.search(r"const TYPE_MAP = \[(.*?)\];", u); u = u.replace(m.group(0), "const TYPE_MAP = [" + m.group(1) + "," + ",".join(str(T0 + i) for i in range(len(P))) + "];", 1)
runs = {}
for i, x in enumerate(P): runs.setdefault(x[3], []).append(T0 + i)
m = re.search(r"/\*NXEBEGIN\*/const NX_ENG = (\[.*?\]);/\*NXEEND\*/", u); ne = json.loads(m.group(1))
for e, l in runs.items(): ne.append([l[0], l[-1], e])
u = u.replace(m.group(0), "/*NXEBEGIN*/const NX_ENG = " + json.dumps(ne, separators=(',', ':')) + ";/*NXEEND*/", 1)
m = re.search(r"/\*NXBEGIN\*/const NX_GROUPS = (\[.*?\]);/\*NXEND\*/", u); ng = json.loads(m.group(1))
for e, l in runs.items(): ng.append([{16: 'ACID 303', 17: 'SWARM', 18: 'DX 6-OP FM'}[e], l])
u = u.replace(m.group(0), "/*NXBEGIN*/const NX_GROUPS = " + json.dumps(ng, separators=(',', ':')) + ";/*NXEND*/", 1)
u = u.replace("12: ['Fold', 'Sweep', 'Symmetry', 'Harmonics'] };", "12: ['Fold', 'Sweep', 'Symmetry', 'Harmonics'], 16: ['Cutoff', 'Reso', 'EnvMod', 'Decay'], 17: ['Spread', 'Density', 'Size', 'Sine'], 18: ['Index', 'Detune', 'Feedback', 'Decay'] };", 1)
u = u.replace("Index: 'Idx', Ratio: 'Rat' };", "Index: 'Idx', Ratio: 'Rat', Cutoff: 'Cut', Reso: 'Res', EnvMod: 'EnvM', Density: 'Dens', Size: 'Size', Sine: 'Sine', Decay: 'Dcy' };", 1)
assert "16: ['Cutoff'" in u and "EnvMod: 'EnvM'" in u
u = u.replace("        { n: 'FOLD', l: nxList(12), initFirst: true },", "        { n: 'ACID 303', l: nxList(16), initFirst: true },\n        { n: 'SWARM', l: nxList(17), initFirst: true },\n        { n: 'DX 6-OP FM', l: nxList(18), initFirst: true },\n        { n: 'FOLD', l: nxList(12), initFirst: true },", 1)
assert "nxList(18)" in u
open(root + 'ui.js', 'w').write(u)
print(len(P), 'new sounds, first', T0, 'total', T0 + len(P))
