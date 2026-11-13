#!/usr/bin/env bash
# Record a full self-playing run of the game with the normal in-game camera and HUD, one PNG per frame.
# Standalone "-game" process (no editor, no MRQ): -benchmark -fps=30 gives a fixed 30 fps game clock, -dumpmovie writes
# the game viewport (3D + AHUD canvas) every frame. TSCAuto (an Exec on ATSCGameMode) plays the game by itself.
#
# Usage: playthrough.sh <job_name> [takes=LALA] [build=HM] [speed=4] [map=/Game/Vikings/Maps/Map_Saga] [tail_frames=240]
#   takes        one L (living, leave it) / A (ashen, press CUT!) per scene, in scene order
#   build        build order letters: H house, M sawmill, E healer, S hammer shrine, T stag shrine
#   speed        game speed 1..16 (the HUD shows it; 4 keeps the run ~6-7 min of game time, units move briskly)
#   tail_frames  frames to keep recording after the first [Epilogue] log line (the skald's text screen)
# Output (UE/Saved/MovieRenders/<job_name>/):
#   frames/MovieFrame00000.png ...   every frame of the run
#   events.txt                       "<frame> <text>" for every [Saga]/[Epilogue] log line (frame = engine frame
#                                    counter, matches the MovieFrame index to within a couple of frames)
#   full.mp4, contact.png            whole run at 30 fps and a 6x6 sheet of it, for picking highlights
#   game.log
set -u
JOB="$1"; TAKES="${2:-LALA}"; BUILD="${3:-HM}"; SPEED="${4:-4}"; MAP="${5:-/Game/Vikings/Maps/Map_Saga}"; TAIL="${6:-240}"
ROOT="$(cd "$(dirname "$0")/../../../.." && pwd)"
UPROJECT="$(ls "$ROOT"/UE/*.uproject | head -1)"
UEEXE="${UE_EDITOR:-/c/Programs/UE_5.9/Engine/Binaries/Win64/UnrealEditor.exe}"
FF="${FFMPEG:-/c/Tools/ffmpeg/bin/ffmpeg}"
SHOTS="$ROOT/UE/Saved/Screenshots/WindowsEditor"
OUT="$ROOT/UE/Saved/MovieRenders/$JOB"
LOG="$OUT/game.log"

rm -rf "$OUT"; mkdir -p "$OUT/frames"
rm -f "$SHOTS"/MovieFrame*.png

kill_game() {
  powershell.exe -NoProfile -Command "Get-CimInstance Win32_Process -Filter \"Name='UnrealEditor.exe'\" | Where-Object { \$_.CommandLine -like '*-dumpmovie*' } | ForEach-Object { Stop-Process -Id \$_.ProcessId -Force }" >/dev/null 2>&1
}
kill_game  # a leftover capture would write into the same folder

# MSYS_NO_PATHCONV: otherwise Git Bash rewrites /Game/... into C:/Program Files/Git/Game/... and the map fails to load
MSYS_NO_PATHCONV=1 "$UEEXE" "$(cygpath -w "$UPROJECT")" "$MAP" -game -windowed -ResX=1280 -ResY=720 \
  -benchmark -fps=30 -dumpmovie -nosound -nosplash \
  -ExecCmds="TSCAuto $TAKES $BUILD $SPEED" -abslog="$(cygpath -w "$LOG")" >/dev/null 2>&1 &

count() { ls "$SHOTS"/MovieFrame*.png 2>/dev/null | wc -l; }
# The log's [frame] is GFrameCounter % 1000. Stamp events while recording: the dumped-frame count at poll time (a few
# dozen frames ahead of the event) picks the right thousand.
seen=0; : > "$OUT/events.txt"
stamp() {
  [ -f "$LOG" ] || return
  local total; total=$(wc -l < "$LOG")
  sed -n "$((seen + 1)),${total}p" "$LOG" | awk -v n="$1" '
    /\[(Saga|Epilogue)\] / && match($0, /^\[[^]]*\]\[ *[0-9]+\]/) {
      f = substr($0, 2); f = substr(f, index(f, "][") + 2) + 0
      k = int((n - f) / 1000 + 0.5); if (k < 0) k = 0
      match($0, /\[(Saga|Epilogue)\] /); print f + 1000 * k, substr($0, RSTART) }' >> "$OUT/events.txt"
  seen=$total
}
start=$(date +%s); endat=""
while :; do
  sleep 5
  n=$(count)
  stamp "$n"
  if [ -f "$LOG" ] && grep -q -E "Fatal error|Unhandled Exception" "$LOG"; then echo "GAME CRASHED (see $LOG)"; kill_game; exit 2; fi
  if [ -f "$LOG" ] && grep -q "Failed to enter" "$LOG"; then echo "MAP FAILED TO LOAD (see $LOG)"; kill_game; exit 2; fi
  if [ -z "$endat" ] && [ -f "$LOG" ] && grep -q "\[Epilogue\]" "$LOG"; then endat=$(( n + TAIL )); echo "epilogue at ~frame $n"; fi
  [ -n "$endat" ] && [ "$n" -ge "$endat" ] && break
  if [ $(( $(date +%s) - start )) -gt 2400 ]; then echo "TIMEOUT after 40 min at $n frames - keeping what was captured"; break; fi
done
kill_game; sleep 3
grep -q "\[Saga\] Autoplay" "$LOG" || echo "WARNING: no '[Saga] Autoplay' line - TSCAuto did not run, the game sat on the title screen"

mv "$SHOTS"/MovieFrame*.png "$OUT/frames/"
N=$(ls "$OUT"/frames/MovieFrame*.png | wc -l)
stamp "$N"

"$FF" -hide_banner -loglevel error -y -framerate 30 -i "$OUT/frames/MovieFrame%05d.png" \
  -c:v libx264 -pix_fmt yuv420p -crf 20 -movflags +faststart "$OUT/full.mp4"
step=$(( N / 36 )); [ "$step" -lt 1 ] && step=1
"$FF" -hide_banner -loglevel error -y -i "$OUT/full.mp4" -vf "select='not(mod(n\,$step))',scale=384:-1,tile=6x6" -frames:v 1 "$OUT/contact.png"
echo "frames=$N  (every ${step}th frame in contact.png)"
echo "out=$OUT"
cat "$OUT/events.txt"
