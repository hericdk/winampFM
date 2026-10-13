#!/bin/sh
# winampFM: compila o firmware (Felucca/SLOOP + interface winampFM) para WebAssembly, para o simulador 3D (../sim).
#   precisa: python3 com ziglang, numpy, pillow  (pip3 install ziglang numpy pillow)
#   ./build-wasm.sh   ->  ../sim/winampfm.wasm
#   BOARD=smk37 ./build-wasm.sh   ->  ../sim/winampfm-smk37.wasm (the M-Vave SMK-37 build's UI: SMK37.md)
set -e
cd "$(dirname "$0")"
ROOT=$(pwd)
OUT=../sim/winampfm.wasm DEF=
[ "${BOARD:-fm1}" = smk37 ] && OUT=../sim/winampfm-smk37.wasm DEF=-DBOARD_SMK37=1
python3 -c "import sys; sys.path.insert(0, '$ROOT/tools'); import build; build.generate()" >/dev/null
python3 -m ziglang cc --target=wasm32-freestanding -O2 -fno-builtin -ffreestanding -nostdlib -w $DEF \
  -I "$ROOT/build/gen" -I "$ROOT/firmware/src" -I wasm \
  -Wl,--no-entry -Wl,--export-memory -Wl,-z,stack-size=1048576 -Wl,--global-base=1048576 \
  -o $OUT wasm/winampfm_wasm.c
ls -la $OUT
