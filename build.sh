#!/bin/bash
export PATH="/opt/homebrew/opt/llvm/bin:/opt/homebrew/opt/lld/bin:$PATH"

# 1. Compile host font compiler tool
clang -O2 -o compile_font compile_font.c -lm -lz

# 2. Run it to generate font.generated.h and font_atlas.png
./compile_font

# 3. Compile target canvas.wasm
clang --target=wasm32 \
  -nostdlib \
  -fno-builtin \
  -Wl,--no-entry \
  -Wl,--export-all \
  -Wl,--allow-undefined \
  -o canvas.wasm canvas.c

