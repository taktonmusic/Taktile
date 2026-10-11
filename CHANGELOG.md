# Changelog

## Unreleased
- **Jog click is consistent now.** It never cycles pages any more: click = open / confirm, or back to the last tab on a track tab, or HOME. The sub-pages of MIX (Volume / Delay / Reverb), EUCLID (2 pages), SAMPLE (Sample / Slice / Envelope) and SCOPE (3 views) switch with **Shift + Left / Right** instead (both directions, wraps around). In-device help updated
- `tools/test/`: headless UI test with a mocked Schwung host (`node tools/test/test_jog.js`), runs in CI
- Repository cleanup: one `taktile/` folder (current state, 5.4.1) instead of three copies
- Build scripts are executable after cloning
- `fetch-header.sh` pins Schwung's plugin API header to tag v1.7.3 (`SCHWUNG_REF` to override)
- GitHub Actions: syntax check, sanitizer compile check, aarch64 build, release on `v*` tags
- Loading a project from an unreadable file shows `CORRUPT` and no longer overwrites the current state
- README: install by uploading the release tarball at move.local:7700, compatibility, compatibility, limits, troubleshooting

## 5.4.1
- Current development state (manuals EN/DE updated to 5.4.1)
