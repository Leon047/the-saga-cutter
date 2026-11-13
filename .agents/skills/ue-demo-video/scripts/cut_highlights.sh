#!/usr/bin/env bash
# Cut a highlight reel from a playthrough capture: frame segments joined with short dips to black, optional music.
# Usage: cut_highlights.sh <frames_dir> <out.mp4> [-m music.mp3] [-f fade_frames=5] start:len [start:len ...]
#   start:len   first MovieFrame index and number of frames of each segment, in playback order
#   music       mixed under the whole reel, faded in/out (TheSagaCutter: Docs/Audio/Music/*.mp3)
# Prints the reel length and writes <out>.contact.png (4 frames per segment) for review.
set -u
DIR="$1"; OUT="$2"; shift 2
MUSIC=""; FADE=5
while getopts "m:f:" o; do case $o in m) MUSIC="$OPTARG";; f) FADE="$OPTARG";; esac; done
shift $((OPTIND - 1))
FF="${FFMPEG:-/c/Tools/ffmpeg/bin/ffmpeg}"
[ $# -ge 1 ] || { echo "no segments"; exit 1; }

inputs=(); fc=""; cat=""; sample=""; i=0; total=0
for seg in "$@"; do
  s=${seg%%:*}; l=${seg##*:}
  inputs+=(-framerate 30 -start_number "$s" -i "$DIR/MovieFrame%05d.png")
  fc+="[$i:v]trim=end_frame=$l,setpts=PTS-STARTPTS"
  [ "$FADE" -gt 0 ] && fc+=",fade=t=in:s=0:n=$FADE,fade=t=out:s=$(( l - FADE )):n=$FADE"
  fc+="[v$i];"; cat+="[v$i]"
  for column in 1 3 5 7; do
    frame=$(( total + l * column / 8 ))
    sample+="eq(n\,$frame)+"
  done
  i=$((i + 1)); total=$((total + l))
done
fc+="${cat}concat=n=$i:v=1:a=0[v]"
secs=$(awk "BEGIN{print $total/30}")

amap=()
if [ -n "$MUSIC" ]; then
  inputs+=(-i "$MUSIC")
  fc+=";[$i:a]atrim=0:$secs,afade=t=in:d=0.5,afade=t=out:st=$(awk "BEGIN{print $secs-1.5}"):d=1.5[a]"
  amap=(-map "[a]" -c:a aac -b:a 192k)
fi

"$FF" -hide_banner -loglevel error -y "${inputs[@]}" -filter_complex "$fc" -map "[v]" "${amap[@]}" \
  -c:v libx264 -pix_fmt yuv420p -crf 20 -movflags +faststart "$OUT" || exit 4
echo "encoded $OUT  segments=$i  frames=$total  seconds=$secs  bytes=$(stat -c %s "$OUT")"

"$FF" -hide_banner -loglevel error -y -i "$OUT" -vf "select='${sample%+}',scale=384:-1,tile=4x$i" -frames:v 1 "${OUT%.mp4}.contact.png" \
  && echo "contact sheet ${OUT%.mp4}.contact.png"
