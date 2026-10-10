#!/usr/bin/env python3
"""Generates the extra presets (src/variants3*.{h,inc}) that follow 'Sample' in the type list, and patches TYPE_NAME / BASE_T in ui.js.
Each family = N presets that move smoothly between a low and a high value of a few parameters (1 = low/short ... N = high/long)."""
import re, sys
root = '/home/claude/groove/'
B = dict(Kick=0, KickHrd=1, KickGoa=2, RumbKick=3, Sub808=4, Clap=5, Snare=6, Rim=7, HatCl=8, HatOp=9, Shaker=10, Crash=11, Ride=12, Tom=13, Conga=14,
         Cowbell=15, PercFM=16, Zap=17, Riser=18, SubBass=19, RollBass=20, Acid=21, Reese=22, Pluck=23, Stab=24, Pad=25, Lead=26, Bell=27, Sweep=28,
         Impact=29, SynLead=73, SynBass=74, SuperSaw=75, EPiano=76, Organ=77, Strings=78, FMBass=79, Hoover=80)
V = []
def lerp(a, b, i, n): return a if n == 1 else a + (b - a) * i / (n - 1)
def fam(prefix, base, n, waves=None, **rg):
    for i in range(n):
        kw = {}
        for k, (lo, hi) in rg.items():
            v = lerp(lo, hi, i, n)
            kw[k] = round(v, 4) if k not in ('f0',) else round(v, 1)
        if waves: kw['wave'] = waves[i % len(waves)]
        V.append(('%s%d' % (prefix, i + 1) if n < 10 else '%s%02d' % (prefix, i + 1), B[base], kw))
# ---- KICKS
fam('KckTch', 'Kick', 8, f0=(46, 58), tau1=(.12, .30), pet=(.012, .03))
fam('KckHse', 'Kick', 6, f0=(50, 44), tau1=(.2, .4), pet=(.05, .07), trim=(.95, .95))
fam('KckTrn', 'KickGoa', 6, f0=(44, 52), tau1=(.16, .32), pet=(.03, .06))
fam('KckPsy', 'RumbKick', 6, f0=(40, 48), tau1=(.22, .45), pet=(.03, .06))
fam('KckHrd', 'KickHrd', 6, f0=(54, 66), tau1=(.08, .2), pet=(.008, .02))
fam('KckDub', 'Kick', 6, f0=(36, 44), tau1=(.3, .7), pet=(.05, .09), trim=(.9, .9))
fam('808Kck', 'Sub808', 6, f0=(34, 58), tau1=(.3, 1.0))
# ---- SNARE / CLAP / RIM
fam('SnrTch', 'Snare', 8, f0=(150, 230), tau1=(.04, .12), tau2=(.06, .16), pet=(.006, .014))
fam('SnrBig', 'Snare', 6, f0=(110, 150), tau1=(.12, .22), tau2=(.16, .30), pet=(.02, .02))
fam('SnrLof', 'Snare', 6, f0=(130, 170), tau1=(.06, .10), tau2=(.08, .12), trim=(.85, .85))
fam('ClpDry', 'Clap', 6, tau1=(.03, .08), tau2=(.05, .12))
fam('ClpRoom', 'Clap', 6, tau1=(.10, .20), tau2=(.20, .40))
fam('ClpTrp', 'Clap', 4, tau1=(.05, .09), tau2=(.09, .16), trim=(1.05, 1.05))
fam('RimTk', 'Rim', 6, tau1=(.004, .02), tau2=(.002, .012))
# ---- HATS / CYMBALS
fam('HatCl', 'HatCl', 10, tau1=(.008, .04), trim=(1.0, .85))
fam('HatOp', 'HatOp', 10, tau1=(.06, .7))
fam('ShkGrv', 'Shaker', 6, tau1=(.025, .12))
fam('CrsCym', 'Crash', 6, tau1=(.25, 1.6))
fam('RdeCym', 'Ride', 6, tau1=(.3, 1.6), tau2=(.2, .9))
# ---- PERCUSSION
fam('TomSet', 'Tom', 8, f0=(60, 300), tau1=(.4, .12), pet=(.05, .025))
fam('CngSet', 'Conga', 6, f0=(200, 420), tau1=(.25, .10))
fam('BngSet', 'Conga', 4, f0=(380, 520), tau1=(.10, .06))
fam('CowSet', 'Cowbell', 4, tau1=(.05, .35))
fam('FMPerc', 'PercFM', 8, f0=(150, 1600), tau1=(.25, .04), tau2=(.15, .02))
fam('ZapSet', 'Zap', 8, f0=(80, 1400), tau1=(.45, .08), tau2=(.14, .03))
# ---- FX
fam('SweepX', 'Sweep', 8, tau1=(.3, 3.0), tau2=(.2, 1.0))
fam('ImpactX', 'Impact', 8, f0=(24, 70), tau1=(.4, 3.0), tau2=(.1, .6))
# ---- BASS
fam('SubBs', 'SubBass', 8, oct=(1, 2), tau1=(.1, .6), att=(.001, .03))
fam('RollBs', 'RollBass', 8, oct=(1, 4), tau1=(.07, .35))
fam('AcidBs', 'Acid', 10, oct=(1, 4), tau1=(.1, .4), tau2=(.06, .2))
fam('ReeseBs', 'Reese', 8, oct=(1, 4), att=(.002, .12))
fam('SynBs', 'SynBass', 12, waves=[1, 2, 3, 4, 5], det=(0.0, .03), sub=(0.0, .5), tau1=(.1, .5))
fam('FMBs', 'FMBass', 8, oct=(1, 4), tau1=(.12, .4), tau2=(.08, .3))
# ---- LEAD
fam('Pluck', 'Pluck', 12, oct=(4, 16), tau1=(.06, .5), tau2=(.04, .3))
fam('SynLd', 'SynLead', 12, waves=[1, 2, 5, 3, 4], det=(.002, .03), sub=(0.0, .4), att=(.001, .05), tau1=(.1, .3))
fam('Hoov', 'Hoover', 8, waves=[1, 2], det=(.02, .06), oct=(1, 8))
fam('Lead', 'Lead', 8, oct=(4, 16), att=(.001, .08), tau1=(.1, .3))
# ---- CHORDS / PADS / KEYS
fam('Stab', 'Stab', 12, oct=(2, 8), tau1=(.15, .8), tau2=(.06, .25))
fam('Pad', 'Pad', 14, waves=[1, 5, 2], oct=(1, 8), att=(.05, .8))
fam('Bell', 'Bell', 10, oct=(2, 16), tau1=(.5, 2.2))
fam('Super', 'SuperSaw', 10, waves=[1, 2, 5], det=(.004, .04), sub=(0.0, .5))
fam('EPno', 'EPiano', 8, waves=[4, 3, 5, 1], det=(0.0, .03), tau1=(.4, 1.4))
fam('Organ', 'Organ', 6, sub=(0.0, .6), det=(0.0, .03), oct=(2, 8))
fam('Strng', 'Strings', 8, waves=[1, 2, 5], att=(.05, .6), det=(.005, .03))
# ---- 4.9.12: many more synth categories (BASS, LEAD, ARP, CHORDS, PADS, KEYS, BELLS, STRINGS, FX)
fam('DeepBs', 'SubBass', 8, oct=(.5, 1), tau1=(.2, .9), att=(.002, .05))
fam('DubBs', 'SubBass', 8, oct=(.5, 1), tau1=(.4, 1.4), att=(.005, .08))
fam('TechBs', 'SynBass', 8, waves=[1, 2], det=(0.0, .01), sub=(.3, .8), tau1=(.08, .25))
fam('AcdSqr', 'Acid', 8, oct=(1, 2), tau1=(.05, .2), tau2=(.03, .1))
fam('RvDark', 'Reese', 8, oct=(.5, 2), att=(.01, .3))
fam('GoaBs', 'RollBass', 8, oct=(1, 2), tau1=(.04, .12))
fam('WobBs', 'FMBass', 8, oct=(.5, 2), tau1=(.2, .6), tau2=(.15, .5))
fam('DnbBs', 'SynBass', 8, waves=[3, 4], det=(.005, .02), sub=(0.0, .4), tau1=(.15, .4))
fam('HseBs', 'SynBass', 8, waves=[3, 4, 2], det=(0.0, .01), sub=(.2, .6), tau1=(.05, .18))
fam('AcdLd', 'Lead', 8, oct=(4, 8), att=(.001, .02), tau1=(.08, .3))
fam('TrcLd', 'SynLead', 8, waves=[1, 2], det=(.01, .04), att=(.002, .03), tau1=(.2, .4))
fam('PsyLd', 'Lead', 8, oct=(4, 12), att=(.001, .02), tau1=(.12, .3))
fam('SqrLd', 'SynLead', 8, waves=[3, 4], det=(0.0, .01), att=(.001, .02), tau1=(.15, .35))
fam('SftLd', 'SynLead', 8, waves=[5, 2], det=(.002, .02), att=(.02, .15), tau1=(.3, .6))
fam('SyncLd', 'Lead', 6, oct=(2, 8), att=(.001, .01), tau1=(.1, .25))
fam('HoovRv', 'Hoover', 8, waves=[1, 2], det=(.03, .08), oct=(.5, 4))
fam('ArpPl', 'Pluck', 12, oct=(4, 16), tau1=(.03, .1), tau2=(.02, .08))
fam('PlkHrp', 'Pluck', 8, oct=(2, 8), tau1=(.3, .9), tau2=(.2, .6))
fam('PlkMut', 'Pluck', 8, oct=(1, 4), tau1=(.05, .15), tau2=(.03, .1))
fam('DubCrd', 'Stab', 8, oct=(1, 3), tau1=(.3, .9), tau2=(.1, .3))
fam('HseStb', 'Stab', 10, oct=(2, 6), tau1=(.08, .3), tau2=(.03, .12))
fam('RvStb', 'Stab', 8, oct=(2, 8), tau1=(.1, .4), tau2=(.03, .1))
fam('TrcChd', 'SuperSaw', 8, waves=[1, 2], det=(.01, .04), sub=(0.0, .3))
fam('DrkPad', 'Pad', 8, waves=[1, 2], oct=(.5, 2), att=(.2, 1.0))
fam('WrmPad', 'Pad', 8, waves=[5, 2], oct=(1, 4), att=(.1, .6))
fam('AtmPad', 'Pad', 8, waves=[1, 5], oct=(.5, 1), att=(.6, 1.6))
fam('GlsPad', 'Pad', 8, waves=[5, 1], oct=(4, 16), att=(.1, .5))
fam('TrcSaw', 'SuperSaw', 8, waves=[1], det=(.01, .05), sub=(0.0, .4))
fam('SawStk', 'SuperSaw', 8, waves=[2, 1], det=(.004, .02), sub=(0.0, .6))
fam('Rhodes', 'EPiano', 8, waves=[4], det=(0.0, .02), tau1=(.6, 1.8))
fam('Clav', 'EPiano', 8, waves=[3, 2], det=(0.0, .01), tau1=(.1, .4))
fam('Wurli', 'EPiano', 6, waves=[5, 4], det=(.005, .03), tau1=(.3, 1.0))
fam('OrgJzz', 'Organ', 6, sub=(.2, .8), det=(0.0, .01), oct=(1, 4))
fam('OrgRck', 'Organ', 6, sub=(0.0, .5), det=(.01, .04), oct=(2, 8))
fam('BellMlt', 'Bell', 8, oct=(2, 8), tau1=(.2, .8))
fam('Chime', 'Bell', 8, oct=(8, 24), tau1=(.4, 1.5))
fam('Gong', 'Bell', 6, oct=(.5, 2), tau1=(1.0, 2.5))
fam('StrSlw', 'Strings', 8, waves=[1, 2], att=(.3, 1.2), det=(.005, .03))
fam('Choir', 'Strings', 8, waves=[5, 2], att=(.15, .5), det=(.01, .04))
fam('Brass', 'SuperSaw', 6, waves=[2, 1], det=(.002, .01), sub=(0.0, .3))
fam('RiseX', 'Riser', 8, tau1=(.5, 4.0), tau2=(.3, 2.0))
fam('DownX', 'Sweep', 8, tau1=(.3, 2.5), tau2=(.2, 1.2))
fam('NoiseSw', 'Sweep', 8, tau1=(.15, 1.2), tau2=(.1, .6))
fam('ImpHit', 'Impact', 8, f0=(40, 100), tau1=(.15, .8), tau2=(.05, .3))
fam('GlchX', 'PercFM', 10, f0=(400, 2600), tau1=(.06, .01), tau2=(.04, .008))
fam('LaserX', 'Zap', 8, f0=(600, 3000), tau1=(.25, .06), tau2=(.1, .02))
names = [x[0] for x in V]
assert len(set(names)) == len(names), 'duplicate names'
for n in names: assert len(n) <= 9, n
u = open(root + 'ui.js').read()
m = re.search(r"const TYPE_NAME = \[(.*?)\];", u); old = [x.strip().strip("'") for x in m.group(1).split(',')]
old = old[:360]; assert old[-1] == 'Sample', old[-3:]
assert not (set(names) & set(old)), set(names) & set(old)
def f(x): return '-1.0f' if x is None else repr(float(x)) + 'f'
open(root + 'src/variants3_n.h', 'w').write('#define N_V3 %d\n' % len(V))
open(root + 'src/variants3_names.inc', 'w').write(',\n    '.join(','.join('"%s"' % n for n in names[i:i + 6]) for i in range(0, len(names), 6)) + '\n')
rows = []
for n, b, kw in V:
    rows.append('    { %d, %s, %s, %s, %s, %s, %s, %s, %d, %s, %s },  /* %s */' % (b, f(kw.get('f0')), f(kw.get('oct')), f(kw.get('tau1')), f(kw.get('tau2')), f(kw.get('pet')), f(kw.get('trim', 1.0)), f(kw.get('att')), kw.get('wave', 0), f(kw.get('det')), f(kw.get('sub')), n))
open(root + 'src/variants3.inc', 'w').write('\n'.join(rows) + '\n')
u = u.replace(m.group(0), "const TYPE_NAME = [" + ",".join("'%s'" % x for x in old + names) + "];", 1)
m = re.search(r"const BASE_T = \[(.*?)\];", u); ob = m.group(1).split(',')[:360]; assert len(ob) == 360 and ob[-1] == '359'
u = u.replace(m.group(0), "const BASE_T = [" + ",".join(ob + [str(x[1]) for x in V]) + "];", 1)
open(root + 'ui.js', 'w').write(u)
print(len(V), 'new presets, total', 360 + len(V))
