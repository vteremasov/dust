#!/bin/bash
clang --target=wasm32 \
  -nostdlib \
  -fno-builtin \
  -Wl,--no-entry \
  -Wl,--export-all \
  -Wl,--allow-undefined \
  -o canvas.wasm canvas.c
