const { ctx, run, params, setCalls, files } = require('./harness.js');
params.pos = '0,0,0,0';
ctx.init(); for (let i = 0; i < 400; i++) ctx.tick();
let fail = 0; const ok = (c, m) => { if (!c) { fail++; console.log('FAIL', m); } else console.log('ok  ', m); };
const ticks = (n) => { for (let i = 0; i < n; i++) ctx.tick(); };
const pos = (playing, gs) => { params.pos = (playing ? '1' : '0') + ',' + gs + ',0,0'; };
const calls = (k) => setCalls.filter((c) => c[0] === k).map((c) => c[1]);
const PF = (n) => '/data/UserData/schwung/taktile-proj-' + n + '.json';
run('splashT0 = 0; askKind = -1; exitDlg = -1;');

// two projects: slot 3 = 100 BPM, current = 140 BPM
run('bpm = 100; projSlot = 2; projSave();'); const saved = files[PF(3)];
ok(!!saved && JSON.parse(saved).bpm === 100, 'project slot 3 saved with 100 BPM');
run('bpm = 140; saveState();');

// playing: Load is queued, not executed
pos(true, 5); ticks(3); ok(run('playing') === true, 'transport is playing (mock)');
setCalls.length = 0; run('projSlot = 2; projLoadOrQueue();');
ok(run('projQueued') === 2 && run('bpm') === 140 && calls('play').length === 0, 'Load while playing: queued, nothing stops, nothing changes yet');
pos(true, 9); ticks(3); ok(run('projQueued') === 2 && run('projLive') === 0, 'mid-bar: still waiting');
pos(true, 15); ticks(1);   // g = 14 -> last two 16ths of the bar
ok(run('projLive') === 1 && run('projQueued') === -1, 'end of bar: switch starts');
const muted = calls('mute').filter((v) => /,1$/.test(v)).length;
ok(muted >= 16, 'all 16 tracks muted at the switch (' + muted + ' mute calls)');
ok(JSON.parse(files['/data/UserData/schwung/taktile-state.json']).bpm === 100, 'state file now holds the new project');
ok(calls('play').length === 0, 'transport was not stopped while loading');
let n = 0; while (run('projLive') === 1 && n < 400) { ticks(1); n++; }
ok(run('projLive') === 0 && run('bpm') === 100, 'new project active (BPM 100) after ' + n + ' ticks, ~' + Math.round(n * 22.7) + ' ms');
const pl = setCalls.filter((c) => c[0] === 'play').map((c) => c[1]);
ok(pl.join(',') === '0,1', 'transport restarted at step 1 after the load (' + pl + ')');
const lastUnmute = calls('mute').slice(-16);
ok(lastUnmute.length === 16 && lastUnmute.every((v) => /,\d$/.test(v)), 'mutes restored from the new project');
ok(run('fastFlush') === false, 'fast flush switched off again');

// cancel by turning Load again
pos(true, 20); ticks(2); run('projSlot = 2; projLoadOrQueue(); projLoadOrQueue();');
ok(run('projQueued') === -1, 'second Load on the same slot cancels');
// stop while queued
run('projLoadOrQueue();'); ok(run('projQueued') === 2, 're-armed'); pos(false, 0); ticks(3);
ok(run('projQueued') === -1, 'stopping the transport cancels the queue');
// empty / corrupt slots are refused
pos(true, 20); ticks(2); run('projSlot = 6; projLoadOrQueue();'); ok(run('projQueued') === -1, 'empty slot: not queued');
files[PF(8)] = 'not json {'; run('projSlot = 7; projLoadOrQueue();'); ok(run('projQueued') === -1, 'corrupt slot: not queued');
// not playing: immediate load as before
pos(false, 0); ticks(3); run('bpm = 140; saveState(); projSlot = 2; projLoadOrQueue();');
ok(run('bpm') === 100 && run('projQueued') === -1, 'stopped: Load is immediate as before');

// two switches in a row while playing
pos(true, 5); ticks(2); run('bpm = 140; projSlot = 2; projLoadOrQueue();'); pos(true, 31); ticks(1);
n = 0; while (run('projLive') === 1 && n < 400) { ticks(1); n++; }
ok(run('bpm') === 100 && run('projLive') === 0, 'second live switch works too');
console.log(fail ? 'FAILED ' + fail : 'ALL PASSED'); process.exit(fail ? 1 : 0);
