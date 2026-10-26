// Naming: after the last letter an OK field; clicking it confirms (like Menu).
const { ctx, run } = require('./harness.js');
let failed = 0; const ok = (c, m) => { console.log((c ? 'ok   ' : 'FAIL ') + m); if (!c) failed++; };
ctx.init && ctx.init(); for (let i = 0; i < 300; i++) ctx.tick(); run('splashOn = false; splashT0 = 0; chosen[0] = 1; askKind = -1; guideI = -1; helpOpen = false');
run("projUI = { i: 2, mode: 2, a: 0, nm: [1,2,3,0,0,0,0,0,0,0,0,0], cur: 0, then: 0, scn: true }; needsRedraw = true"); ctx.tick();
const click = () => { ctx.onMidiMessageInternal([0xB0, 3, 127]); ctx.onMidiMessageInternal([0xB0, 3, 0]); ctx.tick(); };
for (let i = 0; i < 11; i++) click();
ok(run('projUI.cur') === 11, 'cursor on the last letter');
click(); ok(run('projUI && projUI.cur') === 12, 'one more click: cursor on OK');
ctx.onMidiMessageInternal([0xB0, 14, 1]); ok(run('projUI.nm[11]') === 0, 'turning on OK changes no letter');
click(); ok(run('projUI') === null, 'click on OK confirms and closes');
ok(run('sceneNames[2]') === 'ABC', 'name saved: ' + run('sceneNames[2]'));
console.log(failed ? 'FAILED' : 'all ok'); process.exit(failed ? 1 : 0);
