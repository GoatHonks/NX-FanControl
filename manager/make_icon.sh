#!/bin/bash
# Generates the NRO icon (256x256 JPEG, as required by nacptool).
# Requires ImageMagick. Run from the manager/ directory.
set -e

MAGICK="${MAGICK:-/mingw64/bin/magick.exe}"
command -v "$MAGICK" >/dev/null 2>&1 || MAGICK=magick

OUT=icon.jpg

"$MAGICK" -size 256x256 \
    radial-gradient:'#243040'-'#0E1013' \
    -draw "
        push graphic-context
          translate 128,128
          fill '#58A6FF'
          push graphic-context
            rotate 0
            ellipse 0,-54 27,47 0,360
          pop graphic-context
          push graphic-context
            rotate 120
            ellipse 0,-54 27,47 0,360
          pop graphic-context
          push graphic-context
            rotate 240
            ellipse 0,-54 27,47 0,360
          pop graphic-context
          fill '#0E1013'
          circle 0,0 0,26
          fill '#58A6FF'
          circle 0,0 0,13
        pop graphic-context
    " \
    -quality 92 "$OUT"

echo "wrote $OUT"
"$MAGICK" identify "$OUT"
