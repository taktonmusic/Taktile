<p align="center">
  <img src="assets/taktile-banner.svg" alt="Taktile – overtake groovebox for Ableton Move" width="100%">
</p>

<p align="center">
  <img alt="Platform" src="https://img.shields.io/badge/platform-Ableton%20Move-333333">
  <img alt="Framework" src="https://img.shields.io/badge/framework-Schwung%20overtake-333333">
  <img alt="Language" src="https://img.shields.io/badge/DSP-C-333333">
  <img alt="Version" src="https://img.shields.io/badge/version-1.0.0-333333">
  <img alt="License" src="https://img.shields.io/badge/license-MIT-333333">
</p>

<p align="center"><a href="../../releases/latest"><b>⬇ Download the latest release</b></a> &nbsp;·&nbsp; <a href="#install">Install</a> &nbsp;·&nbsp; <a href="taktile/docs/Taktile-Manual-EN.pdf">Manual (EN)</a> &nbsp;·&nbsp; <a href="taktile/docs/Taktile-Handbuch-DE.pdf">Handbuch (DE)</a></p>

<p align="center"><b>A groovebox for the Ableton Move: 16 tracks of synths and drums, step sequencer, 704 sounds, effects and live performance tools – a Schwung overtake module.</b></p>

> [!NOTE]
> **Status:** Developed by TaktOnMusic together with a real Ableton Move. Release 1.0.0 itself was checked on a desktop (UI simulator, 60,000-event fuzz test, native builds). Please report anything that behaves differently on the device.

---

## What is Taktile?

Taktile turns the Move into a pattern-based groove box built for jamming live. It takes over the pads, buttons, knobs and display, and all sound comes from its own C DSP (`dsp.so`). You get a complete instrument, not a controller for something else.

- **Play:** synth and drum tracks, 32 pads with scales, repeat, chords and live FX
- **Program:** up to 128 steps per track, parameter locks, probability, ratchets, Euclid rhythms
- **Shape:** 704 sounds (every list starts with INIT), a drum synth with 89 drum sounds, your own samples, delay, reverb, insert effects, granulator
- **Perform:** song mode, 8 clips per track, 32 project slots, undo almost everywhere

## On the display

<p align="center">
  <img src="taktile/docs/display-preview.png" alt="Preview of the Taktile display pages: sound, filter, FX, Euclid, granulator, sound browser and scope views" width="85%">
</p>

<p align="center"><sub>Display pages rendered on a desktop (simulator): sound, filter, FX, Euclid, granulator with buffer view, sound browser and the spectrum, stereo and waterfall scope views.</sub></p>

## Videos and updates

Demos, tutorials and progress updates are on the [TaktOnMusic YouTube channel](https://www.youtube.com/@taktonmusic).

## Features

| | |
|---|---|
| 🎹 **Tracks** | 16 tracks, synth or drum (16 sounds per drum track), up to 128 steps, 8 clips per track, song chain |
| 🔊 **Sounds** | 704 built-in sounds in sorted lists (INIT first in every list), drum synth with lists for kicks, snares, hats, cymbals, claps, toms, rims and percussion, your own WAV samples (64 slots / 40 MB) on drum tracks, recording with the Move's microphone, slicing, 16 WAVs of a folder as drum pads, time-stretch, resampling |
| 🗂️ **Track tiles** | Synth track: 1 PRESET · 2 SYNTH · 3 MOD · 4 FILTER (+ EQ) · 5 FX (+ GRAIN) · 6 FX TYPE (+ FX TIME) · 7 EUCLID · 8 MIDI FX. Drum tracks have no SYNTH tile but a SAMPLE tile. The header shows the tile top left and the page (e.g. 2/2) top right; jog click switches the page |
| 🎛️ **Effects** | Per-track delay, reverb, chorus, phaser, 10 insert effects, 11 MIDI effects (off by default), granulator, sidechain (auto / all / off) |
| 🎚️ **Sequencing** | Per-step parameter locks, per-track swing, Euclid generator (switch, off by default), quantize grid 1/4 … 1/32 with strength, auto-growing clips while recording, MIDI clock + note out (USB-A) |
| ⏱️ **Feel** | Groove templates (MPC, Shuffle, Laidback, Dilla, Drunk, Push, Funk, Ghost, Human, Roll) with amount next to Swing · nudge, humanize, ratchets, song steps |
| 🧪 **Master bus** | Filter, width, saturation, compressor (with detector HPF), 3-band EQ, limiter |
| 💾 **Workflow** | 32 project slots (load, save, rename, new empty, delete), rolling backups, undo history, CPU meter and 11 scope views incl. spectrum, stereo, waterfall (Menu → EXPERT), in-device help (Shift+Menu), sound browser (Shift+jog), grouped menu |

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

**Easiest (no SSH, no unpacking):** download `taktile-module.tar.gz` from the [latest release](../../releases/latest) and upload it as a custom module through Schwung's web interface:

1. Put the Move and your computer (or phone) in the **same Wi-Fi network**.
2. Open **http://move.local:7700** in a browser.
3. Upload the `taktile-module.tar.gz` file as a custom module. Keep the `.tar.gz` as it is, do not unpack it.
4. Restart Schwung / the Move if Taktile does not show up right away.

**Manual alternative:** unpack the archive and copy the `taktile/` folder (`module.json`, `ui.js`, `dsp.so`) to `/data/UserData/schwung/modules/overtake/` on the Move, e.g. `scp -r taktile ableton@move.local:/data/UserData/schwung/modules/overtake/`, then restart Schwung.

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
- Own samples: 64 slots / 40 MB in total. WAV only: 8/16/24/32-bit PCM or 32-bit float
- Resample takes: up to 16 bars
- State is stored under `/data/UserData/schwung/taktile-*`

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

## Controls in short

| | |
|---|---|
| **Jog turn** | walks through the track pages / the list on the current page |
| **Jog click** | open or confirm (SEQ/SOUND: preset browser; menus and lists: the selected entry). On a tile with two pages (FILTER + EQ, FX + GRAIN, FX TYPE + FX TIME, SYNTH + ENGINE 2, MIDI FX + 2): switches the page |
| **Shift + Jog click** | HOME from anywhere (on the TEMPO page: tap tempo) |
| **Left / Right** | previous / next track. With Shift: step page. In lists and name entry: move the selection / cursor |
| **Shift + Step 1…16** | direct jumps: project, HOME, quantize options + pad layout, live FX, tempo, metronome, swing, … (see the manual) |

## Playing long live sets

Taktile is made for sets that run for a long time without stopping the music:

- **Song list** (MORE → SONG): up to 32 entries with scenes, bars, repeats, own tempo and a way in (HARD, FILL, DRUMS 1ST)
- **Background mode**: Back on the top level asks BACKGROUND / CLOSE, the sequencer keeps playing while you use the Move
- **Safety**: 32 project slots, rolling backups, undo history

## Contributing and bug reports

Found a bug or have an idea? Open an [issue](../../issues/new/choose). The template asks for your firmware, Schwung version and the steps to reproduce. Changes are listed in [CHANGELOG.md](CHANGELOG.md).

## Credits and license

- Built on [Schwung](https://github.com/charlesvestal/schwung).
- `src/wtab.inc` contains 640 single-cycle waveforms derived from the [AKWF-FREE](https://github.com/KristofferKarlAxelEkstrand/AKWF-FREE) collection (CC0 1.0). Details in [NOTICE.md](taktile/NOTICE.md).
- MIT License – see [LICENSE](taktile/LICENSE).

## Thanks

A big thank you to **[Neuro... No Neuro](https://www.neurononeuro.net)** (microsound composer, graphic designer and animator) for testing Taktile and for the honest feedback. It made the module better.

Made by **TaktOnMusic**.
