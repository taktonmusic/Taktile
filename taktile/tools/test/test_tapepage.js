/* the TAPE page: 8 knobs (Tape, Hiss, Wow, Flutter, Age, Dropout, Tone, Vol), saved with the project and sent to the sound engine */
const { ctx, run, params, setCalls } = require('./harness.js');
let bad = 0; const ok = (c, m) => { console.log((c ? 'ok   ' : 'FAIL ') + m); if (!c) bad++; };
const T = n => { for (let i = 0; i < n; i++) ctx.tick(); };
ctx.init && ctx.init(); T(300); run('splashT0 = 0'); run('askKind = -1; view = 0'); T(5);
run('setView(52)'); T(5);
ok(run('VIEW_ITEMS[52].length') === 8, 'the TAPE page has 8 knobs');
ok(run('VIEW_ITEMS[52].map(x => x[0]).join()') === 'm_tape,m_hiss,m_wow,m_tflt,m_tage,m_tdrp,m_tton,m_vol', 'in the order Tape, Hiss, Wow, Flutter, Age, Dropout, Tone, Vol');
ok(run('MASTER_DEF.length') === run('MK.length'), 'one default for every master key (' + run('MK.length') + ')');
ok(run('master[MI.m_tton]') === 0.5 && run('master[MI.m_tage]') === 0, 'defaults: Tone in the middle, Age 0');
for (let i = 0; i < 12; i++) ctx.onMidiMessageInternal([0xB0, 71 + 4, 1]);   /* knob 5 = Age */
T(120);
ok(run('master[MI.m_tage]') > 0.01, 'knob 5 turns Age (' + run('master[MI.m_tage]') + ')');
ok(run('JSON.stringify(master).length') > 0 && run('master.length') === run('MK.length'), 'the master list has the new entries');
ok(setCalls.some(c => c[0] === 'm_tage' && Number(c[1]) > 0), 'the new Age value is sent to the sound engine');
console.log(bad ? bad + ' FAILED' : 'all ok'); process.exit(bad ? 1 : 0);
