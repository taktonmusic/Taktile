<p align="center">
  <img src="assets/banner.svg" alt="Taktile – overtake groovebox for Ableton Move" width="100%">
</p>

<p align="center">
  <img alt="Platform" src="https://img.shields.io/badge/platform-Ableton%20Move-ff4d6d">
  <img alt="Framework" src="https://img.shields.io/badge/framework-Schwung%20overtake-8d6bff">
  <img alt="Language" src="https://img.shields.io/badge/DSP-C-2ec4b6">
  <img alt="Version" src="https://img.shields.io/badge/version-5.4.1-ffb703">
  <img alt="License" src="https://img.shields.io/badge/license-MIT-lightgrey">
</p>

<p align="center"><b>16 tracks of synths and drums, a step sequencer, sounds, effects and live performance tools – all running as a Schwung overtake module on the Ableton Move.</b></p>

> [!NOTE]
> **Status:** Tested on a real Ableton Move. UI logic and DSP are also checked on a desktop (UI simulator, 60,000-event fuzzing, native ASAN/UBSAN runs).

---

## What is Taktile?

Taktile is a pattern-based groove box built for jamming live. It takes over the Move's pads, buttons, knobs and display. All sound comes from its own C DSP (`dsp.so`), so you get a complete instrument rather than a controller for something else.

## On the display

<p align="center">
  <img src="taktile/docs/display-preview.png" alt="Preview of the Taktile display pages: sound, filter, mod, FX, master bus, EQ, kit, step and options" width="85%">
</p>

<p align="center"><sub>Display pages rendered on a desktop (simulator): sound, filter, mod, FX, master bus, master EQ, kit, step parameters and options.</sub></p>

## Features

| | |
|---|---|
| 🎹 **Tracks** | 16 tracks, synth or drum (16 sounds per drum track), up to 128 steps, 8 clips per track, song chain |
| 🔊 **Sounds** | 1210 built-in sounds, your own WAV samples (64 slots / 40 MB), slicing, kits from 16 WAVs, time-stretch, resampling |
| 🗂️ **Track pages** | Tabs on a synth track: SYNTH → SOUND → FILTER → MOD → FX → FX 2 → EUCLID → MIDI FX. Drum tracks and sample sounds get SAMPLE as the last tab instead of SYNTH |
| 🎛️ **Effects** | Per-track delay, reverb, chorus, phaser, 10 insert effects, 10 MIDI effects, sidechain (auto / all / off) |
| 🎚️ **Sequencing** | Per-step parameter locks, per-track swing, Euclid generator, MIDI clock + note out (USB-A) |
| 🧪 **Master bus** | Filter, width, saturation, compressor (with detector HPF), 3-band EQ, limiter |
| 💾 **Workflow** | 8 project slots, rolling backups, undo history, in-device help (Shift+Menu), sound/kit folder browser (Shift+jog), grouped menu |

## Signal flow

<p align="center">
  <img src="assets/signal-flow.svg" alt="Signal flow: sequencer, track, insert FX, send FX, master" width="100%">
</p>

## Documentation

| Language | Manual |
|---|---|
| 🇬🇧 English | [Taktile-Manual-EN.pdf](taktile/docs/Taktile-Manual-EN.pdf) – includes technical notes and open questions for Schwung developers |
| 🇦🇹 Deutsch | [Taktile-Handbuch-DE.pdf](taktile/docs/Taktile-Handbuch-DE.pdf) |

## Install

**Easiest:** download `taktile-module.tar.gz` from the [latest release](../../releases/latest) and unpack it. You get a `taktile/` folder (`module.json`, `ui.js`, `dsp.so`).

1. Copy that `taktile/` folder to `/data/UserData/schwung/modules/overtake/` on the Move (e.g. with `scp -r taktile ableton@move.local:/data/UserData/schwung/modules/overtake/`).
2. Restart Schwung / the Move.

Building it yourself: see below. After a build, `taktile/scripts/install.sh` does the copy over SSH (`ableton@move.local`, set `DEVICE=user@host` to change it).

## Compatibility

| | |
|---|---|
| Module format | Schwung overtake module, `api_version` 2 |
| Built against | Schwung plugin API header from tag **v1.7.3** (`plugin_api_v1.h`, pinned in `fetch-header.sh`) |
| Move firmware | Not recorded yet. When you report a problem, please name your firmware and Schwung version |

If your Schwung is a different version, build with `SCHWUNG_REF=<tag> ./scripts/fetch-header.sh` first. The header has to match the host exactly.

## Limits

- Up to 16 tracks, max. 4 of them drum tracks (16 sounds each), up to 128 steps per track, 8 clips per track
- Own samples: 64 slots / 40 MB in total. WAV only: 8/16/24/32-bit PCM or 32-bit float, 8 kHz to 192 kHz; mono and stereo (more channels are reduced to stereo)
- State is stored under `/data/UserData/schwung/taktile-*`. A project from an unreadable file is rejected with `CORRUPT` and your current state stays untouched

## Troubleshooting

If something goes wrong, Taktile writes logs to `/data/UserData/schwung/`: `taktile-error.log` (crash in a handler) and `taktile-boot.log` (start-up). Attach them to a bug report.

## Build

Needs `gcc-aarch64-linux-gnu`.

```bash
cd taktile
./scripts/build.sh      # fetches Schwung's plugin_api_v1.h (v1.7.3), builds dist/taktile-module.tar.gz
```

### Desktop test of the DSP

```bash
cd taktile
./scripts/fetch-header.sh
gcc -g -fsanitize=address,undefined -Isrc -Ibuild/include your_test.c src/pad_studio.c -lm
```

## Project layout

```
taktile/
├── module.json          Schwung manifest (overtake, api_version 2)
├── ui.js                all UI: views, pads, display, state files
├── src/pad_studio.c     DSP, sequencer, sample engine
├── scripts/             build.sh, install.sh, fetch-header.sh
├── tools/               generators for the sound variant tables
└── docs/                manuals (EN/DE) and display preview
```

State is stored under `/data/UserData/schwung/taktile-*`.

## Contributing and bug reports

Found a bug or have an idea? Open an [issue](../../issues/new/choose). The template asks for your firmware, Schwung version and the steps to reproduce. Changes are listed in [CHANGELOG.md](CHANGELOG.md).

## Credits and license

- Built on [Schwung](https://github.com/charlesvestal/schwung).
- `src/wtab.inc` contains 640 single-cycle waveforms derived from the [AKWF-FREE](https://github.com/KristofferKarlAxelEkstrand/AKWF-FREE) collection (CC0 1.0). Details in [NOTICE.md](taktile/NOTICE.md).
- MIT License – see [LICENSE](taktile/LICENSE).

Made by **TaktOnMusic**.
