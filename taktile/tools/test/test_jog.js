const { ctx, run, cc, writes } = require('./harness.js');
ctx.init(); for (let i = 0; i < 400; i++) ctx.tick();
const SHIFT = 49, JOGCLICK = 3, LEFT = 62, RIGHT = 63;
let fail = 0; const ok = (c, m) => { if (!c) { fail++; console.log('FAIL', m); } else console.log('ok  ', m); };
const shift = (f) => { cc(SHIFT, 127); f(); cc(SHIFT, 0); };
const click = () => { cc(JOGCLICK, 127); cc(JOGCLICK, 0); };
const setup = (v) => { run('splashT0 = 0; helpOpen = false; menuOpen = false; padMenu = false; broOpen = false; shiftHeld = false; exitDlg = -1; askKind = -1; try { fb = null; } catch (e) {} view = ' + v + '; needsRedraw = true;'); };
const key = (n) => { cc(n, 127); cc(n, 0); };
const chk = (v, setupJs, act, cond, msg) => { setup(v); if (setupJs) run(setupJs); act(); ok(cond(), msg); };
const R = () => key(RIGHT), L = () => key(LEFT);
// MIX
chk(20, 'mixMode = 0', click, () => run('mixMode') === 1 && run('view') === 20, 'MIX: click = next page (Volume > Delay)');
chk(20, 'mixMode = 0', () => shift(R), () => run('mixMode') === 1 && run('view') === 20, 'MIX: Shift+Right -> VOLUME->DELAY');
chk(20, 'mixMode = 0', () => shift(L), () => run('mixMode') === 2, 'MIX: Shift+Left wraps to 2');
// EUCLID
chk(36, 'euSetPage(0)', click, () => run('euPage') === 0, 'EUCLID: click does not switch page');
chk(36, 'euSetPage(0)', () => shift(R), () => run('euPage') === 1 && run('view') === 36, 'EUCLID: Shift+Right -> page 2');
chk(36, 'euSetPage(1)', () => shift(R), () => run('euPage') === 0, 'EUCLID: wraps back to page 1');
// SAMPLE
chk(28, 'smpPage = 0; prevTabView = 2', click, () => run('smpPage') === 0 && run('view') === 2, 'SAMPLE: click = back to last tab, no page cycling');
chk(28, 'smpPage = 0', () => shift(R), () => run('smpPage') === 1 && run('view') === 28, 'SAMPLE: Shift+Right -> SLICE');
chk(28, 'smpPage = 1', () => shift(R), () => run('smpPage') === 2, 'SAMPLE: Shift+Right -> ENV');
chk(28, 'smpPage = 2', () => shift(R), () => run('smpPage') === 0, 'SAMPLE: wraps to SAMPLE');
chk(28, 'smpPage = 1', () => shift(L), () => run('smpPage') === 0, 'SAMPLE: Shift+Left -> SAMPLE');
chk(28, 'smpPage = 0; broOpen = true', () => shift(R), () => run('smpPage') === 0, 'SAMPLE: browser open -> Shift+arrows ignored');
// SCOPE
chk(42, 'scopeMode = 0', click, () => run('scopeMode') === 0, 'SCOPE: click does not cycle');
chk(42, 'scopeMode = 0', () => shift(R), () => run('scopeMode') === 1, 'SCOPE: Shift+Right -> WAVE');
chk(42, 'scopeMode = 0', () => shift(L), () => run('scopeMode') === 2, 'SCOPE: Shift+Left wraps');
// unchanged behaviour
chk(0, '', () => shift(R), () => run('view') === 0, 'SEQ: Shift+Right still works, no crash');
chk(0, '', click, () => run('!!fb') === true, 'SEQ: click opens preset browser');
chk(20, '', () => { cc(SHIFT, 127); click(); }, () => run('isHub(view)') === true, 'MIX: Shift+click opens HOME'); cc(SHIFT, 0);
chk(40, '', click, () => true, 'UNDO HISTORY: click still confirms (no crash)');
// fuzz
let seed = 12345; const rnd = () => (seed = (seed * 1103515245 + 12345) & 0x7fffffff) / 0x7fffffff;
const ccs = [49, 3, 62, 63, 55, 54, 60, 119, 88, 85, 86, 51, 58, 50, 56, 118, 43, 42, 41, 40, 79, 52, 71, 72, 73, 74, 75, 76, 77, 78, 14];
for (let i = 0; i < 40000; i++) {
  const r = rnd();
  if (r < 0.15) { ctx.onMidiMessageInternal([0x90, 68 + Math.floor(rnd() * 32), rnd() < 0.5 ? 100 : 0]); }
  else if (r < 0.3) { ctx.onMidiMessageInternal([0x90, 16 + Math.floor(rnd() * 16), rnd() < 0.5 ? 100 : 0]); }
  else cc(ccs[Math.floor(rnd() * ccs.length)], rnd() < 0.5 ? 127 : (rnd() < 0.5 ? 1 : 0));
  if (i % 50 === 0) ctx.tick();
}
ok(writes.length === 0, 'fuzz 40000 events: no handler error logged' + (writes.length ? ' -> ' + String(writes[0]).slice(0, 200) : ''));
console.log(fail ? 'FAILED ' + fail : 'ALL PASSED');
process.exit(fail ? 1 : 0);
