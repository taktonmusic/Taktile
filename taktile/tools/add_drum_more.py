#!/usr/bin/env python3
"""Appends more DRUM SYNTH sounds (engine 5): softer kicks / snares / hats / claps / toms, rims + wood, cymbals (new list, with INIT), shakers, congas, zaps.
Run once on the current files; levels are set afterwards by tools/set_trims_drum.py."""
import re, json
root = '/home/claude/groove/'
P = []
def dr(name, ub, kind, f0, tau1, tau2=0.01, sweep=0, swms=30, mix=0, click=0, drive=0, hp=3000, band=1200, att=0.001):
    assert len(name) <= 9, name
    P.append((name, ub, 5, f0, 0.0, tau1, tau2, att, 0, [kind, sweep, swms, mix, click, drive, hp, band, 0.0, 0.0]))
# kicks
dr('DKickWrm', 0, 0, 52, .6, sweep=2.2, swms=50, click=.05, drive=.05)
dr('DKickSft', 0, 0, 56, .35, sweep=2.0, swms=30, click=.02)
dr('DKickMid', 0, 0, 64, .28, sweep=2.5, swms=25, click=.1, drive=.2)
dr('DKickDeep', 0, 0, 40, 1.0, sweep=1.8, swms=90)
dr('DKickGrv', 0, 0, 58, .38, sweep=3.5, swms=30, click=.25, drive=.3)
dr('DKickAco', 0, 0, 75, .18, sweep=2.2, swms=16, click=.2, mix=.1, hp=900)
dr('DKickTop', 0, 0, 46, .5, sweep=3.2, swms=36, click=.18, drive=.12)
# snares
dr('DSnrSoft', 5, 1, 180, .1, .22, sweep=.5, swms=22, mix=.5, click=.1, hp=1600)
dr('DSnrRimS', 5, 1, 230, .06, .1, sweep=.5, swms=14, mix=.3, click=.5, hp=2000)
dr('DSnrBrsh', 5, 1, 170, .05, .3, sweep=.2, swms=10, mix=.9, hp=2400)
dr('DSnrMid', 5, 1, 200, .11, .2, sweep=.5, swms=20, mix=.55, click=.2, hp=1700)
dr('DSnrPop', 5, 1, 300, .05, .1, sweep=.8, swms=10, mix=.5, click=.3, hp=2200)
dr('DSnrRoom', 5, 1, 165, .14, .55, sweep=.5, swms=30, mix=.55, drive=.1, hp=1100)
dr('DSnrHse', 5, 1, 195, .1, .18, sweep=.5, swms=20, mix=.6, click=.25, hp=2000, drive=.1)
# hats (soft, round)
dr('DHatMute', 8, 2, 200, .02, mix=.2, hp=5000)
dr('DHatHalf', 8, 2, 200, .18, mix=.25, hp=5500)
dr('DHatWarm', 8, 2, 185, .06, mix=.15, hp=3800)
dr('DHatShrt', 8, 2, 210, .04, mix=.25, hp=5200)
dr('DHatDry', 8, 2, 195, .045, mix=.3, hp=4200)
dr('DHatOpSf', 9, 2, 195, .4, mix=.2, hp=4800)
# claps
dr('DClapSft', 6, 3, 1, .09, .16, band=1000, hp=650)
dr('DClapRoom', 6, 3, 1, .12, .6, band=1000, hp=700)
dr('DClapDry', 6, 3, 1, .06, .1, band=1300, hp=900)
dr('DClapSnp', 6, 3, 1, .07, .14, band=1800, hp=1100)
# toms
dr('DTomFlr', 13, 4, 65, .65, sweep=1.4, swms=70, click=.08)
dr('DTomSoft', 13, 4, 105, .4, sweep=1.2, swms=45, click=.03)
dr('DTomDeep', 13, 4, 90, .7, sweep=1.6, swms=65, click=.1)
dr('DTomPerc', 13, 4, 190, .22, sweep=1.5, swms=30, click=.2)
# rims + wood
dr('DRimSoft', 6, 5, 760, .03, click=.3)
dr('DRimWood', 6, 5, 1000, .045, click=.35)
dr('DWoodLo', 14, 5, 650, .06, click=.25)
dr('DWoodHi', 14, 5, 1200, .04, click=.3)
# cymbals: the hat engine, long, softer noise
dr('DCymInit', 11, 2, 200, 1.2, mix=.4, hp=5000)
dr('DCrash', 11, 2, 205, 1.8, mix=.45, hp=4500)
dr('DCrashLg', 11, 2, 190, 2.6, mix=.5, hp=4000)
dr('DCrashSf', 11, 2, 200, 1.4, mix=.35, hp=4200)
dr('DRide', 12, 2, 215, 1.1, mix=.25, hp=5500)
dr('DRideSft', 12, 2, 205, .9, mix=.2, hp=5000)
dr('DRideBel', 12, 6, 520, .8, band=3600)
dr('DCymSplsh', 11, 2, 225, .55, mix=.4, hp=5500)
# percussion
dr('DShaker', 10, 2, 190, .08, mix=1.0, hp=5200)
dr('DShkLong', 10, 2, 190, .16, mix=1.0, hp=4800)
dr('DCongaLo', 14, 4, 190, .25, sweep=.5, swms=20, click=.25)
dr('DCongaHi', 14, 4, 270, .18, sweep=.5, swms=16, click=.3)
dr('DBongo', 14, 4, 340, .14, sweep=.4, swms=14, click=.3)
dr('DCowLo', 14, 6, 470, .3, band=2800)
dr('DZapLow', 17, 4, 400, .3, sweep=4.0, swms=80, click=.1)
dr('DZapHi', 17, 4, 900, .22, sweep=5.0, swms=60, click=.1)
names = [x[0] for x in P]; assert len(set(names)) == len(names)
u = open(root + 'ui.js').read()
tn = eval('[' + re.search(r"const TYPE_NAME = \[(.*?)\];", u).group(1) + ']'); assert not set(names) & set(tn), set(names) & set(tn)
def f(x): return repr(float(x)) + 'f'
n_old = int(re.search(r'N_V4 (\d+)', open(root + 'src/variants4_n.h').read()).group(1)); T0 = 1064 + n_old
rows = ['    { %d, %s, %s, %s, %s, %s, %s, %d, { %s } },  /* %s */' % (eng, f(f0), f(oc), f(t1), f(t2), f(at), f(1.0), sus, ', '.join(f(x) for x in p), n) for (n, ub, eng, f0, oc, t1, t2, at, sus, p) in P]
_t = open(root + 'src/variants4.inc').read().rstrip('\n'); open(root + 'src/variants4.inc', 'w').write(_t + '\n' + '\n'.join(rows) + '\n')
open(root + 'src/variants4_n.h', 'w').write('#define N_V4 %d\n' % (n_old + len(P)))
_t = open(root + 'src/variants4_names.inc').read().rstrip('\n'); open(root + 'src/variants4_names.inc', 'w').write(_t + ',\n    ' + ','.join('"%s"' % n for n in names) + '\n')
def extend(fn, vals):
    t = open(root + fn).read(); i = t.rindex('\n};'); t = t[:i] + ',' + ','.join(str(v) for v in vals) + t[i:]; open(root + fn, 'w').write(t)
extend('src/trim_table.inc', [0] * len(P)); extend('src/type_map.inc', [T0 + i for i in range(len(P))])
m = re.search(r"const TYPE_NAME = \[(.*?)\];", u); u = u.replace(m.group(0), "const TYPE_NAME = [" + m.group(1) + "," + ",".join("'%s'" % x for x in names) + "];", 1)
m = re.search(r"const BASE_T = \[(.*?)\];", u); u = u.replace(m.group(0), "const BASE_T = [" + m.group(1) + "," + ",".join(str(x[1]) for x in P) + "];", 1)
m = re.search(r"const TYPE_MAP = \[(.*?)\];", u); u = u.replace(m.group(0), "const TYPE_MAP = [" + m.group(1) + "," + ",".join(str(T0 + i) for i in range(len(P))) + "];", 1)
m = re.search(r"/\*NXEBEGIN\*/const NX_ENG = (\[.*?\]);/\*NXEEND\*/", u); ne = json.loads(m.group(1)); ne.append([T0, T0 + len(P) - 1, 5])
u = u.replace(m.group(0), "/*NXEBEGIN*/const NX_ENG = " + json.dumps(ne, separators=(',', ':')) + ";/*NXEEND*/", 1)
open(root + 'ui.js', 'w').write(u)
json.dump({'T0': T0, 'names': names}, open('/tmp/new_drum.json', 'w'))
print(len(P), 'new drum sounds, first type', T0)
