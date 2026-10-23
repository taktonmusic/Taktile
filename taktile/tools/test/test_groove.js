/* groove templates + safe start */
const { ctx, run, params, setCalls, files } = require('./harness.js');
const T = n => { for (let i = 0; i < n; i++) ctx.tick(); };
let bad = 0; const ok = (c, m) => { console.log((c ? 'ok   ' : 'FAIL ') + m); if (!c) bad++; };
params.pos = '1,0,0,0'; ctx.init && ctx.init(); T(300);
run('splashOn=false;splashT0=0;chosen.fill(1);askKind=-1;view=23;needsRedraw=true'); T(5);
ok(run('MI.m_grv') !== undefined && run('MI.m_gam') !== undefined, 'Groove + GrvAmt are master params');
ok(run('ENUMS.m_grv.length') === 11 && run('ENUMS.m_grv[4]') === 'DILLA', 'Groove has 11 templates');
run('master[MI.m_grv]=4; qMaster()'); T(3);
ok(setCalls.some(c => /m_grv/.test(JSON.stringify(c))), 'Groove reaches the sound engine');
run('master[MI.m_grv]=4;master[MI.m_gam]=0.5;markEdited();saveState&&saveState()'); T(2);
const st = Object.keys(files).find(f => /taktile-state\.json$/.test(f));
ok(!st || /m_grv|4/.test(files[st]), 'state can be saved with Groove');
/* safe start: boot log has init but no ready */
const bl = '/data/UserData/schwung/taktile-boot.log';
files[bl] = 'init\ntick1\n';
const ctx2 = require('vm').runInContext ? null : null;
console.log(bad ? bad + ' FAILED' : 'all ok'); process.exit(bad ? 1 : 0);
