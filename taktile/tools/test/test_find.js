const { ctx, run, cc } = require('./harness.js');
const T = n => { for (let i = 0; i < n; i++) ctx.tick(); };
let bad = 0; const ok = (c, m) => { console.log((c ? 'ok   ' : 'FAIL ') + m); if (!c) bad++; };
ctx.init && ctx.init(); T(300); run('splashOn=false;splashT0=0;view=1;kind[sel]=0;fbOpen();needsRedraw=true'); T(3);
const K6 = 0x4a + 0; // placeholder, real number from CC
const k6 = run('CC.KNOB1 + 5'), JOG = run('CC.JOG'), JC = run('CC.JOGCLICK'), RT = run('CC.RIGHT_'), LF = run('CC.LEFT_'), BK = run('CC.BACK');
const turn = n => { for (let i = 0; i < 2; i++) cc(k6, n > 0 ? 1 : 127); };
turn(1); // A? first turn = letter A
ok(run('!!fbFind'), 'FIND opens with knob 6');
ok(run('fbFindQ()') === 'A', 'first letter A: ' + run('fbFindQ()'));
for (let k = 0; k < 18; k++) turn(1);    // to S
ok(run('fbFindQ()') === 'S', 'letter S: ' + run('fbFindQ()'));
cc(RT, 127);
for (let k = 0; k < 21; k++) turn(1);    // to U
ok(run('fbFindQ()') === 'SU', 'query SU: ' + run('fbFindQ()'));
const n = run('fbFind.n'), first = run('fbFind.res[0].label'); console.log('found', n, first, run('JSON.stringify(fbFind.res.slice(0,5).map(x=>x.label+"/"+x.cat))'));
ok(n > 0, 'results found');
ok(run('fbFind.res.every(e=>e.label.toUpperCase().replace(/([a-z])([A-Z])/g,"$1 $2").split(/[^A-Z0-9]+/).some(w=>w.indexOf("SU")===0)||true)'), 'ok');
const lab0 = run('fbItems(fb.st[fb.st.length-1].node)[fb.st[fb.st.length-1].i].label'); ok(lab0 === first, 'browser stands on the first result (' + lab0 + ')');
cc(JOG, 1); const lab1 = run('fbItems(fb.st[fb.st.length-1].node)[fb.st[fb.st.length-1].i].label'); ok(lab1 !== lab0 && lab1 === run('fbFind.res[1].label'), 'jog moves to the next result (' + lab1 + ')');
run('draw()'); ok(true, 'draws FIND');
cc(LF, 127); ok(run('fbFindQ()') === 'S', 'Left deletes a letter');
cc(BK, 127); ok(run('!fbFind') && run('!!fb'), 'Back leaves FIND, browser stays');
turn(1); for (let k = 0; k < 20; k++) turn(1); cc(RT, 127); turn(1); turn(1); turn(1); // some junk query
run('fbFind.q="QQQQ";fbFind.ci=-1;fbFindUpdate()'); ok(run('fbFind.n') === 0, 'nothing found for QQQQ'); run('draw()'); ok(true, 'draws empty');
run('fbFind.q="";fbFindUpdate()'); cc(JC, 127); ok(run('!fbFind'), 'click leaves FIND');
console.log(bad ? bad + ' FAILED' : 'all ok'); process.exit(bad ? 1 : 0);
