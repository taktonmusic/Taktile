const { ctx, run } = require('./harness.js');
const T = n => { for (let i = 0; i < n; i++) ctx.tick(); };
let bad = 0; const ok = (c, m) => { console.log((c ? 'ok   ' : 'FAIL ') + m); if (!c) bad++; };
ctx.init && ctx.init(); T(300); run('splashOn=false;splashT0=0;view=0');
run('loopHeld=true;loopT=Date.now()'); T(3); ok(run('loopHeld') === true, 'held stays while fresh');
run('loopT=Date.now()-5000'); T(3); ok(run('loopHeld') === false, 'stuck loop lets go');
run('loopHeld=true;loopT=Date.now();setView(31)'); ok(run('loopHeld') === false, 'page change lets go');
run('view=0;loopHeld=true;loopT=Date.now()-3500;onStep(0,true);onStep(0,false)'); T(2); ok(run('loopHeld') === true, 'using it keeps it held');
console.log(bad ? bad + ' FAILED' : 'all ok'); process.exit(bad ? 1 : 0);
