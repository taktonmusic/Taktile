const { ctx, run, cc } = require('./harness.js');
const T = n => { for (let i = 0; i < n; i++) ctx.tick(); };
let bad = 0; const ok = (c, m) => { console.log((c ? 'ok   ' : 'FAIL ') + m); if (!c) bad++; };
ctx.init && ctx.init(); T(300); run('splashOn=false;splashT0=0;view=1;kind[sel]=0;fbOpen();needsRedraw=true'); T(3);
// go into a list with several letters
run('fb.st[fb.st.length-1].i=0'); run('(function(){let g=0;while(g++<6){const t=fb.st[fb.st.length-1];const its=fbItems(t.node);if(!its.length||!its[t.i].kid)break;fbClick();}})()');
const names = JSON.parse(run('JSON.stringify(fbItems(fb.st[fb.st.length-1].node).map(x=>x.label))'));
console.log(names.length, names.slice(0, 12).join(' '));
run('fb.st[fb.st.length-1].i=0'); const L0 = names[0][0].toUpperCase();
run('fbLetter(1)'); const i1 = run('fb.st[fb.st.length-1].i'); ok(names[i1][0].toUpperCase() !== L0 || names.every(n => n[0].toUpperCase() === L0), 'forward goes to the next letter (' + names[i1] + ')');
run('fbLetter(-1)'); const i2 = run('fb.st[fb.st.length-1].i'); ok(i2 <= i1, 'back goes to the previous letter (' + names[i2] + ')');
ok(JSON.parse(run('JSON.stringify(HUBS[16][1].map(x=>x[0]))')).indexOf('SONG') < JSON.parse(run('JSON.stringify(HUBS[16][1].map(x=>x[0]))')).indexOf('TEMPO'), 'SONG before TEMPO');
console.log(JSON.parse(run('JSON.stringify(HUBS[16][1].map(x=>x[0]))')).join(' '));
run('draw()'); ok(true, 'draws');
console.log(bad ? bad + ' FAILED' : 'all ok'); process.exit(bad ? 1 : 0);
