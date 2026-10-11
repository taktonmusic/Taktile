#!/usr/bin/env bash
# Cross-compiles dsp.so for the Move (aarch64) and packs dist/taktile-module.tar.gz
set -euo pipefail
cd "$(dirname "$0")/.."
if [ ! -f build/include/host/plugin_api_v1.h ]; then ./scripts/fetch-header.sh; fi
CC="${CROSS_PREFIX:-aarch64-linux-gnu-}gcc"
if ! command -v "$CC" >/dev/null 2>&1; then
  command -v docker >/dev/null || { echo "Need aarch64-linux-gnu-gcc or Docker"; exit 1; }
  exec docker run --rm -v "$PWD":/w -w /w debian:bookworm-slim \
    bash -c "apt-get update -qq && apt-get install -y -qq gcc-aarch64-linux-gnu >/dev/null && ./scripts/build.sh"
fi
mkdir -p dist/taktile
"$CC" -O3 -shared -fPIC -Wall -Wextra -Wno-misleading-indentation -Ibuild/include \
    src/pad_studio.c -o dist/taktile/dsp.so -lm
"${CC%gcc}strip" dist/taktile/dsp.so 2>/dev/null || true
cp module.json ui.js dist/taktile/
tar -C dist -czf dist/taktile-module.tar.gz taktile
echo "built: dist/taktile/ and dist/taktile-module.tar.gz"
