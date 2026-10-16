# Legacy tests (5.4 line)

These tests belong to an older development line (Shift + arrows for sub-pages, queued live project switch,
mono sample storage, resample buffer limit). The current line (5.7x) uses jog click for second pages and does not
contain those features, so the tests are kept for reference only and are not run in CI.
The current CI runs `tools/test/test_smoke.js` (40,000 random events), a sanitizer compile check and the aarch64 build.
