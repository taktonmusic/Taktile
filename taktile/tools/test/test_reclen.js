/* recording into an empty clip: a take of 2 bars stays 32 steps however it is stopped (Rec, Play/Stop) and an early downbeat does not add a 3rd bar */
const { ctx, run, params } = require('./harness.js');
let bad = 0; const ok = (c, m) => { console.log((c ? 'ok   ' : 'FAIL ') + m); if (!c) bad++; };
const T = n => { for (let i = 0; i < n; i++) ctx.tick(); };
const cc = (n, v) => ctx.onMidiMessageInternal([0xB0, n, v]);
ctx.init && ctx.init(); T(300); run('splashT0 = 0');
run('chosen[0] = 1; askKind = -1; selectTrack(0); setKind(0, 0); selectTrack(0); askKind = -1; view = 0'); T(10);

/* helper: a fresh empty clip on track 1 that is being recorded, the playhead at step `g` (1 = first step) */
const fresh = () => run('recArmed = true; playing = true; growTrk.fill(false); growTrk[0] = true; growG0[0] = 0; growP0[0] = 0; growMax[0] = 1; tph[0] = 0; lens[0] = 16; for (const l of lanesOf(0)) for (let c = 0; c < NPAT; c++) steps[c][l] = steps[c][l].map(() => [0, 100, 0, 0]);');
const at = (g, f) => { params.pos = '0,' + g + ',0,' + (f || 0); run('gs = ' + g + '; posFr = ' + (f || 0) + '; growTick()'); };
const walk = (a, b) => { for (let g = a; g <= b; g++) at(g, 0); };

/* 1. two bars, stopped with the Rec button right at the bar line */
fresh(); run('recordHit(0, 0, 100, { g: 1, f: 0 })'); walk(2, 32); at(33, 5);
run('recStopLen()'); ok(run('lens[0]') === 32, 'Rec stops at the end of bar 2: 32 steps (' + run('lens[0]') + ')');

/* 2. the same take stopped with the Play button (this used to leave 48 steps: the clip grows one bar ahead on the last step) */
fresh(); run('recordHit(0, 0, 100, { g: 1, f: 0 })'); walk(2, 32); at(33, 5);
cc(85, 127); cc(85, 0); T(2);   /* Play button (CC 85) */
ok(run('lens[0]') === 32, 'Play/Stop at the end of bar 2: 32 steps (' + run('lens[0]') + ')');
run('recArmed = false; playing = false');

/* 3. a hit played a little early on the downbeat of bar 3 (it snaps to step 33): the take still stops at 32 steps and the hit wraps to step 1 */
fresh(); run('recordHit(0, 0, 100, { g: 2, f: 0 }); recordHit(0, 5, 100, { g: 20, f: 0 })'); walk(3, 32);
run('recordHit(0, 7, 100, { g: 32, f: 80 })'); at(33, 20);
run('recStopLen()'); ok(run('lens[0]') === 32, 'an early downbeat does not make a 3rd bar (' + run('lens[0]') + ')');
ok(run('steps[ed[0]][0].filter(s => s[0]).length') === 3, 'and the early hit is not lost (' + run('steps[ed[0]][0].filter(s => s[0]).length') + ' notes)');

/* 4. real notes in bar 3 keep 48 */
fresh(); run('recordHit(0, 0, 100, { g: 1, f: 0 })'); walk(2, 40); run('recordHit(0, 3, 100, { g: 40, f: 0 })'); at(47, 0);
run('recStopLen()'); ok(run('lens[0]') === 48, 'a note in the middle of bar 3 keeps 48 steps (' + run('lens[0]') + ')');

/* 5. a short take (stopped inside bar 1) stays 16 */
fresh(); run('recordHit(0, 0, 100, { g: 1, f: 0 })'); walk(2, 10); at(11, 0);
run('recStopLen()'); ok(run('lens[0]') === 16, 'a take stopped inside bar 1: 16 steps (' + run('lens[0]') + ')');
run('recArmed = false; playing = false; growTrk.fill(false)');
console.log(bad ? bad + ' FAILED' : 'all ok'); process.exit(bad ? 1 : 0);
