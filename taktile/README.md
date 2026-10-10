# Taktile

A pattern-based groove box for the **Ableton Move**, written as an *overtake* module for [Schwung](https://github.com/charlesvestal/schwung). It takes over pads, buttons, knobs and display; all sound comes from its own C DSP (`dsp.so`).

> **Status:** Tested on a real Ableton Move. UI logic and DSP are also checked on a desktop (UI simulator, 60,000-event fuzzing, native ASAN/UBSAN runs).

![display preview](docs/display-preview.png)

## Features
- 16 tracks, synth or drum (16 sounds per drum track), up to 128 steps, 8 clips per track, song chain
- 1210 built-in sounds, own WAV samples (64 slots / 40 MB), slicing, kits from 16 WAVs, time-stretch, resampling
- Track pages as tabs (synth track): SYNTH, SOUND, FILTER, MOD, FX, FX 2, EUCLID, MIDI FX (drum tracks and sample sounds: SAMPLE last instead of SYNTH)
- Per-track delay, reverb, chorus, phaser, 10 insert effects, 10 MIDI effects, sidechain (auto / all / off)
- Per-step parameter locks, per-track swing, Euclid generator, MIDI clock + note out (USB-A)
- Master: filter, width, saturation, compressor (with detector HPF), 3-band EQ, limiter
- 8 project slots, rolling backups, undo history, in-device help (Shift+Menu), sound/kit folder browser (Shift+jog), grouped menu

## Documentation
- English manual: [docs/Taktile-Manual-EN.pdf](docs/Taktile-Manual-EN.pdf) (includes technical notes and open questions for Schwung developers)
- German manual: [docs/Taktile-Handbuch-DE.pdf](docs/Taktile-Handbuch-DE.pdf)

## Install
Build (below) or download the `taktile-module` artifact from CI, then copy the `taktile/` folder (`module.json`, `ui.js`, `dsp.so`) to the Move's Schwung modules directory. `scripts/install.sh` does this over SSH (`ableton@move.local`, into `/data/UserData/schwung/modules/overtake/taktile/`).

## Build
Needs `gcc-aarch64-linux-gnu`.

    ./scripts/build.sh      # fetches Schwung's plugin_api_v1.h, builds dist/taktile-module.tar.gz

## Desktop test of the DSP

    ./scripts/fetch-header.sh
    gcc -g -fsanitize=address,undefined -Isrc -Ibuild/include your_test.c src/pad_studio.c -lm

## Layout
- `module.json` – Schwung manifest (overtake, api_version 2; uses `suspend_keeps_js`, `suspend_self_managed`, `claims_master_knob`)
- `ui.js` – all UI (views, pads, display, state files)
- `src/pad_studio.c` – DSP, sequencer, sample engine
- State is stored under `/data/UserData/schwung/taktile-*`

## License
MIT – see [LICENSE](LICENSE). Author: TaktOnMusic.
