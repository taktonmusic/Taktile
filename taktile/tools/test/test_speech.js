/* speech sound: a new speech sound plays one letter per step; hold a step + jogwheel = its letter */
const { ctx, run, params } = require('./harness.js');
const T = n => { for (let i = 0; i < n; i++) ctx.tick(); };
let bad = 0; const ok = (c, m) => { console.log((c ? 'ok   ' : 'FAIL ') + m); if (!c) bad++; };
params.pos = '0,0,0,0'; ctx.init && ctx.init(); T(300);
run('splashOn=false;splashT0=0;chosen.fill(1);askKind=-1;selectTrack(2);setKind(2,0);selectTrack(2);view=0;needsRedraw=true'); T(5);
run("fbSetType(TYPE_NAME.indexOf(\"SpkEn\"))"); T(3);
ok(run('Math.round(pads[2][PI.dmd])') === 2, 'a new speech sound starts in LETTER mode');
run('onStep(0,true)'); ok(run('steps[ed[2]][2][0][0]') === 1 && run('steps[ed[2]][2][0][8]') > 0, 'a new step has a letter');
const before = run('steps[ed[2]][2][0][8]');
run('heldStep=0'); run('onMidiMessageInternal([0xB0, CC.JOG, 1])'); run('onMidiMessageInternal([0xB0, CC.JOG, 1])'); T(2);
ok(run('steps[ed[2]][2][0][8]') > before, 'hold a step + jogwheel changes its letter');
console.log(bad ? bad + ' FAILED' : 'all ok'); process.exit(bad ? 1 : 0);
