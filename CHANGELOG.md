# Changelog

## Unreleased
- **Sound engines.** The SOUND page has an engine selector (ANALOG, MODAL, FM, VOX, CHIP, MORPH). The Preset knob only walks through the presets of the chosen engine instead of one giant list. Each engine has an INIT preset (ModINIT, FMINIT, VoxINIT, ChpINIT, MrpINIT) as a starting point, so there are 1215 sounds now (INIT is appended last, old projects load unchanged)
- **Engine parameters.** Each engine has its own named SYNTH page (e.g. FM: ratios, indices, feedback, algorithm; VOX: vowels, voice, breath, vibrato; CHIP: duty, arp, bits, slide; MORPH: two wavetables, morph, unison). Values are per pad, saved with projects and clips, and held notes follow the knobs. Without edits a preset sounds exactly as before
- **Tab order.** SOUND, SYNTH, FILTER, MOD, FX, EQ, EUCLID, MIDI FX. The tab strip shows the previous / current / next page
- **FX pads (new pad mode).** Pad mode FX (Pads knob on the TRACK page, or the track button): hold a pad and a performance effect is on, release and the Master page values are back. Synth track: the two top rows (top row grouped 2 / 4 / 2: filter sweeps, EQ kills, drive; second row: dull, thin, mono, wide, squash, boom, air, tape), the two lower rows keep playing notes. Drum track: the right 4x4 block. All FX pads have their own colours, never the note colour. They work on the master bus only, so they cost no CPU
- **MIX page.** The jog click switches Volume > Delay send > Reverb send again (Shift + Left / Right still works). In MIX (and while Mute is held) the pads no longer play the selected track's sound, they only mute
- **Arp octaves.** New ARP OCT option (1-4 octaves) for Up, Down, UpDn and Rand. Arp, Oct and Rate now sit together on the MIDI FX page (they left the MOD page)
- **Simpler QUANT page.** Only what quantizes: Clip Start, Rec Timing, To Scale, Key, Scale
- **FX tiles.** On the FX tab the jog click opens the FX tiles (Delay, Reverb, Chorus, Phaser, Duck); Back goes up again. MIDI is its own tab and no longer a tile there
- **Less CPU, simpler FX.** The insert effect (stutter, reverse, flanger, ...) and the FX 2 tab are removed; delay time / feedback and reverb size now sit on the FX page. The home FX tiles lose MULTI. The MIDI FX page keeps Effect, Amount, Param, Arp and Rate (chords: effect CHRD with 8 chord types). Old projects load, the insert values are ignored
- `tools/test/test_engines.js`, `test_engines.c` (ASAN/UBSAN) and `test_fxpads.js`, run in CI
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
