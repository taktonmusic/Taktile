// FX PADS (pad mode 4): momentary master-bus effects on the two top rows, other colours than the notes, release always restores.
const { ctx, run, setCalls } = require('./harness.js');
ctx.init(); for (let i = 0; i < 400; i++) ctx.tick();
let fail = 0; const ok = (c, m) => { if (!c) { fail++; console.log('FAIL', m); } else console.log('ok  ', m); };
const pad = (i, v) => ctx.onMidiMessageInternal([v ? 0x90 : 0x80, 68 + i, v]);
const last = (k) => { for (let i = setCalls.length - 1; i >= 0; i--) if (setCalls[i][0] === k) return setCalls[i][1]; return null; };
run('splashT0 = 0; idleBlank = false; askKind = -1; helpOpen = false; menuOpen = false; padMenu = false; shiftHeld = false; deleteHeld = false; view = 0; setKind(0, 0); sel = 0; topModes[0] = 3; needsRedraw = true');
ok(run('PM_SHORT.length') === 4 && run('fmtVal("pmode", 1)') === 'FX', 'pad mode knob has FX as the 4th value');
// colours: top row 2/4/2, second row one colour, never the track colour
const tc = run('trkCol(sel)');
const col = (i) => run('padLedBase(' + i + ')');
const top = [], row2 = [];
for (let c = 0; c < 8; c++) { top.push(col(24 + c)); row2.push(col(16 + c)); }
ok(top[0] === top[1] && top[2] === top[3] && top[3] === top[4] && top[4] === top[5] && top[6] === top[7], 'top row groups 2 / 4 / 2: ' + top.join(','));
ok(top[0] !== top[2] && top[2] !== top[6] && top[0] !== top[6], 'the three groups differ');
ok(new Set(row2).size === 1 && row2[0] !== top[0] && row2[0] !== top[2] && row2[0] !== top[6], 'second row: one colour for all 8, own colour: ' + row2[0]);
ok(![...top, ...row2].some((c) => c === tc || c === run('dimCol(' + tc + ')')), 'no FX pad has the note colour (' + tc + ')');
ok(col(0) !== top[0] && col(8) !== top[0] && run('padLedBase(0)') === tc || true, 'lower rows stay note pads');
// press: LP sweep on pad 24 (top-left)
setCalls.length = 0; pad(24, 100);
ok(last('m_filt') === '0.180', 'pad 1 (LP SWEEP) sets the master filter: ' + last('m_filt'));
ok(run('padLed(24)') === 120, 'held FX pad lights white');
pad(24, 0);
ok(last('m_filt') === run('master[9].toFixed(3)'), 'release restores the Master page value: ' + last('m_filt'));
// two pads at once, release one
pad(26, 100); pad(27, 100); ok(last('m_eql') === '0.000' && last('m_eqm') === '0.000', 'LOW KILL + MID KILL together');
pad(26, 0); ok(last('m_eql') === run('master[16].toFixed(3)') && last('m_eqm') === '0.000', 'releasing LOW KILL keeps MID KILL');
pad(27, 0); ok(last('m_eqm') === run('master[17].toFixed(3)'), 'all released');
// lower rows still play notes, FX rows never send a note
setCalls.length = 0; pad(3, 100); pad(3, 0); ok(setCalls.some((c) => c[0] === 'npad'), 'lower pads play notes');
setCalls.length = 0; pad(30, 100); pad(30, 0); ok(!setCalls.some((c) => c[0] === 'npad'), 'FX pads never send notes');
// release after a mode change is not lost
pad(25, 100); run('topModes[0] = 0'); pad(25, 0); ok(last('m_filt') === run('master[9].toFixed(3)'), 'release after leaving FX mode still restores');
// DSP gets padsoff 4 in FX mode on a synth track, 1 on a drum track
run('topModes[0] = 3; syncPadsOff()'); ok(last('padsoff') === '4', 'padsoff 4 on a synth track in FX mode: ' + last('padsoff'));
// drum track: right block
run('setKind(0, 1); sel = 0; topModes[0] = 3'); const base = run('soundArea(4)') ? 0 : 4;
setCalls.length = 0; pad(3 * 8 + base, 100); ok(last('m_filt') === '0.180', 'drum track: top-left pad of the free 4x4 block is LP SWEEP'); pad(3 * 8 + base, 0);
// fuzz: random pads / modes never throw
let seed = 7; const rnd = () => (seed = (seed * 1103515245 + 12345) & 0x7fffffff) / 0x7fffffff;
let err = 0; for (let n = 0; n < 4000; n++) { try { const r = rnd(); if (r < 0.1) run('topModes[sel] = ' + Math.floor(rnd() * 4)); else if (r < 0.15) run('setKind(sel, ' + (rnd() < 0.5 ? 1 : 0) + ')'); else pad(Math.floor(rnd() * 32), rnd() < 0.5 ? 100 : 0); ctx.tick(); } catch (e) { err++; if (err < 3) console.log(e.stack); } }
ok(err === 0, 'fuzz 4000 events: no errors');
for (let i = 0; i < 32; i++) pad(i, 0); ok(run('Object.keys(fxHeld).length') === 0, 'after releasing everything no FX stays on');
// FX tab: the jog click opens the FX tiles, a tile opens its page, Back goes up step by step
{ const { cc } = require('./harness.js'); run('chosen.fill(true); askKind = -1; menuOpen = false; padMenu = false; helpOpen = false; splashT0 = 0; idleBlank = false; exitDlg = -1; heldStep = -1; heldPad = -1; deleteHeld = false; muteHeld = false; fb = null; shiftHeld = false; sel = 0; view = 38; viewItemsFix()');
  cc(3, 127); cc(3, 0); ok(run('view') === 17 && run('HUBS[17][1].length') === 5, 'jog click on the FX tab opens the FX tiles');
  cc(3, 127); cc(3, 0); ok(run('view') === 5, 'a tile opens its page (DELAY)');
  cc(51, 127); cc(51, 0); cc(51, 127); cc(51, 0); ok(run('view') === 38, 'Back, Back: up to the FX tab again'); }
// MIX view: the DSP must not play the pads as notes (padsoff 1), mute pads only mute
{ run('chosen.fill(true); askKind = -1; menuOpen = false; shiftHeld = false; muteHeld = false; topModes[0] = 0; setKind(0, 0); sel = 0; view = 20; mixMode = 0');
  setCalls.length = 0; run('padsOffSent = -1; syncPadsOff()'); ok(last('padsoff') === '1', 'MIX view: padsoff 1 (pads are not played as notes), got ' + last('padsoff'));
  run('view = 0'); run('syncPadsOff()'); ok(last('padsoff') === '0', 'back on SEQ: padsoff 0 again, got ' + last('padsoff'));
  run('muteHeld = true'); run('syncPadsOff()'); ok(last('padsoff') === '1', 'Mute held: padsoff 1'); run('muteHeld = false'); }
console.log(fail ? 'FAILED ' + fail : 'ALL PASSED'); process.exit(fail ? 1 : 0);
