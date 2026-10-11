const { ctx, run, params, setCalls } = require('./harness.js');
ctx.init(); for (let i = 0; i < 400; i++) ctx.tick();
let fail = 0; const ok = (c, m) => { if (!c) { fail++; console.log('FAIL', m); } else console.log('ok  ', m); };
const armed = () => setCalls.filter((c) => c[0] === 'recarm').map((c) => c[1]);
const reset = () => { run('rsState = 0; askKind = -1;'); setCalls.length = 0; };

reset(); params.recmaxbars = '16'; params.smpmem = '10'; run('rsBarI = 4; rsSrc = 0; bpm = 120; rsArm()');
ok(armed().length === 1 && /,16$/.test(armed()[0]), '120 BPM: 16 bars are allowed (' + armed() + ')');

reset(); params.recmaxbars = '10'; run('rsBarI = 4; rsSrc = 0; bpm = 60; rsArm()');
ok(armed().length === 1 && /,8$/.test(armed()[0]) && run('rsBarI') === 3, '60 BPM: 16 bars are cut to 8 (' + armed() + ')');

reset(); params.recmaxbars = '1'; run('rsBarI = 2; rsSrc = 0; bpm = 40; rsArm()');
ok(armed().length === 1 && /,1$/.test(armed()[0]), 'very slow: never below 1 bar (' + armed() + ')');

reset(); params.recmaxbars = '16'; params.smpmem = '99'; run('rsBarI = 4; rsSrc = 0; bpm = 120; rsArm()');
ok(armed().length === 0 && run('rsState') === 0, 'bank 99 % full: recording refused');

reset(); params.smpmem = '97'; run('rsBarI = 0; rsSrc = 0; bpm = 120; rsArm()');
ok(armed().length === 1, 'bank 97 % full, 1 bar (~0.17 MB) still fits');

console.log(fail ? 'FAILED ' + fail : 'ALL PASSED'); process.exit(fail ? 1 : 0);
