// UI test of the engine pages: SOUND (Engine + Preset), SYNTH (named values, pages), editing, browser, tab order, kits, fuzz.
const { ctx, run, cc, writes, setCalls } = require('./harness.js');
ctx.init(); for (let i = 0; i < 400; i++) ctx.tick();
const SHIFT = 49, LEFT = 62, RIGHT = 63, JOGCLICK = 3;
let fail = 0; const ok = (c, m) => { if (!c) { fail++; console.log('FAIL', m); } else console.log('ok  ', m); };
const key = (n) => { cc(n, 127); cc(n, 0); };
const shift = (f) => { cc(SHIFT, 127); f(); cc(SHIFT, 0); };
const setup = (v) => run('splashT0 = 0; helpOpen = false; menuOpen = false; padMenu = false; broOpen = false; shiftHeld = false; deleteHeld = false; exitDlg = -1; askKind = -1; try { fb = null; } catch (e) {} sel = 0; view = ' + v + '; viewItemsFix(); needsRedraw = true;');
const INIT = [1211, 1212, 1213, 1214, 1215];          // MODAL FM VOX CHIP MORPH
const names = ['MODAL', 'FM', 'VOX', 'CHIP', 'MORPH'];

// --- the generated tables are consistent
ok(run('ENG.length') === 5 && run('NX_EID.length') === 152 && run('NXP.length') === 152, 'tables: 5 engines, 152 NEW ENGINES sounds');
ok(run('PK.length') === 72 && run('PK[61]') === 'e0' && run('PK[70]') === 'e9' && run('defOf(61)') === -1, 'pad keys: e0..e9 at 61..70, default -1');
ok(run('TYPE_NAME.length') === 1216 && run('TYPE_NAME[1212]') === 'FMINIT', 'sound names: 1216, FMINIT is number 1212');
for (let e = 0; e < 5; e++) ok(run('engList(' + (e + 1) + ')[0]') === INIT[e] && run('engOfType(' + INIT[e] + ')') === e + 1, names[e] + ': INIT is the first sound of its engine');
ok(run('engList(0).every(t => t < 1064 && BASE_T[t] !== 359)'), 'ANALOG list: only classic sounds, no sample');
let all = 0; for (let e = 1; e <= 5; e++) all += run('engList(' + e + ').length'); ok(all === 152, 'the five engine lists hold all 152 sounds once (' + all + ')');

// --- tab order: SOUND, SYNTH, FILTER, MOD, FX, FX 2, EQ, EUCLID, MIDI
setup(1);
ok(JSON.stringify(run('trackTabs()')) === '[1,9,2,3,38,43,36,27]', 'tabs on a synth track: SOUND SYNTH FILTER MOD FX EQ EUCLID MIDI');
ok(run('lastTabId()') === 9, 'the SYNTH tab is the second tab');
run('setKind(0, 1)'); ok(JSON.stringify(run('trackTabs()')) === '[1,28,2,3,38,43,36,27]', 'drum track: SAMPLE is the second tab'); run('setKind(0, 0)');

// --- SOUND page: Engine + Preset
setup(1);
ok(run("VIEW_ITEMS[1][0][0]") === 'eng' && run("VIEW_ITEMS[1][1][0]") === 'type' && run('VIEW_ITEMS[1].length') === 8, 'SOUND (synth): Engine, Preset, Tune, Decay, Tone, Drive, Voices, Vol');
run('setKind(0, 1); viewItemsFix()'); ok(run("VIEW_ITEMS[1][0][0]") === 'type', 'SOUND (drum track): unchanged, Type first'); run('setKind(0, 0); viewItemsFix()');
run('setLaneP(0, "type", 19)'); ok(run('engLane(0)') === 0 && run('fmtVal("eng", 0)') === 'ANALOG', 'a classic sound is on ANALOG');
for (let e = 1; e <= 5; e++) {
  run('setEngine(0, ' + e + ')');
  ok(run('Math.round(pads[0][0])') === INIT[e - 1] && run('engLane(0)') === e, 'Engine knob target ' + names[e - 1] + ' -> its INIT');
  ok(run('pads[0].slice(61, 71).every(v => v === -1)'), names[e - 1] + ': values start as the preset (-1)');
  ok(run('pads[0][PI.cut]') === 1 && run('pads[0][PI.locut]') === 0, names[e - 1] + ': filter wide open');
}
run('setEngine(0, 2)'); run('setEngine(0, 0)');
ok(run('Math.round(pads[0][0])') > 0 && run('engLane(0)') === 0, 'back to ANALOG finds a classic sound');
run('setLaneP(0, "type", 77)'); run('setEngine(0, 1)'); run('setEngine(0, 0)'); ok(run('Math.round(pads[0][0])') === 77, 'ANALOG remembers the last classic sound (77)');
// Engine knob through the real handler: knob 1 is the first item of the SOUND page
setup(1); run('setEngine(0, 0)'); run('acc.fill(0)');
for (let i = 0; i < 3; i++) cc(71, 1);
ok(run('engLane(0)') === 1, 'knob 1 turned right: ANALOG -> MODAL');
for (let i = 0; i < 3; i++) cc(71, 1);
ok(run('engLane(0)') === 2, 'knob 1 turned on: MODAL -> FM');
for (let i = 0; i < 6; i++) cc(71, 127);
ok(run('engLane(0)') === 0, 'knob 1 turned left twice: back to ANALOG');
// Preset knob (knob 2) stays inside the engine
run('setEngine(0, 2)'); run('acc.fill(0)'); const fmN = run('engList(2).length');
for (let i = 0; i < 2; i++) cc(72, 1);
ok(run('Math.round(pads[0][0])') === run('engList(2)[2]') && run('engLane(0)') === 2, 'Preset knob: two ticks = two sounds further in the FM list');
run('acc.fill(0)'); shift(() => { cc(72, 1); cc(72, 1); });
ok(run('Math.round(pads[0][0])') === run('engList(2)[7]'), 'Shift + Preset knob: 5 sounds at once');
for (let i = 0; i < 400; i++) cc(72, 3);
ok(run('Math.round(pads[0][0])') === run('engList(2)[' + (fmN - 1) + ']') && run('engLane(0)') === 2, 'Preset knob: stops at the end of the FM list (' + fmN + ' sounds), never leaves the engine');
for (let i = 0; i < 400; i++) cc(72, 125);
ok(run('Math.round(pads[0][0])') === 1212, 'Preset knob: back at FMINIT');
// jog: the same, inside the engine
run('setEngine(0, 4)'); run('for (let i = 0; i < 300; i++) browseSound(1); ');
ok(run('engLane(0)') === 4 && run('Math.round(pads[0][0])') === run('engList(4)[engList(4).length - 1]'), 'jog turn stays inside the CHIP engine');

// --- SYNTH page
run('setEngine(0, 2)'); setup(9);
const sy = () => JSON.parse(run('JSON.stringify(VIEW_ITEMS[9].map(x => x[0] + ":" + x[1]))'));
ok(JSON.stringify(sy()) === JSON.stringify(['e0:RatioB', 'e1:IndexB', 'e2:RatioC', 'e3:IndexC', 'e4:Feedback', 'e8:Algo', 'e6:Sub', 'e7:VelSens']), 'FM page 1: ' + sy().join(' '));
shift(() => key(RIGHT)); ok(run('synPage') === 1 && JSON.stringify(sy()) === JSON.stringify(['e5:RatioA', 'e9:IdxEnv', 'att:Attack']), 'Shift+Right: FM page 2: ' + sy().join(' '));
shift(() => key(RIGHT)); ok(run('synPage') === 0, 'Shift+Right again: wraps to page 1');
shift(() => key(LEFT)); ok(run('synPage') === 1, 'Shift+Left: page 2'); shift(() => key(LEFT));
for (const [e, n1, n2] of [[1, 8, 0], [2, 8, 3], [3, 8, 2], [4, 8, 3], [5, 8, 3]]) {
  run('setEngine(0, ' + e + '); synPage = 0; viewItemsFix()'); const a = run('VIEW_ITEMS[9].length'); run('synPage = 1; viewItemsFix()'); const b = run('synPages(0) > 1 ? VIEW_ITEMS[9].length : 0');
  ok(a === n1 && b === n2, names[e - 1] + ': ' + a + ' knobs on page 1' + (n2 ? ', ' + b + ' on page 2' : ', one page'));
}
run('setEngine(0, 0); synPage = 0; viewItemsFix()'); ok(run('VIEW_ITEMS[9][0][0]') === 'wave', 'ANALOG: the SYNTH page is the old one (Wave first)');
run('setEngine(0, 2); synPage = 0; viewItemsFix()');

// --- editing a value
setup(9); run('setEngine(0, 2)'); run('viewItemsFix()');
ok(run('fmtVal("e0", getVal("pad", "e0"))') === '1.00' && run('fmtVal("e8", getVal("pad", "e8"))') === 'C>B>A', 'FMINIT shows its own values: RatioB 1.00, Algo C>B>A');
setCalls.length = 0; run('acc.fill(0)');
for (let i = 0; i < 20; i++) cc(72, 1);                                  // knob 2 = IndexB (e1)
for (let i = 0; i < 4; i++) ctx.tick();
const e1 = run('pads[0][62]');
ok(e1 > 0 && Math.abs(e1 - (1.0 - 0) / 8 - 20 * 0.004) < 1e-6, 'IndexB turned up: 0..1 value ' + e1.toFixed(4) + ' = preset 1.0/8 + 20 steps');
ok(run('pads[0].slice(61, 71).every(v => v >= 0)'), 'first touch wrote the preset values into all ten');
ok(Math.abs(run('pads[0][61]') - (1 - 0.25) / 15.75) < 1e-9, 'RatioB keeps the preset value (1.0 -> 0.04762)');
ok(setCalls.some(c => c[0] === 'setpad' && c[1].split(':')[1].split(',').length === 72), 'the whole row (72 values) went to the DSP once');
setCalls.length = 0; for (let i = 0; i < 5; i++) cc(72, 1); for (let i = 0; i < 4; i++) ctx.tick();
ok(setCalls.some(c => /^p01_e1$/.test(c[0]) || (c[0] === 'setpad')), 'a further turn sends the single value: ' + JSON.stringify(setCalls.slice(0, 2)).slice(0, 80));
ok(/^\d\.\d\d$/.test(run('fmtVal("e1", getVal("pad", "e1"))')), 'value text is a number: ' + run('fmtVal("e1", getVal("pad", "e1"))'));
// int / enum knob: 3 detents = one step
run('acc.fill(0)'); for (let i = 0; i < 2; i++) cc(76, 1);              // knob 6 = Algo (e8)
ok(run('fmtVal("e8", getVal("pad", "e8"))') === 'B+C>A', 'Algo knob: C>B>A -> B+C>A after two ticks');
// Delete + knob: back to the preset's own values
run('deleteHeld = true'); cc(72, 1); run('deleteHeld = false');
ok(run('pads[0].slice(61, 71).every(v => v === -1)') && run('fmtVal("e8", getVal("pad", "e8"))') === 'C>B>A', 'Delete + knob: back to the preset values (-1)');
// another sound drops the edits
cc(72, 1); for (let i = 0; i < 9; i++) cc(72, 1); run('pads[0][62] = 0.5'); run('applyType(0, engList(2)[3])');
ok(run('pads[0].slice(61, 71).every(v => v === -1)'), 'another sound: edits are gone (-1)');
// undo brings the knob back
run('acc.fill(0)'); for (let i = 0; i < 10; i++) cc(72, 1); const edited = run('pads[0][62]');
run('doUndo()'); ok(run('pads[0][62]') !== edited && run('pads[0].slice(61, 71).every(v => v === -1)'), 'Undo takes the knob turn back (to the preset values)');

// --- state: a project row saved before the engines (61 values) loads with -1
ok(run('(function () { const a = pads[0].slice(0, 61); while (a.length < PK.length) a.push(defOf(a.length)); return a.slice(61, 71).every(v => v === -1) && a.length === 72; })()'), 'an old row (61 values) is filled with -1');
ok(run('padStr(pads[0]).split(",").length') === 72, 'padStr sends 72 values');
run('setEngine(0, 2); setLaneP(0, "type", 1213); editParam("pad", "e1", 5, 0)'); run('pads[0][62] = 0.123456789');
ok(run('padStr(pads[0]).split(",")[62]') === '0.12346', 'engine values keep 5 decimals on the wire');

// --- kits
run('setEngine(0, 3); editParam("pad", "e0", 12, 0)');
run('kitsLoad(); kits[5] = { d: 0, r: [pads[0].slice()] }; kitSlot = 5; setEngine(0, 0); kitLoad()');
ok(run('engLane(0)') === 3 && run('pads[0][61]') >= 0, 'a kit slot keeps the engine and the edited values');
run('kits[5] = { d: 0, r: [pads[0].slice(0, 61)] }; pads[0][61] = 0.9; kitLoad()');
ok(run('pads[0][61]') === -1, 'a kit saved before the engines: values back to the sound\'s own (-1)');
run('kits[5] = null');

// --- browser
setup(1); run('setEngine(0, 2); fbOpen()');
ok(run('fb.st[0].node.n') === 'FM' && run('fb.st[0].node.kids[0].n') === 'INIT', 'browser on an FM sound: root is FM, INIT folder first');
ok(run('fb.st.length') === 2 && run('fbItems(fb.st[1].node)[fb.st[1].i].val') === 1212, 'the browser opens on the sound that is on the track');
run('fb = null'); run('setEngine(0, 0); fbOpen()');
ok(run('fb.st[0].node.n') === 'SOUNDS' && !run('fb.st[0].node.kids.some(k => k.n === "NEW ENGINES")'), 'browser on a classic sound: no NEW ENGINES folder, the Engine knob does that');
ok(run('(function walk(n) { if (n.l) return n.l.every(t => t < 1064); return n.kids.every(walk); })(fb.st[0].node)'), 'browser on a classic sound: only classic sounds');
run('fb = null');
run('setKind(0, 1); fbOpen()'); ok(run('fb.st[0].node.kids.some(k => k.n === "NEW ENGINES")'), 'drum track: the NEW ENGINES folder is still there');
run('fb = null; setKind(0, 0)');

// --- titles
run('setEngine(0, 2); synPage = 1');
ok(run('engTitle()') === 'FM 2/2', 'title: FM 2/2'); run('setEngine(0, 0)'); ok(run('engTitle()') === 'SYNTH', 'title: SYNTH on ANALOG');

// --- fuzz: knobs, pages, engine switches, browser, undo in all views
let seed = 4242; const rnd = () => (seed = (seed * 1103515245 + 12345) & 0x7fffffff) / 0x7fffffff;
const ccs = [49, 3, 62, 63, 55, 54, 60, 119, 51, 58, 50, 56, 71, 72, 73, 74, 75, 76, 77, 78, 71, 72, 73, 74, 75, 76, 77, 78, 14];
for (let i = 0; i < 40000; i++) {
  const r = rnd();
  if (r < 0.04) run('setEngine(sel, ' + Math.floor(rnd() * 6) + ')');
  else if (r < 0.08) run('view = ' + [1, 9, 9, 1, 2, 3][Math.floor(rnd() * 6)] + '; viewItemsFix()');
  else if (r < 0.2) ctx.onMidiMessageInternal([0x90, 68 + Math.floor(rnd() * 32), rnd() < 0.5 ? 100 : 0]);
  else cc(ccs[Math.floor(rnd() * ccs.length)], rnd() < 0.5 ? 127 : (rnd() < 0.5 ? 1 : 3));
  if (i % 50 === 0) ctx.tick();
}
ok(writes.length === 0, 'fuzz 40000 events: no handler error logged' + (writes.length ? ' -> ' + String(writes[0]).slice(0, 300) : ''));
ok(run('pads.every(r => r.length === 72 && r.slice(61, 71).every(v => v === -1 || (v >= 0 && v <= 1)))'), 'fuzz: all pad rows still have 72 values and e0..e9 in -1 or 0..1');
console.log(fail ? 'FAILED ' + fail : 'ALL PASSED');
process.exit(fail ? 1 : 0);
