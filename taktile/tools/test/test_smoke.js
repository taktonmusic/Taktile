// Smoke + fuzz test: loads ui.js with a mocked Schwung host, sends 40000 random button / pad / knob events,
// and fails if any handler error was logged.
const { ctx, cc, writes } = require('./harness.js');
let failed = 0; const ok = (c, m) => { console.log((c ? 'ok   ' : 'FAIL ') + m); if (!c) failed++; };
ctx.init && ctx.init();
let seed = 12345; const rnd = () => (seed = (seed * 1103515245 + 12345) & 0x7fffffff) / 0x7fffffff;
const ccs = [49, 3, 62, 63, 55, 54, 60, 119, 88, 85, 86, 51, 58, 50, 56, 118, 43, 42, 41, 40, 79, 52, 71, 72, 73, 74, 75, 76, 77, 78, 14];
for (let i = 0; i < 40000; i++) {
  const r = rnd();
  if (r < 0.15) ctx.onMidiMessageInternal([0x90, 68 + Math.floor(rnd() * 32), rnd() < 0.5 ? 100 : 0]);
  else if (r < 0.3) ctx.onMidiMessageInternal([0x90, 16 + Math.floor(rnd() * 16), rnd() < 0.5 ? 100 : 0]);
  else cc(ccs[Math.floor(rnd() * ccs.length)], rnd() < 0.5 ? 127 : (rnd() < 0.5 ? 1 : 0));
  if (i % 50 === 0) ctx.tick();
}
ok(writes.length === 0, 'fuzz 40000 events: no handler error logged' + (writes.length ? ' -> ' + String(writes[0]).slice(0, 200) : ''));
if (failed) { console.log('FAILED ' + failed); process.exit(1); }
console.log('all ok');
