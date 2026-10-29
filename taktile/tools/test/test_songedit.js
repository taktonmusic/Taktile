const { ctx, run, cc } = require('./harness.js');
const T = n => { for (let i = 0; i < n; i++) ctx.tick(); };
let bad = 0; const ok = (c, m) => { console.log((c ? 'ok   ' : 'FAIL ') + m); if (!c) bad++; };
ctx.init && ctx.init(); T(600);
run('splashOn=false;splashT0=0;chosen.fill(1);askKind=-1;view=0;playing=false;needsRedraw=true'); T(3);
run('songList=[[0,2,1,0,0,0],[1,4,1,0,0,0],[2,2,1,0,0,0]];songSlot=1;view=31;songOn=true;songLoop=true;playing=true;gs=1;songSent=-1;songOff=0;songNext=-1;songTick()'); T(2);
// advance bars: bar b begins at gs = 1+16*b
const at = b => { run('gs=' + (1 + 16 * b + 2) + ';songTick()'); };
for (let b = 0; b < 3; b++) at(b);
const cur = () => run('(()=>{const bb=Math.floor(Math.max(0,gs-1)/16),pl=songEntryAt(bb)[0];return [pl,(((bb+songOff)%songTotal())+songTotal())%songTotal()-songStart(pl)]})()');
let c = cur(); console.log('now', c);
ok(c[0] === 1 && c[1] === 2, 'playing step 2, bar 3 of 4 (before edit)');
// shrink bars of playing step 4 -> 2, keep playing the same step, no stall
run('songSlot=1'); const sn0 = run('songSent');
run('(()=>{const sn=songSnap();songList[1][1]=2;songFix(sn)})()');
c = cur(); console.log('after', c, 'sent', run('songSent'));
ok(c[0] === 1 && c[1] === 1, 'still step 2, bar 2 of 2 after Bars 4->2');
ok(run('songSent') >= 0, 'song not stalled');
// grow to 6
run('(()=>{const sn=songSnap();songList[1][1]=6;songFix(sn)})()'); c = cur();
ok(c[0] === 1 && c[1] === 1, 'still step 2 bar 2 after Bars 2->6');
// insert before: playing step index shifts
run('(()=>{const sn=songSnap();songList.splice(0,0,songCopy(songList[0]));songFix(sn)})()'); c = cur();
ok(c[0] === 2 && c[1] === 1, 'insert before keeps playing step (now index 2)');
// delete playing step -> songNext set, no crash
run('(()=>{const sn=songSnap();songList.splice(2,1);songFix(sn)})()');
ok(run('songNext') >= 0, 'deleted playing step -> next chosen');
console.log(bad ? 'FAILED' : 'all ok'); process.exit(bad ? 1 : 0);
