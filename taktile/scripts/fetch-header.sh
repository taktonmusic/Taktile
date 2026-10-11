#!/usr/bin/env bash
# Downloads Schwung's real plugin_api_v1.h (must match the host exactly).
# SCHWUNG_REF pins the Schwung tag/branch/commit the header is taken from, so builds stay
# reproducible. Override it if your Move runs a different Schwung version, e.g.
#   SCHWUNG_REF=main ./scripts/fetch-header.sh
set -euo pipefail
SCHWUNG_REF="${SCHWUNG_REF:-v1.7.3}"
cd "$(dirname "$0")/.."
DEST=build/include/host
mkdir -p "$DEST"
if [ -n "${SCHWUNG_ROOT:-}" ] && [ -f "$SCHWUNG_ROOT/src/host/plugin_api_v1.h" ]; then
  cp "$SCHWUNG_ROOT/src/host/plugin_api_v1.h" "$DEST/"
else
  curl -fsSL https://raw.githubusercontent.com/charlesvestal/schwung/${SCHWUNG_REF}/src/host/plugin_api_v1.h \
       -o "$DEST/plugin_api_v1.h"
fi
echo "header ok: $DEST/plugin_api_v1.h (Schwung ${SCHWUNG_REF})"
