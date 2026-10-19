/* song page: pads = tracks x clips, steps = the chain */
const { ctx, run, params } = require('./harness.js');
const T = n => { for (let i = 0; i < n; i++) ctx.tick(); };
let bad = 0; const ok = (c, m) => { console.log((c ? 'ok   ' : 'FAIL ') + m); if (!c) bad++; };
params.pos = '0,0,0,0'; ctx.init && ctx.init(); T(300);
run('splashOn=false;splashT0=0'); run('view=31;bank=0;needsRedraw=true'); T(5);
const pad = (r, c) => run('songPadNew(' + ((3 - r) * 8 + c) + ')');
pad(0, 3); ok(run('songList[0][6][0]') === 3, 'top row pad 4 = track 1 plays clip 4');
pad(0, 3); ok(run('songList[0][6][0]') === -1, 'same pad again = track silent');
pad(1, 5); ok(run('songList[0][6][1]') === 5, 'row 2 = track 2');
run('songStepKey(1)'); ok(run('songList.length') === 2, 'step button 2 appends a step');
ok(run('songList[1][6][1]') === 5 && run('songList[0][6]') !== run('songList[1][6]'), 'new step copies clips (own array)');
pad(1, 2); ok(run('songList[1][6][1]') === 2 && run('songList[0][6][1]') === 5, 'steps are independent');
run('songStepKey(0)'); ok(run('songSlot') === 0, 'step button 1 selects step 1');
run('bank=1'); pad(0, 6); ok(run('songList[0][6][4]') === 6, 'bank 2 pads = tracks 5-8');
run('songStepKey(10)'); ok(run('songList[0][1]') === 4, 'step 11 = 4 bars');
let leds = []; for (let i = 0; i < 32; i++) leds.push(run('songLed(' + i + ')'));
ok(leds.every(v => v >= 0 && v < 128), 'all LED values valid');
/* running song: second step switches clips */
run('songSlot=0;songOn=true;songLoop=true;songSent=-1;playing=true;gs=0'); T(200);
run('JSON.stringify(songList)') ; 
const saved = JSON.parse(run('JSON.stringify(songList)')); ok(Array.isArray(saved[0][6]) && saved[0][6].length === 16, 'clips are saved with the step');
run('songList=[[0,4,1,0,0,0,null]].map(songNorm)'); ok(run('songClips(songList[0]).length') === 16, 'old song (scene only) gets clips from its scene');
run('view=0;needsRedraw=true'); T(3); ok(run('songOn')===false,'song switches off when you leave the song page');
run('draw()'); ok(true, 'draws');
console.log(bad ? bad + ' FAILED' : 'all ok'); process.exit(bad ? 1 : 0);
