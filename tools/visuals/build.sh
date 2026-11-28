#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Sornyak's visuals for a projector: the flora engine as WebAssembly and its page.
#   sh tools/visuals/build.sh [--serve]
#   -> build/visuals/{index.html, vis.js, sornyak-visuals.wasm}
# --serve: then serves it at http://localhost:8017 (open it in Chrome or Edge: Web MIDI; Safari and Firefox have none).
# Needs emcc (the Emscripten SDK: ~/emsdk/emsdk_env.sh is sourced when emcc is not on the PATH).
set -e
ROOT=$(cd "$(dirname "$0")/../.." && pwd)
cd "$ROOT"
if ! command -v emcc >/dev/null 2>&1; then
    for e in "${EMSDK:-}/emsdk_env.sh" "$HOME/emsdk/emsdk_env.sh"; do
        [ -f "$e" ] && { . "$e" >/dev/null 2>&1; break; }
    done
fi
command -v emcc >/dev/null 2>&1 || { echo "emcc not found: install the Emscripten SDK (~/emsdk)"; exit 1; }
[ -f build/gen/cf_title.h ] || python3 tools/gen_title.py build/gen/cf_title.h   # (the intro's title)
OUT=build/visuals
mkdir -p "$OUT"
emcc -O2 --no-entry -sSTANDALONE_WASM -sERROR_ON_UNDEFINED_SYMBOLS=1 -sINITIAL_MEMORY=1MB -sSTACK_SIZE=64KB \
     -Ifirmware/src/flora -Ifirmware/src -Ibuild/gen -o "$OUT/sornyak-visuals.wasm" tools/visuals/vis.c
cp tools/visuals/index.html tools/visuals/vis.js "$OUT/"
echo "visuals: $OUT"
if [ "${1:-}" = "--serve" ]; then
    echo "open http://localhost:8017 in Chrome (Ctrl-C stops it)"
    cd "$OUT" && exec python3 -m http.server 8017 --bind 127.0.0.1
fi
