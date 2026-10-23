# Third-party content

`src/wtab.inc` contains 640 single-cycle waveforms derived (resampled to 256 samples, normalised) from the
**AKWF-FREE** collection (Adventure Kid Waveforms, https://github.com/KristofferKarlAxelEkstrand/AKWF-FREE),
released under **CC0 1.0 Universal** (public domain dedication).

## Engines written after open-source designs

The three newest engines (`src/newsynth.inc`) are written in plain C for Taktile, after these open-source projects:

- **ACID 303** - the TB-303 filter model (diode ladder with feedback high-pass and the measured cutoff / resonance curves) from **Open303** by Robin Schmidt (rs-met.com), MIT licence, Copyright (c) 2009 Robin Schmidt.
- **SWARM** - the swarm of detuned saws with random grain envelopes from **Mutable Instruments Plaits** by Emilie Gillet, MIT licence, Copyright 2016 Emilie Gillet.
- **DX 6-OP FM** - the 32 operator routings (algorithms) of the 6-operator FM core of **music-synthesizer-for-android (msfa)**, Copyright Google Inc., Apache License 2.0 (http://www.apache.org/licenses/LICENSE-2.0). The sound patches are Taktile's own.

MIT licence: permission is granted to use, copy, modify, merge, publish, distribute, sublicense and sell copies of the software, provided the copyright notice and this permission notice are included; the software is provided "as is", without warranty of any kind.
