# Taktile

A pattern-based groove box for the **Ableton Move**, written as an *overtake* module for [Schwung](https://github.com/charlesvestal/schwung). It takes over pads, buttons, knobs and display; all sound comes from its own C DSP (`dsp.so`).

> **Status:** Developed and tested together with a real Ableton Move. Release 5.88.0 itself was checked on a desktop (UI simulator, 60,000-event fuzzing, native builds).

![display preview](docs/display-preview.png)

## Features
- 16 tracks, synth or drum (16 sounds per drum track), up to 128 steps, 8 clips per track, song chain
- 1462 built-in sounds, own WAV samples (64 slots / 40 MB) on synth and drum tracks, slicing, kits from 16 WAVs, time-stretch, resampling
- Track tiles (synth track): 1 PRESET, 2 SYNTH, 3 MOD, 4 FILTER (+ EQ), 5 FX (+ GRAIN), 6 FX TYPE (+ FX TIME), 7 EUCLID, 8 MIDI FX, 9 SAMPLE; drum tracks have no SYNTH tile. Tile top left, page top right, jog click = next page
- Per-track delay, reverb, chorus, phaser, 11 insert effects, 11 MIDI effects (off by default), granulator, sidechain (auto / all / off)
- Per-step parameter locks, per-track swing, Euclid generator (off by default), quantize with grid and strength, clips that grow while recording, MIDI clock + note out (USB-A)
- Master: filter, width, saturation, compressor (with detector HPF), 3-band EQ, limiter
- 32 project slots, rolling backups, undo history, in-device help (Shift+Menu), sound/kit folder browser (Shift+jog), grouped menu

## Documentation
- English manual: [docs/Taktile-Manual-EN.pdf](docs/Taktile-Manual-EN.pdf) (includes technical notes and open questions for Schwung developers)
- German manual: [docs/Taktile-Handbuch-DE.pdf](docs/Taktile-Handbuch-DE.pdf)

## Install
Download `taktile-module.tar.gz` from the [latest release](../../../releases/latest). With the Move and your computer or phone on the same Wi-Fi, open `http://move.local:7700` and upload the `.tar.gz` (not unpacked) as a custom module. Restart Schwung / the Move if needed.

Manual alternative: unpack it, copy the `taktile/` folder (`module.json`, `ui.js`, `dsp.so`) to `/data/UserData/schwung/modules/overtake/` and restart Schwung. After building yourself, `scripts/install.sh` copies the build over SSH (`ableton@move.local`, set `DEVICE=user@host` to change it).

Built against Schwung plugin API **v1.7.3** (see the root README, "Compatibility").

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
