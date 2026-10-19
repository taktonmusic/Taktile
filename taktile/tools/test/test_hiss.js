/* an old save (no hv flag) with Hiss 60% loads quieter; a new save keeps its value */
const { ctx, run, files, params } = require('./harness.js');
const T = n => { for (let i = 0; i < n; i++) ctx.tick(); };
let bad = 0; const ok = (c, m) => { console.log((c ? 'ok   ' : 'FAIL ') + m); if (!c) bad++; };
params.pos = '0,0,0,0'; ctx.init && ctx.init(); T(300);
run('splashOn=false;splashT0=0');
run('master[MI.m_hiss]=0.6;dirty=true;saveState()'); T(5);
const key = Object.keys(files).find(k => /state/i.test(k) && files[k] && files[k].includes('"master"'));
ok(!!key, 'state file written');
const st = JSON.parse(files[key]); ok(st.hv === 2, 'new saves carry the hiss flag');
st.master[run('MI.m_hiss')] = 0.6; files[key] = JSON.stringify(st); run('loadState()');
ok(Math.abs(run('master[MI.m_hiss]') - 0.6) < 1e-6, 'a new save keeps Hiss 60%');
delete st.hv; files[key] = JSON.stringify(st); run('loadState()');
const h = run('master[MI.m_hiss]'); ok(h < 0.3 && h > 0.1, 'an old save is lowered (60% -> ' + h.toFixed(2) + ')');
console.log(bad ? bad + ' FAILED' : 'all ok'); process.exit(bad ? 1 : 0);
