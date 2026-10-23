/* hold a step + left / right arrow = nudge of that step */
const { ctx, run } = require('./harness.js');
let bad = 0; const ok = (c, m) => { console.log((c ? 'ok   ' : 'FAIL ') + m); if (!c) bad++; };
const T = n => { for (let i = 0; i < n; i++) ctx.tick(); };
ctx.init && ctx.init(); T(300); run('splashT0 = 0');
run('chosen[0] = 1; askKind = -1; selectTrack(0); setKind(0, 0); selectTrack(0); askKind = -1; view = 0'); T(10);
ctx.onMidiMessageInternal([0x90, 16 + 2, 100]); T(2);            /* hold step 3 */
const hs = run('heldStep'); ok(hs === 2, 'step 3 is held (' + hs + ')');
const n0 = run('stepAt(heldStep)[6]');
ctx.onMidiMessageInternal([0xB0, 63, 127]); ctx.onMidiMessageInternal([0xB0, 63, 127]); ctx.onMidiMessageInternal([0xB0, 63, 127]); T(2);
const n1 = run('stepAt(heldStep)[6]'); ok(n1 === n0 + 3, 'right arrow x3: nudge ' + n0 + ' -> ' + n1);
ctx.onMidiMessageInternal([0xB0, 62, 127]); T(2);
ok(run('stepAt(heldStep)[6]') === n1 - 1, 'left arrow: one step back');
for (let i = 0; i < 40; i++) ctx.onMidiMessageInternal([0xB0, 63, 127]);
ok(run('stepAt(heldStep)[6]') === run('NUD_MAX'), 'stops at the maximum');
ok(run('sel') === 0, 'the track did not change');
ctx.onMidiMessageInternal([0x80, 16 + 2, 0]); T(2);
ctx.onMidiMessageInternal([0xB0, 63, 127]); T(2);
ok(run('sel') === 1, 'without a held step the arrows still change the track');
console.log(bad ? bad + ' FAILED' : 'all ok'); process.exit(bad ? 1 : 0);
