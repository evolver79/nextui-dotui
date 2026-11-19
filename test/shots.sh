#!/bin/sh
# Builds test/shots.c in the Kino build image (ARM64 bullseye, SDL2 + SDL2_ttf) and renders test/shots/*.png.
set -e
cd "$(dirname "$0")/.."
mkdir -p test/shots
docker run --rm -v "$PWD:/w" -w /w ${DOT_IMAGE:-nextui-kino-build} sh -c '
gcc -O2 -Wall -Wextra -Wno-unused-parameter -o test/shots/shots test/shots.c src/dot.c $(pkg-config --cflags --libs sdl2) -lSDL2_ttf -lm
test/shots/shots test/shots fonts'
for f in test/shots/*.bmp; do ffmpeg -v error -y -i "$f" "${f%.bmp}.png" && rm "$f"; done
