// Drum browser: only the drum synth (+ sampler); it opens in the list of the sound on the pad.
const { ctx, run } = require('./harness.js');
let failed = 0; const ok = (c, m) => { console.log((c ? 'ok   ' : 'FAIL ') + m); if (!c) failed++; };
const T = (n) => { for (let i = 0; i < n; i++) ctx.tick(); };
ctx.init && ctx.init(); T(300);
run('splashOn = false; splashT0 = 0; chosen[0] = 1; askKind = -1; guideI = -1; needsRedraw = true'); T(20);
run('selectTrack(0); setKind(0, 1); selectTrack(0); askKind = -1; delete kitFresh[0]'); T(20);
const where = () => JSON.parse(run('JSON.stringify(fb.st.map(s => s.node.n || "root"))'));
const cur = () => run('(() => { const t = fb.st[fb.st.length - 1], it = fbItems(t.node)[t.i]; return it ? it.label : null; })()');
run('fbOpen(); fb.st = [fb.st[0]]');
const kids = JSON.parse(run('JSON.stringify(fbTree().kids.find(k => k.n === "DRUMS").kids.map(k => k.n))'));
ok(kids.join() === 'KICKS,SNARES,HATS,CYMBALS,CLAPS,TOMS,RIMS,PERCUSSION,SAMPLER', 'drum browser lists only the drum synth lists + sampler: ' + kids.join());
run('fb = null');
const cases = [['DKickDub', 'KICKS'], ['DSnrFat', 'SNARES'], ['DHatOp', 'HATS'], ['DClapTgt', 'CLAPS'], ['DTomHi', 'TOMS'], ['DCowbell', 'PERCUSSION'], ['DRim', 'RIMS'], ['Kick', 'KICKS'], ['Snare', 'SNARES'], ['HatCl', 'HATS']];
for (const [nm, list] of cases) {
  run('pads[ln()][0] = tIdx("' + nm + '"); fbOpen()');
  const w = where(); ok(w[w.length - 1] === list, nm + ' opens in ' + list + ' (' + w.join('>') + ')');
  run('fbBack(); while (fb) fbBack(); fb = null');
}
run('pads[ln()][0] = tIdx("DKickDub"); fbOpen()'); ok(cur() === 'Kick Dub' || /dub/i.test(cur()), 'and on the sound itself: ' + cur());
console.log(failed ? 'FAILED' : 'all ok'); process.exit(failed ? 1 : 0);
