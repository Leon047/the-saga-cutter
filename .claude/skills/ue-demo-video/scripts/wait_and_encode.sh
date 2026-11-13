#!/usr/bin/env bash
# Wait for an MRQ PNG render to finish, encode it to H.264 mp4, and write a contact sheet for review.
# Usage: wait_and_encode.sh <frames_dir> <frame_count> <out.mp4> [contact.png] [log]
#   frames_dir   folder with frame.0000.png ... (ue_demo.output_dir(job_name))
#   frame_count  expected number of frames (sequence length)
#   contact.png  optional: 6x6 sheet of every Nth frame, to check for black frames and wall clipping
#   log          optional: editor log to watch for a crash (default: newest %LOCALAPPDATA%/ue-mcp/logs/ue_*.log)
set -u
DIR="$1"; N="$2"; OUT="$3"; SHEET="${4:-}"; LOG="${5:-$(ls -t "$LOCALAPPDATA"/ue-mcp/logs/ue_*.log 2>/dev/null | head -1)}"
FF="${FFMPEG:-/c/Tools/ffmpeg/bin/ffmpeg}"

# 1) wait: all frames present (MRQ writes them last-to-disk, so count, do not trust the log line alone)
start=$(date +%s)
until [ "$(ls "$DIR"/frame.*.png 2>/dev/null | wc -l)" -ge "$N" ]; do
  if [ -n "$LOG" ] && tail -n 200 "$LOG" | grep -q -E "Fatal error|Unhandled Exception"; then echo "EDITOR CRASHED (see $LOG)"; exit 2; fi
  if [ $(( $(date +%s) - start )) -gt 1500 ]; then echo "TIMEOUT: $(ls "$DIR"/frame.*.png 2>/dev/null | wc -l)/$N frames"; exit 3; fi
  sleep 2
done
sleep 2  # last PNG may still be flushing

# 2) encode (matches the project's existing gameplay clip: h264 High, yuv420p, faststart)
"$FF" -hide_banner -loglevel error -y -framerate 30 -start_number 0 -i "$DIR/frame.%04d.png" \
  -c:v libx264 -pix_fmt yuv420p -crf 20 -movflags +faststart "$OUT" || exit 4

# 3) sanity: a mostly black video encodes to only tens of KB
size=$(stat -c %s "$OUT")
echo "encoded $OUT  frames=$N  bytes=$size"
[ "$size" -lt 500000 ] && echo "WARNING: file is tiny - frames are probably black (missing game mode override / lights)"

# 4) contact sheet
if [ -n "$SHEET" ]; then
  step=$(( N / 36 )); [ "$step" -lt 1 ] && step=1
  "$FF" -hide_banner -loglevel error -y -i "$OUT" -vf "select='not(mod(n\,$step))',scale=384:-1,tile=6x6" -frames:v 1 "$SHEET" \
    && echo "contact sheet $SHEET (every ${step}th frame)"
fi
