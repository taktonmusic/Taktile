# Changelog

## Unreleased
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
