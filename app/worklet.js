/* Plays the audio that the page renders. It asks for more whenever less than LOW frames are waiting. */
class TaktileOut extends AudioWorkletProcessor {
  constructor() {
    super();
    this.q = []; this.cur = null; this.pos = 0; this.avail = 0; this.low = 1024; this.pending = true;
    this.port.onmessage = (e) => {
      const d = e.data;
      if (d && d.chunk) { this.q.push(d.chunk); this.avail += d.chunk.length >> 1; this.pending = false; }
    };
    this.port.postMessage({ need: 1 });
  }
  process(inputs, outputs) {
    const o = outputs[0], L = o[0], R = o[1] || o[0], n = L.length;
    for (let i = 0; i < n; i++) {
      if (!this.cur || this.pos >= (this.cur.length >> 1)) { this.cur = this.q.length ? this.q.shift() : null; this.pos = 0; }
      if (this.cur) { L[i] = this.cur[this.pos * 2]; R[i] = this.cur[this.pos * 2 + 1]; this.pos++; this.avail--; }
      else { L[i] = 0; R[i] = 0; }
    }
    if (!this.pending && this.avail < this.low) { this.pending = true; this.port.postMessage({ need: 1 }); }
    return true;
  }
}
registerProcessor('taktile-out', TaktileOut);
