#!/usr/bin/env bash
set -euo pipefail
cd -- "$(dirname -- "$0")"
python3 tools/embed_shaders.py
"${CXX:-i686-w64-mingw32-g++}" -std=c++17 -O2 -Wall -Wextra -Werror \
  -shared -static -static-libgcc -static-libstdc++ src/plugin.cpp \
  -o PSXFX.SA.asi -ladvapi32 -Wl,--kill-at
