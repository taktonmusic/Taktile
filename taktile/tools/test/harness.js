const vm = require('vm'), fs = require('fs');
const files = {}; const writes = []; const params = {}; const setCalls = [];
const ctx = vm.createContext({ console, Math, JSON, Date, Array, Object, String, Number, parseInt, parseFloat, isNaN, Float32Array, Int16Array, Uint8Array, setTimeout, clearTimeout });
ctx.globalThis = ctx;
Object.assign(ctx, {
  host_read_file: (f) => (f in files ? files[f] : null), host_write_file: (f, t) => { files[f] = t; if (/error\.log/.test(f)) writes.push(t); },
  host_module_set_param: (k, v) => { setCalls.push([k, v]); }, host_module_get_param: (k) => (k in params ? params[k] : ''), fill_rect() {}, draw_rect() {}, set_pixel() {}, clear_screen() {}, print() {}, text_width: (s) => String(s).length * 5,
  move_midi_internal_send() {}, move_midi_external_send() {}, host_suspend_overtake() {}, host_return_to_menu() {}, host_exit_module() {}, shadow_set_overtake_suppress_master_volume() {}, shadow_inbound_pad_midi_active: () => false,
});
new vm.Script(fs.readFileSync(process.argv[2] || require('path').join(__dirname, '../../ui.js'), 'utf8'), { filename: 'ui.js' }).runInContext(ctx);
const run = (code) => new vm.Script(code).runInContext(ctx);
const cc = (n, v) => ctx.onMidiMessageInternal([0xB0, n, v]);
module.exports = { ctx, run, cc, writes, files, params, setCalls };
