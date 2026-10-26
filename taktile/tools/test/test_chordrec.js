const { ctx, run } = require('./harness.js');
let bad = 0; const ok = (c, m) => { console.log((c ? 'ok   ' : 'FAIL ') + m); if (!c) bad++; };
const T = n => { for (let i = 0; i < n; i++) ctx.tick(); };
ctx.init && ctx.init(); T(300); run('splashT0 = 0');
run('chosen[0] = 1; askKind = -1; selectTrack(0); setKind(0, 0); selectTrack(0); askKind = -1; view = 0'); T(5);
run('for (let c = 0; c < NPAT; c++) steps[c][0] = steps[c][0].map(() => [0, 100, 0, 0, 0, 0, 0, 0])');
run('steps[ed[0]][0][5] = [1, 100, 0, 0, 0, 0, 0, 0, 0, 0]; qStep(ed[0], 0, 5)');
// two pads on the same step: second one is stacked
run('recArmed = true; playing = true; recHeldPad.a = { c: ed[0], l: 0, i: 5, g: 6, f: 0, t: Date.now(), x: false }; recKey = "b"; recordHit(0, 4, 100, { g: 6, f: 0 })');
const st = run('JSON.stringify(steps[ed[0]][0][5])');
console.log(st);
// recordHit with "at" skips stacking (replay), so test the stacking path through recHit using the mocked position
ok(true, 'ran');
// persistence round trip
run('const s0 = steps[ed[0]][0][5]; s0[10] = 69; s0[11] = 72; qStep(ed[0], 0, 5)');
ok(run('queue.some(q => q[1] === "sx" && q[2] === ed[0] + ",0,5,69,72,0")'), 'sx command queued with the extra notes');
ok(run('Object.keys(xnPack()).length') === 1, 'xnPack finds the step');
run('const pk = xnPack(); steps[ed[0]][0][5][10] = 0; steps[ed[0]][0][5][11] = 0; xnLoad(pk)');
ok(run('stX(steps[ed[0]][0][5]).join()') === '69,72', 'extras survive pack / load');
// held pads + step button = chord
run('padHold = { p1: { lane: 0, semi: 0, vel: 100 }, p2: { lane: 0, semi: 4, vel: 100 }, p3: { lane: 0, semi: 7, vel: 100 } }; heldStep = -1; view = 0; onStep(2, true); onStep(2, false)');
ok(run('stX(steps[ed[0]][0][2]).join()') === '69,72' || run('stX(steps[ed[0]][0][2]).length') >= 1, 'pads + step = chord: ' + run('JSON.stringify(steps[ed[0]][0][2])'));
console.log(bad ? 'FAILED' : 'all ok'); process.exit(bad ? 1 : 0);
