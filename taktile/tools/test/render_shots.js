// Renders display pages and the pad LEDs of the mocked UI to JSON (for preview pictures). Usage: node render_shots.js out.json
const vm = require('vm'), fs = require('fs'), path = require('path');
let ops = [];
const ctx = vm.createContext({ console, Math, JSON, Date, Array, Object, String, Number, parseInt, parseFloat, isNaN, Float32Array, Int16Array, Uint8Array, setTimeout, clearTimeout });
ctx.globalThis = ctx; const led = {};
Object.assign(ctx, {
  host_read_file: () => null, host_write_file() {}, host_module_set_param() {}, host_module_get_param: () => '',
  fill_rect: (x, y, w, h, c) => ops.push(['f', x, y, w, h, c]), draw_rect: (x, y, w, h, c) => ops.push(['r', x, y, w, h, c]), set_pixel: (x, y, c) => ops.push(['p', x, y, c]),
  clear_screen: () => { ops = []; }, print: (x, y, s, c) => ops.push(['t', x, y, String(s), c]), text_width: (s) => String(s).length * 6,
  move_midi_internal_send: (m) => { if (m[0] === 0x09) led[m[2]] = m[3]; }, move_midi_external_send() {}, host_suspend_overtake() {}, host_return_to_menu() {}, host_exit_module() {},
  shadow_set_overtake_suppress_master_volume() {}, shadow_inbound_pad_midi_active: () => false,
});
new vm.Script(fs.readFileSync(path.join(__dirname, '../../ui.js'), 'utf8'), { filename: 'ui.js' }).runInContext(ctx);
const run = (c) => new vm.Script(c).runInContext(ctx);
ctx.init(); for (let i = 0; i < 400; i++) ctx.tick();
run('splashT0 = 0; idleBlank = false; askKind = -1; menuOpen = false; helpOpen = false; shiftHeld = false');
const out = { screens: {}, pads: {} };
const RS = 'chosen.fill(true); askKind = -1; exitDlg = -1; fb = null; menuOpen = false; '; const shot = (name, setup) => { run(RS + setup); run(RS); for (let i = 0; i < 3; i++) { ops = []; run('needsRedraw = true'); ctx.tick(); } ops = []; run('draw()'); out.screens[name] = ops.slice(); };
const padShot = (name, setup) => { run(RS + setup); run(RS); for (let i = 0; i < 40; i++) { run('ledSent = {}'); ctx.tick(); } out.pads[name] = { led: Object.assign({}, led), tc: run('trkCol(sel)'), drum: run('kind[sel]'), fx: run('Array.from({length:32}, (_, i) => fxIdxOf(i))'), names: run('FX_PADS.map(x => x[0])') }; };
shot('sound_fm', 'setKind(0,0); sel=0; setEngine(0,2); view=1; viewItemsFix()');
shot('synth_fm', 'view=9; viewItemsFix()');
shot('fx', 'view=38; viewItemsFix(); pads[0][PI.dly]=0.3; pads[0][PI.rvb]=0.4');
shot('midi_chord', 'view=27; viewItemsFix(); pads[0][PI.mfx]=2; pads[0][PI.mamt]=0.7; pads[0][PI.mprm]=0.25');
shot('seq_fxpads', 'view=0; topModes[0]=3; viewItemsFix()');
padShot('synth', 'view=0; topModes[0]=3; setKind(0,0); sel=0');
padShot('drum', 'setKind(0,1); sel=0; topModes[0]=3');
fs.writeFileSync(process.argv[2], JSON.stringify(out));
