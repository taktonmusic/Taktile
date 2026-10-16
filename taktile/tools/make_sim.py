#!/usr/bin/env python3
"""Builds the web simulator: the current ui.js + module.json version inside tools/sim_template.html.
Usage: python3 tools/make_sim.py [out.html]   (default /tmp/sim/taktile-sim.html)"""
import json, sys, os
here = os.path.dirname(os.path.abspath(__file__)); root = os.path.dirname(here)
out = sys.argv[1] if len(sys.argv) > 1 else '/tmp/sim/taktile-sim.html'
ui = open(os.path.join(root, 'ui.js'), encoding='utf8').read()
ver = json.load(open(os.path.join(root, 'module.json')))['version']
glyphs = json.dumps(json.load(open(os.path.join(here, 'sim_glyphs.json'))), separators=(',', ':'))
tpl = open(os.path.join(here, 'sim_template.html'), encoding='utf8').read()
ui = ui.replace('</script', '<\\/script').replace('<!--', '<\\!--')
html = tpl.replace('%%VERSION%%', ver).replace('%%GLYPHS%%', glyphs).replace('%%UIJS%%', ui)
os.makedirs(os.path.dirname(out), exist_ok=True)
open(out, 'w', encoding='utf8').write(html)
print('written', out, len(html), 'bytes, version', ver)
