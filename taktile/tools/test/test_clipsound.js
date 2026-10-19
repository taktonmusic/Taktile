/* a new (empty) clip starts with the sound of the clip you came from; changes after that stay in their own clip */
const { ctx, run, params } = require('./harness.js');
const T = n => { for (let i = 0; i < n; i++) ctx.tick(); };
let bad = 0; const ok = (c, m) => { console.log((c ? 'ok   ' : 'FAIL ') + m); if (!c) bad++; };
params.pos = '0,0,0,0'; ctx.init && ctx.init(); T(300);
run('chosen[0]=1;askKind=-1;needsRedraw=true'); T(20); run('selectTrack(1);setKind(1,0);selectTrack(1);askKind=-1'); T(20);
run('splashOn=false;splashT0=0');
run('ed[1]=0; setLaneP(1,"tune",0.8)'); const v0 = run('pads[1][PI.tune]');
run('setClip(1,1)'); T(5);
ok(Math.abs(run('pads[1][PI.tune]') - v0) < 1e-6, 'a new clip 2 takes over the sound of clip 1 (tune ' + run('pads[1][PI.tune]').toFixed(2) + ')');
run('setLaneP(1,"tune",0.2)'); const v1 = run('pads[1][PI.tune]');
run('setClip(1,0)'); T(5); ok(Math.abs(run('pads[1][PI.tune]') - v0) < 1e-6, 'clip 1 keeps its own sound');
run('setClip(1,1)'); T(5); ok(Math.abs(run('pads[1][PI.tune]') - v1) < 1e-6, 'clip 2 keeps the sound you changed (not overwritten again)');
run('setClip(1,2)'); T(5); ok(Math.abs(run('pads[1][PI.tune]') - v1) < 1e-6, 'clip 3 follows the clip you came from (clip 2)');
console.log(bad ? bad + ' FAILED' : 'all ok'); process.exit(bad ? 1 : 0);
