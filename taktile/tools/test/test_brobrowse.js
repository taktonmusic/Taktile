// Sample browser: a pad tap plays the highlighted WAV without clicking it first.
const { ctx, run, params, setCalls } = require('./harness.js');
let failed = 0; const ok = (c, m) => { console.log((c ? 'ok   ' : 'FAIL ') + m); if (!c) failed++; };
const T = (n) => { for (let i = 0; i < n; i++) ctx.tick(); };
ctx.init && ctx.init(); T(300);
run('chosen[0] = 1; askKind = -1; selectTrack(0); setKind(0, 0); askKind = -1; needsRedraw = true'); T(20);
params.brres = '1'; run('splashT0 = 0');
run("view = 28; broOpen = true; broList = [['d','..'],['f','kick.wav',0],['f','snare.wav',1]]; broSel = 2; needsRedraw = true"); T(3);
setCalls.length = 0;
ctx.onMidiMessageInternal([0x90, 68, 100]); ctx.onMidiMessageInternal([0x80, 68, 0]);
ok(setCalls.some(c => c[0] === 'brprev' && c[1] === '1'), 'pad tap previews the highlighted WAV (second file)');
ok(!setCalls.some(c => c[0] === 'npad'), 'the pad plays no track note meanwhile');
ok(run('broOpen') === true, 'browser stays open');
run("broSel = 0"); setCalls.length = 0;
ctx.onMidiMessageInternal([0x90, 70, 100]); ctx.onMidiMessageInternal([0x80, 70, 0]); T(2);
ok(!setCalls.some(c => c[0] === 'brprev'), 'on a folder entry nothing is previewed');
run("broOpen = false; view = 0"); T(3); 
console.log(failed ? 'FAILED' : 'all ok'); process.exit(failed ? 1 : 0);
