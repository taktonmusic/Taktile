const { ctx, run, cc } = require('./harness.js');
const T = n => { for (let i = 0; i < n; i++) ctx.tick(); };
let bad = 0; const ok = (c, m) => { console.log((c ? 'ok   ' : 'FAIL ') + m); if (!c) bad++; };
ctx.init && ctx.init(); T(600);
run('splashOn=false;splashT0=0;chosen.fill(1);askKind=-1;view=0;playing=true;recArmed=true;gs=9;posFr=0;needsRedraw=true'); T(3);
run('editParam("master","m_filt",20,0)');
ok(Object.keys(run('AUTO')).some(k => /^0\|99\|m_filt$/.test(k)), 'master knob turn is recorded');
const before = run('master[MI.m_filt]');
run('recArmed=false; playing=true; gs=9+256; posFr=0; autoHold["0|99|m_filt"]=0; autoLast={}; autoPlay()'); run('gs=10+256; autoPlay()');
run('master[MI.m_filt]=0.1; gs=9+512; autoLast["0|99|m_filt"]=autoIdxM()-1; autoPlay()');
ok(Math.abs(run('master[MI.m_filt]') - before) < 0.01, 'the recorded value is played back (' + before.toFixed(3) + ')');
const js = JSON.stringify(run('AUTO')); ok(/m_filt/.test(js), 'saved key keeps underscore');
run('AUTO["0|99|m_filt"]={}; deleteHeld=true; editParam("master","m_filt",1,0); deleteHeld=false');
ok(!run('AUTO["0|99|m_filt"]'), 'Delete + knob clears it');
run('recArmed=true;playing=true;gs=20;posFr=0;editParam("sys","bpm",5,0)');
ok(Object.keys(run('AUTO')).some(k => /^0\|99\|bpm$/.test(k)), 'tempo turn is recorded');
{ const b = run('bpm'); run('bpm=100;recArmed=false;autoHold["0|99|bpm"]=0;autoLast["0|99|bpm"]=autoIdxM()-1;autoPlay()'); ok(run('bpm') === b, 'tempo is played back (' + b + ')'); }
run('smpDummy=0');
{ run('acc.fill(0); sel=0; pads[sel][0]=359'); const a = run('Math.round(pads[ln()][PI.smd]*100)'); for (let i = 0; i < 5; i++) run('editParam("pad","smd",1,3)'); ok(run('Math.round(pads[ln()][PI.smd]*100)') === a, 'sample mode needs more than 5 clicks to change'); run('editParam("pad","smd",1,3)'); ok(run('Math.round(pads[ln()][PI.smd]*100)') !== a, 'and changes with the 6th'); }
console.log(bad ? 'FAILED' : 'all ok'); process.exit(bad ? 1 : 0);
