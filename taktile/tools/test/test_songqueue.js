// Song: tempo, mutes and transposition of the next step wait for the bar line (queued keys), STAY holds the step, a tapped step waits for the bar end.
const { ctx, run, setCalls, params } = require('./harness.js');
const T = n => { for (let i = 0; i < n; i++) ctx.tick(); };
let bad = 0; const ok = (c, m) => { console.log((c ? 'ok   ' : 'FAIL ') + m); if (!c) bad++; };
ctx.init && ctx.init(); T(300);
run('splashOn=false;splashT0=0;view=31;needsRedraw=true'); T(3);
run('for (let t=0;t<4;t++) for (let c=0;c<8;c++) steps[c][lanesOf(t)[0]][0]=[1,100,0,0,0,0,0,0]'); T(2);
run('songPadNew(24+0)'); run('songStepKey(1)'); run('songStepKey(2)');
run('songList.forEach(e => { e[1] = 1; }); songList[1][3] = 100; songList[1][8] = 5; songList[1][7] = [0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0]; songList[1][6][0] = 1;');
run('songSlot=0;songOn=true;songLoop=true;songSent=-1;posFail=0'); params.pos = '1,1,0,0,0'; T(5);
params.pos = '1,3,0,0,0'; T(3);
setCalls.length = 0; params.pos = '1,14,0,0,0'; T(4);
const keys = setCalls.map(c => c[0]);
ok(keys.includes('qbpm') && keys.includes('qsctr') && keys.includes('qmute'), 'next step: tempo, transpose and mutes are queued');
ok(!keys.includes('bpm') && !keys.includes('sctr') && !keys.includes('mute'), 'next step: nothing changes at once');
ok(keys.includes('qbar'), 'the bar line applies them together with the clips');
// STAY: the step comes round again
run('songOn=true;songSent=-1;songNext=-1'); params.pos = '1,1,0,0,0'; T(5); params.pos = '1,3,0,0,0'; T(3);
setCalls.length = 0; run('loopHeld=true;loopT=Date.now()'); params.pos = '1,14,0,0,0'; T(4);
ok(setCalls.some(c => c[0] === 'qsctr' && c[1] === '0') && !setCalls.some(c => c[0] === 'qsctr' && c[1] === '5'), 'STAY: the song starts the same step again, not the next one');
run('loopHeld=false');
console.log(bad ? bad + ' FAILED' : 'all ok'); process.exit(bad ? 1 : 0);
