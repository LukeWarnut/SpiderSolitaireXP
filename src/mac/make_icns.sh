#!/bin/sh
# make_icns.sh <103.ico> <out.icns>
# Builds the bundle icon from spider.exe's icon 103. Streams 3-5 of that .ico
# are the 48/32/16 px images with alpha; larger sizes are nearest-neighbour
# enlargements of the 48 px image.
set -e
ico="$1"
out="$2"
set_dir="${out%.icns}.iconset"
rm -rf "$set_dir"
mkdir -p "$set_dir"

emit() {
    ffmpeg -hide_banner -loglevel error -y -i "$ico" -map "0:$1" \
        -vf "scale=${2}:${2}:flags=neighbor" -frames:v 1 "$set_dir/$3"
}

emit 5 16 icon_16x16.png
emit 4 32 icon_16x16@2x.png
emit 4 32 icon_32x32.png
emit 3 64 icon_32x32@2x.png
emit 3 128 icon_128x128.png
emit 3 256 icon_128x128@2x.png
emit 3 256 icon_256x256.png
emit 3 512 icon_256x256@2x.png
emit 3 512 icon_512x512.png
emit 3 1024 icon_512x512@2x.png
iconutil -c icns "$set_dir" -o "$out"
rm -rf "$set_dir"
