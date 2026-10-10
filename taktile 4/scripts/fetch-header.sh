#!/usr/bin/env bash
# Downloads Schwung's real plugin_api_v1.h (must match the host exactly).
set -euo pipefail
cd "$(dirname "$0")/.."
DEST=build/include/host
mkdir -p "$DEST"
if [ -n "${SCHWUNG_ROOT:-}" ] && [ -f "$SCHWUNG_ROOT/src/host/plugin_api_v1.h" ]; then
  cp "$SCHWUNG_ROOT/src/host/plugin_api_v1.h" "$DEST/"
else
  curl -fsSL https://raw.githubusercontent.com/charlesvestal/schwung/main/src/host/plugin_api_v1.h \
       -o "$DEST/plugin_api_v1.h"
fi
echo "header ok: $DEST/plugin_api_v1.h"
