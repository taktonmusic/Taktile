// MIC page test (mocked host): the input is only watched while the page is open, a take is armed / saved, no errors.
const { ctx, run, cc, writes, params, setCalls } = require('./harness.js');
let failed = 0; const ok = (c, m) => { console.log((c ? 'ok   ' : 'FAIL ') + m); if (!c) failed++; };
const T = (n) => { for (let i = 0; i < n; i++) ctx.tick(); };
const calls = (k) => setCalls.filter((c) => c[0] === k).map((c) => c[1]);
params.pos = '0,0,0,0'; ctx.init && ctx.init(); T(300);
run('chosen[0] = 1; askKind = -1; needsRedraw = true'); T(20);
run('selectTrack(0); setKind(0, 1); selectTrack(0); askKind = -1'); T(20);

run('view = 28; smpPage = 3; needsRedraw = true'); T(150);
ok(calls('micmon').length === 0, 'the input is not touched while another page is open');
run('smpPage = 4; needsRedraw = true'); T(150);
ok(calls('micmon').slice(-1)[0] === '1', 'opening the MIC page switches the level meter on');
params.micpeak = '55,0,1'; T(150);
ok(run('micLvl') === 55, 'the level of the input is shown (' + run('micLvl') + ')');
params.micpeak = '0,0,0'; T(150);
ok(run('micOk') === 0, 'no input signal is reported');
params.micpeak = '30,0,1'; T(150);

const knob = (k, v) => { ctx.onMidiMessageInternal([0xB0, 71 + k, v]); };
const t0 = run('micSecI'); knob(0, 1); knob(0, 1); ok(run('micSecI') === t0 + 1, 'knob 1 sets the length');
const g0 = run('micGainI'); knob(1, 1); knob(1, 1); ok(run('micGainI') === g0 + 1 && calls('micgain').slice(-1)[0] === '15', 'knob 2 sets the gain and tells the DSP');
const w0 = run('micThr'); knob(2, 127); knob(2, 127); ok(run('micThr') === w0 - 1, 'knob 3 sets wait-for-sound');

// a take
run('micThr = 0'); setCalls.length = 0; params.recstate = '0,0,-1'; run('micGo()'); T(150);
ok(run('micState') === 2, 'a 0 right after the start (DSP has not seen the command yet) does not end the take');
ok(calls('micarm').length === 1 && /^\d+,0,\d+$/.test(calls('micarm')[0]), 'start arms a take at once (micarm ' + calls('micarm')[0] + ')');
params.recstate = '2,40,-1'; T(150);
ok(run('micState') === 2 && run('micPct') === 40, 'the take runs, progress shown');
run('micGo()'); ok(calls('micstop').length === 1, 'starting again while a take runs stops it');
let saved = false;
Object.defineProperty(params, 'recstate', { configurable: true, get() { return saved ? '0,0,7' : '3,100,-1'; } });
const oSet = ctx.host_module_set_param; ctx.host_module_set_param = (k, v) => { if (k === 'recsave') saved = true; oSet(k, v); };
T(60);
ok(calls('recsave').length === 1 && run('micState') === 0, 'a finished take is saved once');
ok(Math.round(run('pads[micLane][0]')) === 359 && Math.abs(run('pads[micLane][PI.smp]') - 7 / 63) < 1e-6, 'the take is loaded as the sample of the pad');
ctx.host_module_set_param = oSet;

// leaving the page
run('view = 1; needsRedraw = true'); T(150);
ok(calls('micmon').slice(-1)[0] === '0', 'leaving the page switches the level meter off');
// wait for sound state does not break drawing
run('view = 28; smpPage = 4; micState = 5; needsRedraw = true'); T(10); run('micState = 0');
ok(writes.length === 0, 'no errors logged');
console.log(failed ? failed + ' FAILED' : 'all ok'); process.exit(failed ? 1 : 0);
