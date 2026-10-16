# Changelog

## 5.80.0 (16 October 2026)
- SYNTH tile page 2: Chop is back for the sound engines; classic synths show Drive in its place (and the waveform window on the PRESET tile)

## 5.79.3 (16 October 2026)
- Sound engines: Drive is back on the PRESET tile (the waveform window stays on classic synths only)

## 5.79.2 (16 October 2026)
- The waveform window on the PRESET tile only shows for classic synths (where the wave has an effect); sound engines show the delay send there

## 5.79.1 (16 October 2026)
- Recording length now uses the DSP's exact play position when Rec is pressed the second time (pressing Rec within the first 16 steps keeps 16)

## 5.79.0 (16 October 2026)
- Recording: the clip length is set when Rec is pressed the second time (rounded up to 16, 32, 48 ... steps), on synth and drum tracks
- Clearing a clip sets its length back to 16 steps

## 5.78.0 (16 October 2026)
- PRESET tile on synth tracks: a small preset-style window to pick the waveform (Auto, Saw, Square, Tri, Sine ...) where Drive was. Drive is on SYNTH page 2
- Manuals, README and guides corrected accordingly

## 5.77.1 (16 October 2026)
- New synth track: the browser opens on the SYNTHS family list (Analog, Wavetable, FM 4-OP, FM 2-OP, Fold ...) instead of inside the first family

## 5.77.0 (16 October 2026)
- SYNTH tile, page 2: Drive replaces Chop (Chop removed). On synth tracks the SOUND tile shows the delay send where Drive was (drums keep Drive)

## 5.76.0 (16 October 2026)
- Synth tracks: no more click when the next pad is played (the old note now fades out in about 10 ms instead of being cut)
- Setting a step while stopped starts playback

## 5.75.0 (16 October 2026)
Desktop-tested (UI simulator, 60,000-event fuzzing, native builds, GLIBC 2.17 check); not yet confirmed on the device.

**Tiles and navigation**
- Track tiles reorganised: 1 PRESET · 2 SYNTH (synth tracks) · MOD · FILTER (+ EQ) · FX (+ GRAIN) · FX TYPE (+ FX TIME) · EUCLID · MIDI FX · SAMPLE (now also on synth tracks). Jog click switches the page of a tile
- Header: tile number and name top left, page (e.g. 2/2, EUCLID 1/7) top right, always
- Left / Right = previous / next track (Shift = step page); in lists and name entry they move the selection / cursor
- Menu always opens at the top on QUICK. Knob turns show the name top left and only the value top right
- MOD tile: Ring, RingFrq, Pump, Wobble / Duck, DuckRel, DuckSrc, PumpCrv (drums: Rumble / RumbleTail instead of Wobble / PumpCrv). Drum tracks show a delay send on the first page instead of voices
- FX types turn more slowly; Euclid and MIDI FX are off by default (older saves are migrated once)

**Sequencing and recording**
- Quantize page (Shift + Step 3): grid 1/4, 1/8, 1/8T, 1/16, 1/16T, 1/32, strength 25–100 %, pad **Layout** (KEY, SEQ, OCT, 4THS, 3RDS, ROW8). "Quantize now" in the menu (EDIT CLIP) and Shift + Step 16. Live recording snaps to the grid and keeps the remainder as nudge
- Live recording keeps the real hold length of pads; a held pad + a step button sets that step to the pad's pitch and velocity
- Recording into an empty clip grows it with you: 16 → 32 → 48 … steps (also drum tracks)
- Euclid: master switch OFF / ON / ADD on the first page, lane pages start with the lane number; Euclid no longer overwrites steps while it is off
- Swing is correct for odd loop lengths (the groove restarts on every pass)
- No stray notes while the Synth / Drum question of a new track is open; a fresh drum slot clears old patterns in the engine

**Sounds and browser**
- Preset categories are single names (no more "A + B"); new synth tracks start in SYNTHS → first family → INIT

**Project and menu**
- Project slots: LOAD, SAVE HERE, RENAME, NEW EMPTY, DELETE; Back deletes a character in name entry
- EXPERT: CPU meter / Load info screen (load, peak, history, number of sounds and effects), Scope with new animations (DANCE, STARS, RINGS)
- Exit dialog: BACKGROUND / CLOSE selectable again; the scene toast no longer pops up during song playback

**Engine**
- Denormal flush-to-zero around every audio block (less CPU spike and crackle on decaying tails); the UI polls the engine less often while stopped

**Docs and CI**
- Manuals (EN/DE), animated guide, README and display preview updated to 5.75.0
- CI runs a syntax check, a UI smoke + fuzz test, a sanitizer compile check and the aarch64 build; the older 5.4-line tests live in `taktile/tools/test/legacy-5.4/`

**Not carried over from the previous main branch** (still available on the branch `pre-5.74-upload`): queued live project switch, mono sample storage with resample buffer guard, `CORRUPT` project guard, Shift + arrow sub-pages. The 5.7x line uses jog click for second pages.

## Previous main branch (not part of 5.75.0)
- **Live project switch.** While the sequencer plays, PROJECT → Load no longer stops the music. The chosen project is queued and swaps in at the end of the current bar: all tracks are muted (delay and reverb tails keep ringing), the new project is sent to the DSP with a fast queue (48 messages per tick instead of 3; a full project of about 1000 messages takes roughly half a second), then the transport restarts at step 1 with the new project and the mutes of that project. Turn Load again for the same slot, or stop the transport, to cancel. A stopped sequencer loads immediately, as before. It is not gapless: expect a short silence at the switch
- **Jog click is consistent now.** It never cycles pages any more: click = open / confirm, or back to the last tab on a track tab, or HOME. The sub-pages of MIX (Volume / Delay / Reverb), EUCLID (2 pages), SAMPLE (Sample / Slice / Envelope) and SCOPE (3 views) switch with **Shift + Left / Right** instead (both directions, wraps around). In-device help updated
- **Samples use half the memory.** WAV files are stored as mono (L+R)/2, which is how the engine always played them. A stereo file now takes the same space as a mono one, and resample takes are stored as mono too. The 40 MB bank holds twice as much stereo material
- **Resample respects the buffer.** The take buffer holds 40 s. At slow tempi fewer bars are chosen automatically and a message says so (16 bars below ~96 BPM used to be cut off silently). If the take would not fit into the free sample bank, recording is refused with "SAMPLE BANK FULL" instead of failing at the end
- `tools/test/`: headless UI tests with a mocked Schwung host (`test_jog.js`, `test_resample.js`) and a native DSP test with sanitizers (`run_dsp_test.sh`), all run in CI
- Repository cleanup: one `taktile/` folder (current state, 5.4.1) instead of three copies
- Build scripts are executable after cloning
- `fetch-header.sh` pins Schwung's plugin API header to tag v1.7.3 (`SCHWUNG_REF` to override)
- GitHub Actions: syntax check, sanitizer compile check, aarch64 build, release on `v*` tags
- Loading a project from an unreadable file shows `CORRUPT` and no longer overwrites the current state
- README: install by uploading the release tarball at move.local:7700, compatibility, compatibility, limits, troubleshooting

## 5.4.1
- Current development state (manuals EN/DE updated to 5.4.1)
