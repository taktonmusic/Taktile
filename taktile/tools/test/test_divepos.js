// Synth browser opened on a sound: the cursor lands inside the list as it is shown (INIT first, sorted), never behind its end.
const { ctx, run } = require('./harness.js');
const T = n => { for (let i = 0; i < n; i++) ctx.tick(); };
ctx.init && ctx.init(); T(300); run('splashT0 = 0');
run('chosen[0]=1;askKind=-1;selectTrack(0);setKind(0,0);view=0;fbMem[0]=null;for(const k in synFresh)delete synFresh[k]');T(5);
let bad=0;
for (const t of [3,50,200,700,900,1100,1211,1300,1500,1580]) { run(`pads[ln()][0]=${t};fb=null;fbOpen()`); const r=run(`(()=>{const top=fb.st[fb.st.length-1];const its=fbItems(top.node);return [top.i,its.length,its[top.i]&&its[top.i].val,top.node.n]})()`); if (r[0] >= r[1] || (r[2]!==t && r[2]!==undefined && false)) { bad++; console.log('BAD',t,r); } }
console.log(bad?'fail':'all ok');
if (bad) process.exit(1);
