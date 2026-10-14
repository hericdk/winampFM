#!/bin/sh
# winampFM: compila o firmware (Felucca/SLOOP + interface winampFM) para WebAssembly, para o simulador 3D (../sim).
#   precisa: python3 com ziglang, numpy, pillow  (pip3 install ziglang numpy pillow)
#   ./build-wasm.sh   ->  ../sim/winampfm.wasm
set -e
cd "$(dirname "$0")"
ROOT=$(pwd)
"${PYTHON:-python3}" -c "import sys; sys.path.insert(0, '$ROOT/tools'); import build; build.generate()" >/dev/null
"${PYTHON:-python3}" -m ziglang cc --target=wasm32-freestanding -O2 -fno-builtin -ffreestanding -nostdlib -w \
  -I "$ROOT/build/gen" -I "$ROOT/firmware/src" -I wasm \
  -Wl,--no-entry -Wl,--export-memory -Wl,-z,stack-size=1048576 -Wl,--global-base=1048576 \
  -o ../sim/winampfm.wasm wasm/winampfm_wasm.c
ls -la ../sim/winampfm.wasm
