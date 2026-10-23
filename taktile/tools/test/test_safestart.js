/* crash guard: a start that never wrote 'ready' makes the next start a safe start */
const { execFileSync } = require('child_process');
let bad = 0; const ok = (c, m) => { console.log((c ? 'ok   ' : 'FAIL ') + m); if (!c) bad++; };
const BL = '/data/UserData/schwung/taktile-boot.log', SF = '/data/UserData/schwung/taktile-state.json';
const probe = `const h=require('./harness.js');h.params.pos='1,0,0,0';h.ctx.init&&h.ctx.init();for(let i=0;i<400;i++)h.ctx.tick();
console.log(JSON.stringify({safe:h.run('typeof safeNote!=="undefined"?safeNote:null'),ready:/ready/.test(h.files['${BL}']||''),bak:!!h.files['/data/UserData/schwung/taktile-state.before-safe-start.json']}));`;
const go = files => JSON.parse(execFileSync('node', ['-e', probe], { cwd: __dirname, env: { ...process.env, PRESET_FILES: JSON.stringify(files) } }).toString().trim().split('\n').pop());
let r = go({ [BL]: 'loaded init tick1 ', [SF]: '{"v":1}' });
ok(r.bak, 'unfinished last start -> project file kept as a copy');
ok(r.ready, 'safe start still reaches ready');
r = go({ [BL]: 'loaded init tick1 ready ', [SF]: '{"v":1}' });
ok(!r.bak, 'finished last start -> normal start, no copy');
r = go({});
ok(r.ready && !r.bak, 'first start works');
console.log(bad ? bad + ' FAILED' : 'all ok'); process.exit(bad ? 1 : 0);
