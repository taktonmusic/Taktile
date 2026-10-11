#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
DEVICE="${DEVICE:-ableton@move.local}"
DEST=/data/UserData/schwung/modules/overtake
[ -f dist/taktile/dsp.so ] || { echo "Run ./scripts/build.sh first"; exit 1; }
ssh "$DEVICE" "mkdir -p $DEST/taktile"
scp dist/taktile/* "$DEVICE:$DEST/taktile/"
echo "installed. Restart Schwung / the Move."
