// Sound browser test: sorted lists, level knobs, preview modes, pads while browsing (mocked host).
const { ctx, run, cc, writes, setCalls } = require('./harness.js');
let failed = 0; const ok = (c, m) => { console.log((c ? 'ok   ' : 'FAIL ') + m); if (!c) failed++; };
const T = (n) => { for (let i = 0; i < n; i++) ctx.tick(); };
ctx.init && ctx.init(); T(300);
run('chosen[0] = 1; askKind = -1; needsRedraw = true'); T(20);
run('selectTrack(0); setKind(0, 0); selectTrack(0); askKind = -1'); T(20);
const knob = (k, v) => { run('fbKT = 0'); ctx.onMidiMessageInternal([0xB0, 71 + k, v]); };
const clicks = (k, v, n) => { for (let i = 0; i < n; i++) knob(k, v); };
const path = () => JSON.parse(run('JSON.stringify(fb.st.map(s => s.node.n || "root"))'));
const label = () => run('(() => { const t = fb.st[fb.st.length - 1], it = fbItems(t.node)[t.i]; return it ? it.label : null; })()');

// 1. every list of sounds / kits / samples / effects is sorted by name (INIT first)
run('fbOpen(); fb.st = [fb.st[0]]');
let leaves = 0, unsorted = 0;
const walk = (p) => {
  if (run('!!(' + p + ').kids')) { const n = run('(' + p + ').kids.length'); for (let i = 0; i < n; i++) walk(p + '.kids[' + i + ']'); return; }
  const l = JSON.parse(run('JSON.stringify(fbItems(' + p + ').map(x => x.label))')); const body = l[0] === 'INIT' ? l.slice(1) : l;
  const s = body.slice().sort((a, b) => a.localeCompare(b, 'en', { numeric: true, sensitivity: 'base' })); leaves++;
  if (body.some((x, i) => x !== s[i])) unsorted++;
};
walk('fbTree()'); ok(leaves > 50 && unsorted === 0, 'all ' + leaves + ' sound lists are sorted by name (' + unsorted + ' unsorted)');

// 2. level knobs: knob 1 changes the main folder, the display follows, 6 clicks make one step
run('fbOpen()'); T(3);
const d0 = run('fb.st.length'), i0 = run('fb.st[0].i');
clicks(0, 1, 5); ok(run('fb.st[0].i') === i0, '5 clicks on knob 1: no step yet');
clicks(0, 1, 1); const i1 = run('fb.st[0].i'); ok(i1 !== i0, 'the 6th click moves the main folder (' + i0 + ' -> ' + i1 + ')');
ok(run('fbShow') === 0, 'the display shows the level of knob 1');
clicks(1, 1, 6); ok(run('fbShow') === 1 || run('fb.st.length') === 1, 'knob 2 shows the next level');
ok(run('fbShowIdx()') >= 0, 'shown level is valid');

// 3. preview mode on knob 8: OFF / STOP / ALL (Shift + knob 8 = volume)
run('fbPm = 1'); clicks(7, 1, 2); ok(run('fbPm') === 2, 'knob 8 right: preview ALL');
clicks(7, 127, 4); ok(run('fbPm') === 0, 'knob 8 left: preview OFF');
run('fbSetPm(1)'); run('playing = false'); ok(run('fbPrevOn()') === true, 'STOP: preview active while stopped');
run('playing = true'); ok(run('fbPrevOn()') === false, 'STOP: preview silent while playing');
run('fbSetPm(2)'); ok(run('fbPrevOn()') === true, 'ALL: preview also while playing');
run('playing = false; fbSetPm(0)');

// 4. a pad press in the browser loads the highlighted sound and records nothing
run('fbOpen(); while (fb.st.length > 1 && !fbIsLeaf(fb.st[fb.st.length - 1].node)) fbClick();');
run('fbMove(1, 1)'); const typeBefore = run('Math.round(pads[ln()][0])'); const target = run('fbItems(fb.st[fb.st.length - 1].node)[fb.st[fb.st.length - 1].i].val');
const stepsBefore = run('JSON.stringify(steps)'); run('recArmed = true; playing = true'); setCalls.length = 0;
ctx.onMidiMessageInternal([0x90, 68, 100]); T(2); ctx.onMidiMessageInternal([0x90, 68, 0]); T(2);
ok(run('JSON.stringify(steps)') === stepsBefore, 'pad pressed while browsing in record mode: nothing is recorded');
console.log('DBG actual', run('Math.round(pads[ln()][0])'), 'target', target, 'ln', run('ln()'), 'fb', run('fb ? JSON.stringify(fb.st.map(s=>s.i)) : null'), 'kind', run('kind[sel]'), 'view', run('view'), 'menu', run('menuOpen'));
ok(run('Math.round(pads[ln()][0])') === target, 'pad press with preview OFF loads the highlighted sound (' + typeBefore + ' -> ' + target + ')');
ok(setCalls.some((c) => c[0] === 'npad' && /^0,\d+,0$/.test(c[1])), 'and the pad plays it');

ok(writes.length === 0, 'no handler error logged' + (writes.length ? ' -> ' + String(writes[0]).slice(0, 200) : ''));
if (failed) { console.log('FAILED ' + failed); process.exit(1); }
console.log('all ok');
