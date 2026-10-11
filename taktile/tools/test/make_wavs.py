"""Writes the small test WAVs used by test_dsp.c (stereo 8/16/24/float, mono, 6 channels)."""
import struct, sys, os
out = sys.argv[1]; os.makedirs(out, exist_ok=True)
n, sr = 4410, 44100
def wav(name, ch, bits, fmt, frames):
    bps = bits // 8; data = b''
    for i in range(n):
        for c in range(ch):
            v = frames(i, c)
            if fmt == 3: data += struct.pack('<f', v)
            elif bits == 8: data += bytes([int(v * 127) + 128])
            elif bits == 16: data += struct.pack('<h', int(v * 32767))
            elif bits == 24: data += struct.pack('<i', int(v * 8388607))[:3]
    hdr = b'RIFF' + struct.pack('<I', 36 + len(data)) + b'WAVE' + b'fmt ' + struct.pack('<IHHIIHH', 16, fmt, ch, sr, sr * ch * bps, ch * bps, bits) + b'data' + struct.pack('<I', len(data))
    open(os.path.join(out, name), 'wb').write(hdr + data)
lr = lambda i, c: 0.5 if c == 0 else -0.1          # left 0.5, right -0.1 -> mono mean 0.2
wav('st16.wav', 2, 16, 1, lr); wav('st24.wav', 2, 24, 1, lr); wav('stf32.wav', 2, 32, 3, lr)
wav('mono16.wav', 1, 16, 1, lambda i, c: 0.25); wav('u8.wav', 2, 8, 1, lr)
wav('six.wav', 6, 16, 1, lambda i, c: 0.5 if c == 0 else (-0.1 if c == 1 else 0.9))
