const { ctx, run, setCalls, params } = require('./harness.js');
const T = n => { for (let i = 0; i < n; i++) ctx.tick(); };
let bad = 0; const ok = (c, m) => { console.log((c ? 'ok   ' : 'FAIL ') + m); if (!c) bad++; };
ctx.init && ctx.init(); T(300);
run('splashOn=false;splashT0=0;view=31;needsRedraw=true'); T(3);
run('for (let t=0;t<8;t++) for (let c=0;c<8;c++) steps[c][lanesOf(t)[0]][0]=[1,100,0,0,0,0,0,0]'); T(2);
run('songPadNew(24+0)'); run('songStepKey(1)'); run('songStepKey(2)'); run('songList[2][6][0]=3;songList[1][6][0]=2');
run('songSlot=0;songOn=true;songLoop=true;songSent=-1'); run('posFail=0'); params.pos='1,1,0,0,0'; T(5);
params.pos='1,21,0,0,0'; T(3); console.log('sent', run('songSent'), run('songSlot'));
setCalls.length = 0; { const SF = run('STEP_FIRST'); ctx.onMidiMessageInternal([0x90, SF + 2, 100]); T(2); ctx.onMidiMessageInternal([0x80, SF + 2, 0]); T(2); }
ok(run('songSlot') === 2 && run('songNext') === 2, 'first tap: the step waits for the bar end (NEXT)');
{ const SF = run('STEP_FIRST'); ctx.onMidiMessageInternal([0x90, SF + 2, 100]); T(2); ctx.onMidiMessageInternal([0x80, SF + 2, 0]); T(2); }
ok(run('songSlot') === 2 && run('songNext') === -1, 'second tap on the waiting step: jump now');
ok(setCalls.some(c => c[0] === 'clip' && /^0,3/.test(c[1])) || run('clipsQ[0]') === 3, 'track 1 gets clip 4 at once');
params.pos='1,22,0,0,0'; T(3); ok(run('songPlayIdx()') === 2, 'song position is step 3');
console.log(run('JSON.stringify([songList.map(e=>[e[1],e[2],e[6][0]]),songOff,songSent,songTotal(),gs,songPlayIdx(),clipsQ[0],songOn,playing])'));console.log(bad ? bad + ' FAILED' : 'all ok'); process.exit(bad ? 1 : 0);
