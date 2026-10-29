const { ctx, run, cc } = require('./harness.js');
const T = n => { for (let i = 0; i < n; i++) ctx.tick(); };
let bad = 0; const ok = (c, m) => { console.log((c ? 'ok   ' : 'FAIL ') + m); if (!c) bad++; };
ctx.init && ctx.init(); T(600);
run('splashOn=false;splashT0=0;chosen.fill(1);askKind=-1;view=0;playing=false;needsRedraw=true'); T(3);
run('openHub(); hubSel[16]=HUBS[16][1].findIndex(t=>t[1]===31)'); T(2);
cc(run('CC.JOGCLICK'), 127); cc(run('CC.JOGCLICK'), 0); T(5);
const K8 = run('CC.KNOB1+7');
// song off -> turn knob 8 until it is on again: the song must really start
run('songOn=false;playing=true;gs=40;songSent=-1;songTick()');
cc(K8, 5);
T(2); run('songTick()');
ok(run('songOn') === true && run('songSent') >= 0, 'turning the Song knob on starts the song (sent ' + run('songSent') + ')');
// hold a step
const SF = run('STEP_FIRST'), pad = (n, v) => ctx.onMidiMessageInternal([v ? 0x90 : 0x80, n, v]);
run('songSlot=0'); pad(SF + 1, 100); T(2);
ok(run('songHeldKey') === 1, 'step key held');
run('songHeldAt=Date.now()-1000; songTick()'); run('draw()');
ok(run('songHeldShown') === true, 'options shown after a moment');
pad(SF + 1, 0); T(2);
ok(run('songHeldKey') === -1, 'options go away on release');
run('draw()');
console.log(bad ? 'FAILED' : 'all ok'); process.exit(bad ? 1 : 0);
