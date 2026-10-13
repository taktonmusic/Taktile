#!/usr/bin/env python3
"""Generates the 'NEW ENGINES' presets (src/variants4*.{h,inc}) that follow the 1064 older sounds, and patches ui.js
(TYPE_NAME, BASE_T, NX_GROUPS). Five engines: 0 MODAL, 1 FM, 2 VOX, 3 CHIP, 4 MORPH.
Row: eng, f0, oct, tau1, tau2, att, trim, sus, p[10]

Also writes src/engines.inc (value ranges of the 10 engine parameters, used by the DSP to map the knob values 0..1 of
the pad parameters e0..e9) and the /*ENGBEGIN*/../*ENGEND*/ block of ui.js (engine names, parameter pages, preset lists).
The five INIT presets (one per engine) are appended LAST, so all older sound numbers stay valid.
Run: python3 tools/gen_variants4.py   (TAKTILE_ROOT=<dir with ui.js and src/> to run on another copy)"""
import re, json, os
root = os.environ.get('TAKTILE_ROOT') or os.path.join(os.path.dirname(os.path.abspath(__file__)), '..')
root = root.rstrip('/') + '/'
P = []   # (name, group, uibase, eng, f0, oct, tau1, tau2, att, sus, p[10])
def add(name, group, ub, eng, p, f0=0.0, oct=0.0, tau1=0.5, tau2=0.1, att=0.005, sus=0):
    p = list(p) + [0.0] * (10 - len(p)); assert len(p) == 10 and len(name) <= 9, name
    P.append((name, group, ub, eng, f0, oct, tau1, tau2, att, sus, p))
# ============ MODAL: p = set, bright, burst_ms, tilt, stretch, click, trem_depth, trem_rate
# sets: 0 marimba 1 vibes 2 xylo 3 kalimba 4 glock 5 tubular 6 church 7 steeldrum 8 harp 9 nylon 10 koto 11 timpani 12 membrane 13 wood 14 bowl 15 gamelan 16 crystal 17 plate 18 hollow
M = 'MALLETS'; S = 'STRINGS'; B = 'BELLS + GONGS'; D = 'MODAL PERC'
add('MarimLo', M, 23, 0, [0, .55, 3, .9, 0, .35], oct=4, tau1=.55)
add('MarimMid', M, 23, 0, [0, .6, 3, .9, 0, .3], oct=8, tau1=.45)
add('MarimHi', M, 23, 0, [0, .7, 2, .8, 0, .25], oct=16, tau1=.3)
add('MarimHard', M, 23, 0, [0, .95, 1.5, .7, 0, .5], oct=8, tau1=.35)
add('MarimSft', M, 23, 0, [0, .15, 8, 1.6, 0, .05], oct=4, tau1=.8)
add('VibesMid', M, 27, 0, [1, .5, 4, .4, 0, .15, .35, 5.2], oct=8, tau1=2.0)
add('VibesLo', M, 27, 0, [1, .4, 5, .5, 0, .1, .5, 3.5], oct=4, tau1=2.6)
add('VibesFst', M, 27, 0, [1, .85, 2, .3, .004, .3, .9, 9.0], oct=16, tau1=1.0)
add('XyloMid', M, 23, 0, [2, .85, 1.5, .6, 0, .5], oct=8, tau1=.16)
add('XyloHi', M, 23, 0, [2, .9, 1.2, .5, 0, .5], oct=16, tau1=.12)
add('KalimLo', M, 23, 0, [3, .6, 2, .7, .01, .4], oct=8, tau1=.9)
add('KalimHi', M, 23, 0, [3, .7, 2, .7, .01, .4], oct=16, tau1=.6)
add('Celesta', M, 27, 0, [4, .3, 4, .6, 0, .1], oct=8, tau1=1.6)
add('GlockHi', B, 27, 0, [4, .8, 1.5, .35, 0, .3], oct=32, tau1=2.4)
add('Tubular', B, 27, 0, [5, .3, 4, .25, .002, .4], oct=2, tau1=5.0)
add('ChurchLo', B, 27, 0, [6, .3, 5, .15, 0, .5], oct=1, tau1=8.0)
add('ChurchHi', B, 27, 0, [6, .55, 3, .2, 0, .4], oct=4, tau1=4.5)
add('SteelLo', B, 27, 0, [7, .55, 3, .55, 0, .35], oct=2, tau1=1.4)
add('SteelHi', B, 27, 0, [7, .7, 2, .55, 0, .3], oct=8, tau1=.8)
add('Crystal', B, 27, 0, [16, .75, 1.5, .4, .02, .2], oct=32, tau1=2.2)
add('GlassTap', B, 27, 0, [16, 1.0, .8, .8, .03, .7], oct=32, tau1=.6)
add('BowlLo', B, 27, 0, [14, .35, 8, .1, 0, .15], oct=4, tau1=7.0)
add('BowlHi', B, 27, 0, [14, .5, 6, .15, 0, .15], oct=8, tau1=5.0)
add('GamelanL', B, 27, 0, [15, .55, 3, .3, 0, .4], oct=2, tau1=3.5)
add('GamelanH', B, 27, 0, [15, .65, 2, .35, 0, .4], oct=8, tau1=2.0)
add('PlateLo', B, 27, 0, [17, .9, 1.5, .3, .08, .6], oct=8, tau1=2.5)
add('PlateHi', B, 27, 0, [17, .8, 1.5, .4, 0, .5], oct=16, tau1=1.2)
add('HarpLo', S, 23, 0, [8, .3, 5, .9, 0, .1], oct=2, tau1=2.6)
add('HarpMid', S, 23, 0, [8, .5, 4, .8, 0, .1], oct=8, tau1=1.8)
add('HarpHi', S, 23, 0, [8, .6, 3, .9, 0, .1], oct=16, tau1=1.2)
add('NylonLo', S, 23, 0, [9, .4, 5, 1.0, 0, .25], oct=2, tau1=1.1)
add('NylonHi', S, 23, 0, [9, .75, 3, 1.0, 0, .35], oct=8, tau1=.7)
add('Koto', S, 23, 0, [10, .85, 1.5, .6, .003, .6], oct=4, tau1=1.0)
add('Hollow', S, 23, 0, [18, .5, 3, .4, 0, .2], oct=4, tau1=1.4)
# modal drums (f0 absolute)
add('TimpLo', D, 14, 0, [11, .5, 5, .3, 0, .4], f0=60, tau1=1.3)
add('TimpHi', D, 14, 0, [11, .55, 4, .3, 0, .4], f0=110, tau1=.9)
add('Taiko', D, 13, 0, [11, .35, 8, .25, .002, .5], f0=55, tau1=1.6)
add('TablaLo', D, 14, 0, [12, .95, 1.5, .5, .03, .6], f0=150, tau1=.3)
add('TablaHi', D, 14, 0, [12, .8, 1.5, .5, 0, .5], f0=300, tau1=.25)
add('Bongo', D, 14, 0, [12, .85, 1.2, .6, 0, .6], f0=420, tau1=.18)
add('TomModLo', D, 13, 0, [12, .5, 3, .35, 0, .4], f0=85, tau1=.45)
add('TomModMd', D, 13, 0, [12, .55, 3, .35, 0, .4], f0=130, tau1=.38)
add('TomModHi', D, 13, 0, [12, .45, 4, .35, 0, .3], f0=240, tau1=.4)
add('WoodLo', D, 7, 0, [13, .9, 1, .8, 0, .6], f0=620, tau1=.07)
add('WoodHi', D, 7, 0, [13, .95, .8, .8, 0, .6], f0=1100, tau1=.05)
add('Clave', D, 7, 0, [13, 1.0, .5, .4, 0, .7], f0=2100, tau1=.12)
add('MetalHit', D, 15, 0, [17, .85, 1, .3, 0, .6], f0=360, tau1=.7)
add('Anvil', D, 15, 0, [17, .95, 1, .2, 0, .8], f0=520, tau1=1.2)
add('TinCan', D, 7, 0, [17, .9, 1, .9, .01, .7], f0=820, tau1=.1)
add('PipeHit', D, 15, 0, [18, .8, 1.5, .3, 0, .5], f0=300, tau1=.5)
# ============ FM: p = ratioB, idxB, ratioC, idxC, fb, carrierRatio, sub, velSens, algo, idxFloor
K = 'FM KEYS'; BS = 'FM BASS'; FB = 'FM BELLS + PLUCK'; FL = 'FM LEAD + PAD'; FX = 'FM PERC + FX'
add('EpTine', K, 76, 1, [14, 1.1, 1, 1.6, 0, 1, 0, .8, 1, 0], oct=8, tau1=1.6, tau2=.18)
add('EpWarm', K, 76, 1, [1, 1.3, 7, .35, .05, 1, .1, .6, 1, .05], oct=8, tau1=1.9, tau2=.7)
add('EpBright', K, 76, 1, [14, 1.9, 1, 2.4, 0, 1, 0, .9, 1, 0], oct=8, tau1=1.2, tau2=.12)
add('EpSoft', K, 76, 1, [1, .8, 14, .5, 0, 1, .1, .5, 1, .1], oct=8, tau1=2.2, tau2=.9)
add('FmClav', K, 76, 1, [1, 3.2, 3, 1.5, .4, 1, 0, .7, 2, 0], oct=8, tau1=.35, tau2=.06)
add('FmOrgan', K, 77, 1, [2, .5, 3, .35, .1, 1, .25, 0, 1, .8], oct=8, tau1=.2, tau2=1.5, att=.004, sus=1)
add('FmOrgan2', K, 77, 1, [3, .7, 5, .3, 0, 1, .4, 0, 1, .9], oct=4, tau1=.2, tau2=1.5, att=.006, sus=1)
add('FmMarim', FB, 23, 1, [3.99, 2.2, 1, .2, 0, 1, 0, .7, 0, 0], oct=8, tau1=.5, tau2=.05)
add('FmVibe', FB, 27, 1, [4.0, 1.6, 1, .1, 0, 1, 0, .6, 0, 0], oct=8, tau1=1.8, tau2=.3)
add('FmBell', FB, 27, 1, [3.5, 4.5, 1.4, 2.0, 0, 1, 0, .5, 0, 0], oct=8, tau1=3.0, tau2=1.2)
add('FmChime', FB, 27, 1, [5.04, 3.5, 7.1, 1.6, 0, 1, 0, .6, 1, 0], oct=16, tau1=2.6, tau2=.5)
add('FmGlass', FB, 27, 1, [2.01, 3.0, 4.2, 1.2, 0, 1, 0, .3, 0, .2], oct=16, tau1=2.0, tau2=1.5)
add('FmPluck', FB, 23, 1, [2, 3.5, 3, 2.2, .5, 1, 0, .8, 2, 0], oct=8, tau1=.5, tau2=.08)
add('FmHarp', FB, 23, 1, [4, 2.0, 2, 1.0, 0, 1, 0, .6, 0, 0], oct=8, tau1=1.2, tau2=.2)
add('FmKalim', FB, 23, 1, [5.4, 1.4, 1, .1, 0, 1, 0, .6, 0, 0], oct=16, tau1=.9, tau2=.12)
add('FmSlap', BS, 74, 1, [1, 4.5, 2, 1.0, .35, 1, .3, .7, 0, 0], oct=1, tau1=.35, tau2=.07)
add('FmSub', BS, 74, 1, [1, 2.0, 1, .3, .1, 1, .6, .4, 0, .2], oct=1, tau1=.5, tau2=.3, att=.004, sus=1)
add('FmGrowl', BS, 79, 1, [2, 3.0, 3, 2.5, .8, 1, .3, .3, 0, .6], oct=.5, tau1=.4, tau2=1.0, att=.005, sus=1)
add('FmWobble', BS, 79, 1, [1, 5.0, 1.5, 1.0, .5, 1, .4, .2, 0, .3], oct=.5, tau1=.5, tau2=.6, att=.005, sus=1)
add('FmReedBs', BS, 74, 1, [2, 1.6, 3, 2.4, .2, 1, .2, .5, 2, .3], oct=1, tau1=.4, tau2=.25, att=.003, sus=1)
add('FmBrass', FL, 73, 1, [1, 2.5, 1, 1.0, .15, 1, 0, .3, 0, .5], oct=4, tau1=.4, tau2=.8, att=.06, sus=1)
add('FmLead', FL, 73, 1, [1, 2.2, 2, .8, .55, 1, .1, .4, 0, .6], oct=8, tau1=.3, tau2=.5, att=.01, sus=1)
add('FmSaw', FL, 73, 1, [1, 2.8, 1, 0, .9, 1, 0, .3, 0, 1.0], oct=8, tau1=.3, tau2=1.0, att=.008, sus=1)
add('FmPad', FL, 25, 1, [1.001, 1.2, 2, .6, 0, 1, .2, 0, 1, .7], oct=4, tau1=.8, tau2=2.0, att=.6, sus=1)
add('FmChoir', FL, 78, 1, [3, .6, 5, .25, .1, 1, .1, 0, 1, .8], oct=4, tau1=.7, tau2=2.0, att=.35, sus=1)
add('FmTom', FX, 13, 1, [1.5, 3.0, 2.3, 1.0, .1, 1, 0, .6, 0, 0], f0=110, tau1=.35, tau2=.04)
add('FmZap', FX, 17, 1, [2.7, 6.0, 1.1, 2.0, .3, 1, 0, .2, 0, 0], f0=900, tau1=.22, tau2=.06)
add('FmMetal', FX, 16, 1, [1.4142, 5.0, 2.76, 3.5, .5, 1, 0, .4, 2, 0], f0=520, tau1=.45, tau2=.2)
add('FmHat', FX, 8, 1, [1.4142, 6.0, 2.31, 5.0, .8, 1, 0, .2, 2, 0], f0=1500, tau1=.05, tau2=.02)
add('FmClick', FX, 7, 1, [7.3, 4.0, 1, 1, 0, 1, 0, .5, 0, 0], f0=1800, tau1=.04, tau2=.01)
# ============ VOX: p = vowelA, vowelB, -, glot, vibD, vibR, breath, bw, tilt, gender   (tau2 = morph time)
V = 'VOCAL'
add('ChoirAh', V, 78, 2, [0, 0, 0, .1, .006, 5.2, .06, 1, .5, 1.0], oct=4, tau1=.5, tau2=9, att=.35, sus=1)
add('ChoirOh', V, 78, 2, [3, 3, 0, .1, .006, 5, .06, 1, .5, 1.0], oct=4, tau1=.5, tau2=9, att=.35, sus=1)
add('ChoirEe', V, 78, 2, [2, 2, 0, .15, .006, 5.5, .05, 1, .6, 1.0], oct=4, tau1=.5, tau2=9, att=.3, sus=1)
add('ChoirOo', V, 78, 2, [4, 4, 0, .05, .007, 4.8, .08, 1, .4, 1.0], oct=4, tau1=.6, tau2=9, att=.45, sus=1)
add('MaleAh', V, 25, 2, [0, 0, 0, .2, .008, 5, .04, .9, .6, .85], oct=2, tau1=.4, tau2=9, att=.12, sus=1)
add('FemaleAh', V, 25, 2, [0, 0, 0, .1, .007, 5.6, .05, 1, .5, 1.22], oct=8, tau1=.4, tau2=9, att=.12, sus=1)
add('VoxWow', V, 26, 2, [0, 4, 0, .3, .004, 5, .02, 1, .6, 1.0], oct=4, tau1=.25, tau2=.5, att=.03, sus=1)
add('VoxYoi', V, 26, 2, [2, 3, 0, .4, .005, 5, .02, .9, .7, 1.0], oct=4, tau1=.25, tau2=.3, att=.02, sus=1)
add('Talkbox', V, 26, 2, [1, 0, 0, .6, 0, 5, .01, .7, .8, 1.0], oct=2, tau1=.2, tau2=.18, att=.01, sus=1)
add('Robot', V, 26, 2, [0, 0, 0, 1.0, 0, 5, 0, .6, .7, 1.0], oct=2, tau1=.2, tau2=9, att=.005, sus=1)
add('Whisper', V, 25, 2, [1, 3, 0, 0, 0, 5, 1.0, 1.2, .5, 1.0], oct=4, tau1=.5, tau2=2.5, att=.25, sus=1)
add('GhostCh', V, 25, 2, [4, 0, 0, .05, .01, 4, .55, 1.1, .3, 1.1], oct=4, tau1=1.2, tau2=3.5, att=.9, sus=1)
add('Gospel', V, 78, 2, [3, 0, 0, .15, .012, 5.8, .05, 1, .6, 1.0], oct=2, tau1=.8, tau2=1.5, att=.3, sus=1)
add('VoxBass', V, 74, 2, [3, 0, 0, .35, .004, 5, .02, .8, .8, .7], oct=1, tau1=.3, tau2=.6, att=.02, sus=1)
# ============ CHIP: p = duty, arp, arpRate, vibD, vibDelay, bits, noise, slide(semi/s), dutySweep, hold
C1 = 'CHIP LEADS'; C2 = 'CHIP ARPS'; C3 = 'CHIP DRUMS'
add('Pulse12', C1, 26, 3, [.125, 0, 0, .004, .2, 0, 0, 0, 0, 1], oct=8, tau1=.08, att=.002, sus=1)
add('Pulse25', C1, 26, 3, [.25, 0, 0, .004, .25, 0, 0, 0, 0, 1], oct=8, tau1=.08, att=.002, sus=1)
add('Pulse50', C1, 26, 3, [.5, 0, 0, .012, .15, 5, 0, 0, 0, 2], oct=8, tau1=.08, att=.002, sus=1)
add('PwmSweep', C1, 26, 3, [.5, 0, 0, .003, .3, 0, 0, 0, -.35, 1], oct=4, tau1=.1, att=.004, sus=1)
add('Crunch4', C1, 26, 3, [.25, 0, 0, .004, .2, 4, 0, 0, 0, 2], oct=8, tau1=.08, att=.002, sus=1)
add('ChipBass', C1, 74, 3, [.5, 0, 0, 0, 0, 4, 0, 0, 0, 1], oct=1, tau1=.1, att=.002, sus=1)
add('ChipBs12', C1, 74, 3, [.125, 0, 0, 0, 0, 3, 0, 0, 0, 1], oct=2, tau1=.1, att=.002, sus=1)
add('ChipSiren', C1, 26, 3, [.25, 0, 0, .12, 0, 4, 0, 0, 0, 1], oct=16, tau1=.1, att=.01, sus=1)
add('ArpMaj', C2, 23, 3, [.25, 1, 18, .003, .3, 0, 0, 0, 0, 1], oct=8, tau1=.1, att=.002, sus=1)
add('ArpMin', C2, 23, 3, [.125, 2, 12, .005, .3, 3, 0, 0, 0, 1], oct=4, tau1=.1, att=.002, sus=1)
add('ArpOct', C2, 23, 3, [.5, 3, 24, 0, 0, 0, 0, 0, 0, 1], oct=4, tau1=.1, att=.002, sus=1)
add('ArpPower', C2, 23, 3, [.125, 4, 20, 0, 0, 4, 0, 0, 0, 1], oct=4, tau1=.1, att=.002, sus=1)
add('ArpFifth', C2, 23, 3, [.5, 6, 14, .003, .2, 0, 0, 0, 0, 1], oct=8, tau1=.1, att=.002, sus=1)
add('Coin', C2, 23, 3, [.5, 6, 30, 0, 0, 0, 0, 0, 0, 1], oct=16, tau1=.16, att=.001, sus=0)
add('PowerUp', C2, 17, 3, [.25, 1, 28, 0, 0, 0, 0, 12, 0, 1], oct=8, tau1=.5, att=.001, sus=0)
add('ChipLaser', C3, 17, 3, [.25, 0, 0, 0, 0, 0, 0, -40, 0, 1], oct=32, tau1=.25, att=.001, sus=0)
add('NoiseHat', C3, 8, 3, [0, 0, 0, 0, 0, 0, 1, 0, 0, 1], f0=9000, tau1=.04, att=.001)
add('NoiseOpen', C3, 9, 3, [0, 0, 0, 0, 0, 0, 1, 0, 0, 1], f0=7000, tau1=.25, att=.001)
add('NoiseSnr', C3, 6, 3, [0, 0, 0, 0, 0, 0, 1, -6, 0, 1], f0=3500, tau1=.14, att=.001)
add('NoiseMet', C3, 7, 3, [0, 0, 0, 0, 0, 0, 2, 0, 0, 1], f0=4200, tau1=.12, att=.001)
add('ChipKick', C3, 0, 3, [.5, 0, 0, 0, 0, 4, 0, -90, 0, 1], f0=110, tau1=.14, att=.001)
add('ChipTom', C3, 13, 3, [.5, 0, 0, 0, 0, 4, 0, -40, 0, 1], f0=200, tau1=.18, att=.001)
# ============ MORPH: p = tableA, tableB, m0, m1, lfoRate, lfoDepth, unison, cutMul, fenv, sub    (tau2 = morph time)
MB = 'MORPH BASS'; ML = 'MORPH LEAD'; MP = 'MORPH PAD'; MK = 'MORPH KEYS'
def mo(name, grp, ub, i, m0, m1, lr, ld, un, cm, fe, sub, **kw):
    a = (i * 37 + 11) % 300; b = 300 + (i * 53 + 17) % 330
    add(name, grp, ub, 4, [a, b, m0, m1, lr, ld, un, cm, fe, sub], **kw)
mo('MBassDp', MB, 74, 1, .1, .9, .2, .15, 0, 6, 1.5, .4, oct=1, tau1=.4, tau2=.8, att=.005, sus=1)
mo('MBassGr', MB, 74, 2, .8, .2, .4, .2, .01, 8, 2.0, .3, oct=1, tau1=.3, tau2=.4, att=.004, sus=1)
mo('MBassWb', MB, 79, 3, .2, .8, 2.5, .35, 0, 5, 1.2, .3, oct=1, tau1=.3, tau2=.9, att=.004, sus=1)
mo('MBassCl', MB, 74, 4, .5, .5, 0, 0, 0, 10, 0, .5, oct=.5, tau1=.5, tau2=1.0, att=.003, sus=1)
mo('MBassSn', MB, 74, 5, .0, 1.0, 0, 0, 0, 7, 1.0, .2, oct=1, tau1=.35, tau2=.15, att=.002, sus=1)
mo('MBassSb', MB, 19, 6, .3, .6, .3, .1, 0, 3, 0, .8, oct=.5, tau1=.5, tau2=1.2, att=.006, sus=1)
mo('MBassRs', MB, 22, 7, .2, .7, .3, .1, .018, 6, 1.0, .4, oct=1, tau1=.4, tau2=1.0, att=.01, sus=1)
mo('MLeadBr', ML, 73, 8, .7, .2, 0, 0, .006, 7, 1.4, 0, oct=8, tau1=.3, tau2=.5, att=.01, sus=1)
mo('MLeadSw', ML, 26, 9, .1, .9, .5, .3, .008, 9, 0, 0, oct=8, tau1=.3, tau2=2.0, att=.006, sus=1)
mo('MLeadGl', ML, 26, 10, .6, .6, 5, .25, .004, 12, 0, 0, oct=8, tau1=.3, tau2=1.0, att=.004, sus=1)
mo('MLeadSt', ML, 26, 11, .0, .5, 0, 0, .012, 8, 1.0, 0, oct=4, tau1=.3, tau2=.4, att=.003, sus=1)
mo('MLeadAr', ML, 26, 12, .4, .4, 0, 0, 0, 14, 0, 0, oct=8, tau1=.2, tau2=1.0, att=.002, sus=1)
mo('MLeadHm', ML, 26, 13, .9, .1, .2, .2, .02, 6, 2.0, 0, oct=8, tau1=.4, tau2=.7, att=.01, sus=1)
mo('MLeadDk', ML, 26, 14, .2, .45, .15, .1, .003, 5, 1.0, .2, oct=4, tau1=.4, tau2=1.2, att=.008, sus=1)
mo('MLeadAc', ML, 26, 15, .0, .8, 0, 0, 0, 5, 3.0, 0, oct=4, tau1=.2, tau2=.18, att=.002, sus=1)
mo('MPadDrk', MP, 25, 16, .1, .9, .08, .25, .012, 3, .5, .2, oct=2, tau1=1.0, tau2=3.0, att=.8, sus=1)
mo('MPadAir', MP, 25, 17, .3, .8, .12, .3, .014, 7, .6, 0, oct=4, tau1=1.2, tau2=4.0, att=1.0, sus=1)
mo('MPadGls', MP, 25, 18, .9, .2, .2, .2, .01, 12, 0, 0, oct=8, tau1=1.0, tau2=3.5, att=.6, sus=1)
mo('MPadSwl', MP, 25, 19, 0, 1, .25, .4, .016, 5, .8, .1, oct=4, tau1=1.0, tau2=5.0, att=.5, sus=1)
mo('MPadWrm', MP, 25, 20, .4, .6, .1, .1, .008, 4, .4, .3, oct=2, tau1=1.0, tau2=3.0, att=.4, sus=1)
mo('MPadEvo', MP, 25, 21, .0, 1.0, .05, .3, .018, 6, 1.0, 0, oct=2, tau1=1.5, tau2=8.0, att=1.2, sus=1)
mo('MPadOrg', MP, 25, 22, .5, .5, .3, .15, .005, 8, 0, .3, oct=4, tau1=.8, tau2=2.0, att=.3, sus=1)
mo('MPadDst', MP, 25, 23, .7, .3, .4, .4, .02, 9, .7, 0, oct=2, tau1=1.0, tau2=3.0, att=.7, sus=1)
mo('MKeyPlk', MK, 23, 24, .8, .1, 0, 0, .004, 8, 3.0, 0, oct=8, tau1=.5, tau2=.12, att=.001)
mo('MKeyStb', MK, 24, 25, .7, .2, 0, 0, .01, 9, 2.0, 0, oct=8, tau1=.4, tau2=.2, att=.001)
mo('MKeyBel', MK, 27, 26, .1, .9, 0, 0, .002, 14, 0, 0, oct=16, tau1=1.5, tau2=.6, att=.001)
mo('MKeyWrm', MK, 76, 27, .3, .3, 4, .1, .003, 7, 1.0, .2, oct=8, tau1=1.2, tau2=.5, att=.002)
mo('MKeyArp', MK, 23, 28, .0, .6, 0, 0, .006, 10, 2.5, 0, oct=8, tau1=.18, tau2=.08, att=.001)
mo('MKeyTr', MK, 24, 29, .5, .0, 0, 0, .02, 8, 1.5, 0, oct=4, tau1=.45, tau2=.3, att=.001)
mo('MKeyGlw', MK, 27, 30, .0, 1.0, 0, 0, .012, 11, 0, 0, oct=16, tau1=1.0, tau2=.5, att=.001)
mo('MKeyOrg', MK, 77, 31, .4, .6, 3, .2, .005, 9, 0, .5, oct=4, tau1=.6, tau2=1.0, att=.003)
# ---------------------------------------------------------------------------------------------------------
# ============ INIT: one plain starting sound per engine (appended last: older sound numbers never move)
IN = 'INIT'
add('ModINIT', IN, 23, 0, [0, .6, 3, .9, 0, .3], oct=8, tau1=.45)
add('FMINIT', IN, 73, 1, [1, 1.0, 1, 0, 0, 1, 0, .3, 0, 1.0], oct=8, tau1=.3, tau2=1.0, att=.005, sus=1)
add('VoxINIT', IN, 78, 2, [0, 0, 0, .1, .006, 5.2, .06, 1, .5, 1.0], oct=4, tau1=.5, tau2=9, att=.05, sus=1)
add('ChpINIT', IN, 26, 3, [.5, 0, 0, 0, 0, 0, 0, 0, 0, 1], oct=8, tau1=.08, att=.002, sus=1)
add('MrpINIT', IN, 26, 4, [3, 305, 0, 0, 0, 0, 0, 8, 0, 0], oct=8, tau1=.3, tau2=1.0, att=.005, sus=1)

# ============ ENGINE PARAMETER SPEC: which of the 10 values p[0..9] a knob edits, its range and how it is shown.
# The pad parameters e0..e9 hold 0..1 (or -1 = "use the preset's own value"); the DSP maps them with lo/hi and rounds the int / enum ones.
def F(s, n, a, lo, hi, d=2, u=''): return dict(s=s, n=n, a=a, lo=lo, hi=hi, k='f', d=d, u=u)
def I(s, n, a, lo, hi, u=''): return dict(s=s, n=n, a=a, lo=lo, hi=hi, k='i', d=0, u=u)
def E(s, n, a, names, lo=0): return dict(s=s, n=n, a=a, lo=lo, hi=lo + len(names) - 1, k='e', d=0, u='', e=names)
SETS = ['MARIMB', 'VIBES', 'XYLO', 'KALIMB', 'GLOCK', 'TUBULR', 'CHURCH', 'STEELD', 'HARP', 'NYLON', 'KOTO', 'TIMP', 'MEMBRN', 'WOOD', 'BOWL', 'GAMLAN', 'CRYSTL', 'PLATE', 'HOLLOW']     # at most 6 letters: the value cell is 30 px wide
ENG = [
    dict(n='MODAL', att=False, p=[E(0, 'Set', 'Set', SETS), F(1, 'Bright', 'Brt', 0, 1), F(2, 'BurstMs', 'Burst', 0.5, 20, 1), F(3, 'Tilt', 'Tilt', 0, 2),
                                  F(4, 'Stretch', 'Strc', 0, 0.1, 3), F(5, 'Click', 'Click', 0, 1), F(6, 'TremDepth', 'TrmD', 0, 1), F(7, 'TremHz', 'TrmR', 0, 12, 1)]),
    dict(n='FM', att=True, p=[F(0, 'RatioB', 'RatB', 0.25, 16), F(1, 'IndexB', 'IdxB', 0, 8), F(2, 'RatioC', 'RatC', 0.25, 16), F(3, 'IndexC', 'IdxC', 0, 8),
                              F(4, 'Feedback', 'Fdbk', 0, 1), E(8, 'Algo', 'Algo', ['C>B>A', 'B+C>A', 'PAIRS']), F(6, 'Sub', 'Sub', 0, 1), F(7, 'VelSens', 'Vel', 0, 1),
                              F(5, 'RatioA', 'RatA', 0.25, 8), F(9, 'IdxEnv', 'IEnv', 0, 1)]),
    dict(n='VOX', att=True, p=[E(0, 'VowelA', 'VowA', ['AH', 'EH', 'EE', 'OH', 'OO', 'AE', 'UH']), E(1, 'VowelB', 'VowB', ['AH', 'EH', 'EE', 'OH', 'OO', 'AE', 'UH']),
                               F(3, 'Voice', 'Voice', 0, 1), F(6, 'Breath', 'Brth', 0, 1), F(4, 'VibDepth', 'VibD', 0, 0.02, 3), F(5, 'VibHz', 'VibR', 3, 8, 1),
                               F(7, 'Damp', 'Damp', 0.4, 1.6), F(8, 'Tilt', 'Tilt', 0, 1), F(9, 'Gender', 'Gndr', 0.6, 1.5)]),
    dict(n='CHIP', att=True, p=[F(0, 'Duty', 'Duty', 0, 0.5), E(6, 'Noise', 'Noise', ['OFF', 'LONG', 'SHORT']), E(1, 'Arp', 'Arp', ['OFF', 'MAJ', 'MIN', 'OCT', '5TH', 'MAJ B', 'PWR']),
                                F(2, 'ArpRate', 'ArpR', 0, 32, 0), F(3, 'VibDepth', 'VibD', 0, 0.15, 3), F(4, 'VibDelay', 'VibT', 0, 0.5, 2),
                                E(5, 'Bits', 'Bits', ['OFF', '1', '2', '3', '4', '5', '6', '7', '8']), F(7, 'Slide', 'Slide', -96, 12, 0),
                                F(8, 'DutySweep', 'Swp', -0.5, 0.5), I(9, 'Hold', 'Hold', 1, 8)]),
    dict(n='MORPH', att=True, p=[I(0, 'WaveA', 'WavA', 0, 639), I(1, 'WaveB', 'WavB', 0, 639), F(2, 'MorphStart', 'Mrp0', 0, 1), F(3, 'MorphEnd', 'Mrp1', 0, 1),
                                 F(4, 'LfoHz', 'LfoR', 0, 8, 2), F(5, 'LfoDepth', 'LfoD', 0, 0.5), F(6, 'Unison', 'Uni', 0, 0.03, 3), F(7, 'FiltRatio', 'Filt', 1, 16, 1),
                                 F(8, 'FiltEnv', 'FEnv', 0, 4), F(9, 'Sub', 'Sub', 0, 1)]),
]
for e, g in enumerate(ENG):                                        # every preset value must lie inside the range of its knob (otherwise editing would jump)
    used = {q['s']: q for q in g['p']}
    for x in P:
        if x[3] != e: continue
        for i in range(10):
            if i in used: assert used[i]['lo'] - 1e-6 <= x[10][i] <= used[i]['hi'] + 1e-6, (g['n'], x[0], i, x[10][i], used[i]['lo'], used[i]['hi'])
            else: assert x[10][i] == 0.0, (g['n'], x[0], i, x[10][i])
    assert len(used) == len(g['p']) and max(len(g['p']), 1) <= 10
    assert all(q['hi'] - q['lo'] < 1000 for q in g['p'])             # 3 decimals in the 0..1 values must still resolve every whole number
names = [x[0] for x in P]; assert len(set(names)) == len(names), [n for n in names if names.count(n) > 1]
trimf = root + 'tools/nx_trim.json'
trim = json.load(open(trimf)) if os.path.exists(trimf) else {}
u = open(root + 'ui.js').read()
m = re.search(r"const TYPE_NAME = \[(.*?)\];", u); old = [x.strip().strip("'") for x in m.group(1).split(',')]
old = old[:1064]; assert len(old) == 1064 and old[-1] == 'LaserX8', old[-2:]
assert not (set(names) & set(old)), set(names) & set(old)
def f(x): return repr(float(x)) + 'f'
open(root + 'src/variants4_n.h', 'w').write('#define N_V4 %d\n' % len(P))
open(root + 'src/variants4_names.inc', 'w').write(',\n    '.join(','.join('"%s"' % n for n in names[i:i + 6]) for i in range(0, len(names), 6)) + '\n')
rows = []
for (n, g, ub, eng, f0, oc, t1, t2, at, sus, p) in P:
    rows.append('    { %d, %s, %s, %s, %s, %s, %s, %d, { %s } },  /* %s */' % (eng, f(f0), f(oc), f(t1), f(t2), f(at), f(trim.get(n, 1.0)), sus, ', '.join(f(x) for x in p), n))
open(root + 'src/variants4.inc', 'w').write('\n'.join(rows) + '\n')
u = u.replace(m.group(0), "const TYPE_NAME = [" + ",".join("'%s'" % x for x in old + names) + "];", 1)
m = re.search(r"const BASE_T = \[(.*?)\];", u); ob = m.group(1).split(',')[:1064]; assert len(ob) == 1064, len(ob)
u = u.replace(m.group(0), "const BASE_T = [" + ",".join(ob + [str(x[2]) for x in P]) + "];", 1)
groups = {}
for i, x in enumerate(P):
    if x[1] != IN: groups.setdefault(x[1], []).append(1064 + i)      # INIT sounds are not part of the folders of the drum browser
blk = "/*NXBEGIN*/const NX_GROUPS = " + json.dumps([[g, l] for g, l in groups.items()], separators=(',', ':')) + ";/*NXEND*/"
if '/*NXBEGIN*/' in u: u = re.sub(r"/\*NXBEGIN\*/.*?/\*NXEND\*/", lambda _: blk, u, flags=re.S)
else: u = u.replace("const SYN_ORDER =", blk + "\nconst SYN_ORDER =", 1)
# ---- engines: spec for the UI (ui.js) and value ranges for the DSP (src/engines.inc)
lists = []                                                           # per engine: [[folder, [sound numbers]], ...], INIT first
for e, g in enumerate(ENG):
    fo = {}
    for i, x in enumerate(P):
        if x[3] == e: fo.setdefault(x[1], []).append(1064 + i)
    lists.append([[IN, fo.pop(IN)]] + [[k, v] for k, v in fo.items()])
def num(x): return int(x) if float(x).is_integer() else round(float(x), 5)
spec = [dict(n=g['n'], att=g['att'], p=[dict(s=q['s'], n=q['n'], a=q['a'], lo=num(q['lo']), hi=num(q['hi']), k=q['k'], d=q['d'], u=q['u'], **({'e': q['e']} if 'e' in q else {})) for q in g['p']], l=lists[e]) for e, g in enumerate(ENG)]
eblk = ("/*ENGBEGIN*/const ENG = " + json.dumps(spec, separators=(',', ':')) + ";const NX_EID = " + json.dumps([x[3] for x in P], separators=(',', ':')) +
        ";const NXP = " + json.dumps([[num(v) for v in x[10]] for x in P], separators=(',', ':')) + ";/*ENGEND*/")
if '/*ENGBEGIN*/' in u: u = re.sub(r"/\*ENGBEGIN\*/.*?/\*ENGEND\*/", lambda _: eblk, u, flags=re.S)
else: u = u.replace("const SYN_ORDER =", eblk + "\nconst SYN_ORDER =", 1)
open(root + 'ui.js', 'w').write(u)
lo = [[0.0] * 10 for _ in ENG]; hi = [[0.0] * 10 for _ in ENG]; it = [[0] * 10 for _ in ENG]
for e, g in enumerate(ENG):
    for q in g['p']: lo[e][q['s']] = q['lo']; hi[e][q['s']] = q['hi']; it[e][q['s']] = 0 if q['k'] == 'f' else 1
def tbl(t, fm): return '{\n' + ',\n'.join('    { ' + ', '.join(fm(v) for v in r) + ' }' for r in t) + '\n}'
open(root + 'src/engines.inc', 'w').write(
    '/* generated by tools/gen_variants4.py - do not edit. Range of the 10 parameters of every NEW ENGINE (0 MODAL, 1 FM, 2 VOX, 3 CHIP, 4 MORPH):\n'
    '   the pad parameters e0..e9 hold 0..1 and map to lo..hi (rounded when int = 1). Unused slots: 0..0. */\n'
    'static const float ENG_LO[5][10] = ' + tbl(lo, f) + ';\n'
    'static const float ENG_HI[5][10] = ' + tbl(hi, f) + ';\n'
    'static const unsigned char ENG_INT[5][10] = ' + tbl(it, str) + ';\n')
print(len(P), 'new presets, total', 1064 + len(P))
