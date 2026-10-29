const { ctx, run, cc, setCalls } = require('./harness.js');
const T = n => { for (let i = 0; i < n; i++) ctx.tick(); };
let bad = 0; const ok = (c, m) => { console.log((c ? 'ok   ' : 'FAIL ') + m); if (!c) bad++; };
ctx.init && ctx.init(); T(600);
run('splashOn=false;splashT0=0;kind[sel]=0;setLaneP(ln(),"type",359);view=28;smpPage=0;needsRedraw=true'); T(3);
ok(run('PI.sscn>PI.l4p && PI.stps===PI.sunp+1'), 'new keys come after the LFO keys');
ok(run('pads[0].length===PK.length') , 'pad arrays have the new length');
const JC = run('CC.JOGCLICK');
for (let i = 0; i < 5; i++) cc(JC, 127), cc(JC, 0);
ok(run('smpPage') === 5, 'jog click cycles to the PLAY page (' + run('smpPage') + ')');
run('draw()'); ok(true, 'PLAY page draws');
ok(run('JSON.stringify(VIEW_ITEMS[itemsView()].map(i=>i[0]))').indexOf('sscn') > 0, 'items are the play items');
setCalls.length = 0;
for (let i = 0; i < 6; i++) cc(run('CC.KNOB1'), 1);
T(200); ok(setCalls.some(c => c[0] === 'p01_sscn' && Number(c[1]) > 0), 'knob 1 sends the scan value to the DSP (p01_sscn)');
ok(run('fmtVal("sscn",0)') === 'OFF' && /%/.test(run('fmtVal("sscn",0.5)')), 'scan shows OFF / percent');
ok(run('fmtVal("sunv",0)') === 'OFF' && run('fmtVal("sunv",1)') === '4 VC', 'voices show OFF / 4 VC');
ok(/s$/.test(run('fmtVal("stps",0.5)')), 'tape stop shows seconds');
// voices knob steps by 1 voice
setCalls.length = 0; const v0 = run('pads[ln()][PI.sunv]');
for (let i = 0; i < 6; i++) cc(run('CC.KNOB1 + 1'), 1);
ok(run('pads[ln()][PI.sunv]') > v0 && Math.abs(run('pads[ln()][PI.sunv]') * 3 - Math.round(run('pads[ln()][PI.sunv]') * 3)) < 1e-6, 'voices knob moves in whole voices');
// LFO target list has SCAN
ok(run('ENUMS.l1t[17]') === 'SCAN', 'LFO target SCAN exists');
// old project with shorter arrays loads
run('(function(){ const a = new Array(PK.length - 4).fill(0); while (a.length < PK.length) a.push(defOf(a.length)); pads[0] = a; })()'); run('draw()'); ok(run('pads[0][PI.sunp]') === 0.5, 'old pad rows get default spread 0.5');
console.log(bad ? bad + ' FAILED' : 'all ok'); process.exit(bad ? 1 : 0);
