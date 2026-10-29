const { ctx, run, files } = require('./harness.js');
const T = n => { for (let i = 0; i < n; i++) ctx.tick(); };
let bad = 0; const ok = (c, m) => { console.log((c ? 'ok   ' : 'FAIL ') + m); if (!c) bad++; };
ctx.init && ctx.init(); T(300);
run('splashOn=false;splashT0=0');
const W = (f, t) => { files[f] = t; };
ok(run('typeof restoreBackup') === 'function', 'restore exists');
const D = '/data/UserData/schwung/taktile-' + (/taktile-test/.test(process.argv[2] || '') ? 'test-' : '');
const st = JSON.parse(run('(function(){saveState();return host_read_file(STATE_FILE)})()'));
for (let i = 0; i < 4; i++) { const o = Object.assign({}, st, { bpm: 100 + i }); files[D + 'backup-' + i + '.json'] = JSON.stringify(o); }
files[D + 'backup.idx'] = '4';
run('restoreBackup()'); T(3); ok(run('bpm') === 103, 'first revert = newest backup (bpm ' + run('bpm') + ')');
run('restoreBackup()'); T(3); ok(run('bpm') === 102, 'second revert = one older (bpm ' + run('bpm') + ')');
run('restoreBackup()'); T(3); ok(run('bpm') === 101, 'third = older again');
run('tres[0]=3;'); const m = JSON.parse(run('miscGet()')); ok(m.tr[0] === 3, 'grid is in undo');
run('draw()'); ok(true, 'draws');
console.log(bad ? bad + ' FAILED' : 'all ok'); process.exit(bad ? 1 : 0);
