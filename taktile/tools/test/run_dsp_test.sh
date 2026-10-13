#!/usr/bin/env bash
# Native DSP test with sanitizers: sample import (mono mixdown, memory use) and resample length limit.
set -euo pipefail
cd "$(dirname "$0")/../.."
[ -f build/include/host/plugin_api_v1.h ] || ./scripts/fetch-header.sh
T=$(mktemp -d); trap 'rm -rf "$T"' EXIT
python3 tools/test/make_wavs.py "$T/wav"
gcc -g -fsanitize=address,undefined -Wno-misleading-indentation -Ibuild/include tools/test/test_dsp.c -o "$T/test_dsp" -lm
"$T/test_dsp" "$T/wav"
gcc -g -fsanitize=address,undefined,float-cast-overflow -fno-sanitize-recover=undefined -Wno-misleading-indentation -Ibuild/include tools/test/test_engines.c -o "$T/test_engines" -lm
"$T/test_engines"
