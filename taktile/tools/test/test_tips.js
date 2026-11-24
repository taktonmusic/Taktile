/* first-time tips: a less obvious page shows its help once, a later visit does not; the menu entry turns tips off */
const { ctx, run } = require('./harness.js');
let bad = 0; const ok = (c, m) => { console.log((c ? 'ok   ' : 'FAIL ') + m); if (!c) bad++; };
const T = n => { for (let i = 0; i < n; i++) ctx.tick(); };
ctx.init && ctx.init(); T(300); run('splashT0 = 0; askKind = -1; view = 0; helpOpen = false'); T(5);
ctx.onMidiMessageInternal([0xB0, 71, 1]); T(2);                      /* a knob turn: the device is in use */
ok(run('tipsArmed') === true, 'tips start once the device is used');
run('setView(52)'); T(2);
ok(run('helpOpen') === true && run('view') === 52, 'the first visit to the TAPE page shows its help');
ctx.onMidiMessageInternal([0xB0, 51, 127]); T(2);                    /* any key closes it */
ok(run('helpOpen') === false, 'a key closes it');
run('setView(0)'); run('setView(52)'); T(2);
ok(run('helpOpen') === false, 'the second visit shows nothing');
run('setView(0); tipsSeen = {}; tipsOn = false; setView(52)'); T(2);
ok(run('helpOpen') === false, 'with tips off nothing opens');
console.log(bad ? bad + ' FAILED' : 'all ok'); process.exit(bad ? 1 : 0);
