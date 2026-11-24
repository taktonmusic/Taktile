/* Undo track type: the switch Synth <-> Drum can be taken back; the synth notes and the drum kit both stay stored */
const { ctx, run } = require('./harness.js');
let bad = 0; const ok = (c, m) => { console.log((c ? 'ok   ' : 'FAIL ') + m); if (!c) bad++; };
const T = n => { for (let i = 0; i < n; i++) ctx.tick(); };
ctx.init && ctx.init(); T(300); run('splashT0 = 0'); run('askKind = -1; view = 0'); T(5);
run('chosen[5] = 1; selectTrack(5)'); T(3);
ok(run('kind[5]') === 0, 'track 6 starts as a synth track');
run('steps[ed[5]][5][0][0] = 1; steps[ed[5]][5][0][1] = 100'); const before = run('JSON.stringify(steps[ed[5]][5][0])');
run('setKind(5, 1)'); T(3);
ok(run('kind[5]') === 1, 'switched to Drum');
run('undoKind()'); T(3);
ok(run('kind[5]') === 0, 'Undo track type: back to Synth');
ok(run('JSON.stringify(steps[ed[5]][5][0])') === before, 'the synth note is still there');
run('undoKind()'); T(3);
ok(run('kind[5]') === 0, 'a second Undo without a new switch does nothing');
run('setKind(5, 1)'); run('setKind(5, 0)'); run('undoKind()'); T(3);
ok(run('kind[5]') === 1, 'Undo goes back one switch at a time');
console.log(bad ? bad + ' FAILED' : 'all ok'); process.exit(bad ? 1 : 0);
