/* shortened preset list: only the kept sounds are listed, removed ones of old projects map to a similar kept sound of the same synth */
const { ctx, run } = require('./harness.js');
let bad = 0; const ok = (c, m) => { console.log((c ? 'ok   ' : 'FAIL ') + m); if (!c) bad++; };
const keep = run('TYPE_KEEP.filter(Boolean).length'), all = run('TYPE_NAME.length');
ok(keep > 300 && keep < 800 && all === run('TYPE_MAP.length'), 'kept ' + keep + ' of ' + all + ' sounds');
ok(run('TYPE_MAP.every((m, i) => TYPE_MAP[m] === m && BASE_T[m] === BASE_T[i])'), 'every removed sound maps to a kept sound of the same synth');
ok(run('[...new Set(BASE_T)].every(b => TYPE_KEEP[b])'), 'the first sound of every synth stays');
ok(run('[0,1,2,3,4,5,6,7].every(c => catList(c).every(t => TYPE_KEEP[t]))'), 'the browser lists only kept sounds');
ok(run("TYPE_KEEP[TYPE_NAME.indexOf('FmInit')]"), 'FM INIT stays');
ok(run('INIT_DRUMS.every(n => TYPE_KEEP[TYPE_NAME.indexOf(n)])'), 'the INIT drum kit stays');
const r = run('TYPE_MAP.findIndex((m, i) => m !== i)');
ok(run('mapT(' + r + ') === TYPE_MAP[' + r + ']'), 'mapT maps a removed sound');
console.log(bad ? bad + ' FAILED' : 'all ok'); process.exit(bad ? 1 : 0);
