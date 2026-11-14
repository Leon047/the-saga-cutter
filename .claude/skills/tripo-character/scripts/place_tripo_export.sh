#!/bin/sh
# usage: place_tripo_export.sh <key>  -- takes the newest tripo_convert_*.zip from Downloads
set -e
key="$1"
zip=$(ls -t "$USERPROFILE"/Downloads/tripo_convert_*.zip | head -1)
dst="C:/Users/Sega/Documents/Projects/TheSagaCutter/Art/Tripo/axolotl_$key/rigged"
mkdir -p "$dst"
unzip -o -q "$zip" -d "$dst"
base=$(basename "$zip" .zip)
mv "$dst/$base.fbx" "$dst/${key}_rigged.fbx"
rm -rf "$dst/${key}_rigged.fbm"; mv "$dst/$base.fbm" "$dst/${key}_rigged.fbm"
echo "$zip -> $dst"; ls -l "$dst" "$dst/${key}_rigged.fbm"
