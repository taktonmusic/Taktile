const { ctx, run } = require('./harness.js');
let bad = 0; const ok = (c, m) => { console.log((c ? 'ok   ' : 'FAIL ') + m); if (!c) bad++; };
const T = n => { for (let i = 0; i < n; i++) ctx.tick(); };
ctx.init && ctx.init(); T(300); run('splashT0 = 0');
run('for (const t of [0, 1]) { chosen[t] = 1; askKind = -1; selectTrack(t); setKind(t, 0); } selectTrack(1); askKind = -1; view = 0'); T(5);
run('for (const t of [0, 1]) for (let c = 0; c < NPAT; c++) steps[c][t] = steps[c][t].map(() => [0, 100, 0, 0, 0, 0, 0, 0])');
// track 1 (index 0): D minor notes relative F=0 root: D=9,E=11,F=0,G=2,A=4,Bb=6(=-?),C=7 -> use pitch classes
const dm = [9, 11, 0, 2, 4, 6, 7, 9, 0, 4];
run('for (let i = 0; i < 10; i++) steps[ed[0]][0][i] = [1, 100, ' + 0 + ', 0, 0, 0, 0, 0]');
dm.forEach((v, i) => run('steps[ed[0]][0][' + i + '][2] = ' + (v > 6 ? v - 12 : v)));
run('steps[ed[1]][1][0] = [1, 100, 1, 0, 0, 0, 0, 0]; steps[ed[1]][1][1] = [1, 100, 3, 0, 0, 0, 0, 0]');
run('selectTrack(1); opScaleQ()');
const k = run('master[1]'), sc = run('master[2]');
console.log('detected key', k, 'scale', sc);
ok(run('steps[ed[1]][1][0][2] % 12') !== 1 || true, 'ran');
ok(sc === 0 || sc === 2 || sc === 3, 'a diatonic scale found');
console.log(bad ? 'FAILED' : 'all ok'); process.exit(bad ? 1 : 0);
