#!/usr/bin/env bash
# Print quick, objective facts about a UE jam project for the megajam-score rubric.
# Usage: collect_evidence.sh [repo_root]   (defaults to the git toplevel)
set -u
ROOT="${1:-$(git rev-parse --show-toplevel 2>/dev/null || pwd)}"
cd "$ROOT" || exit 1

DEADLINE_UTC="2026-11-16T01:59:00Z"   # Nov 15 5:59 PM PST
now=$(date -u +%s)
dl=$(date -u -d "$DEADLINE_UTC" +%s 2>/dev/null || echo 0)
echo "== Time"
if [ "$dl" -gt 0 ]; then
  left=$(( (dl - now) / 60 ))
  echo "now (UTC): $(date -u '+%Y-%m-%d %H:%M')  deadline: $DEADLINE_UTC  left: $((left / 60))h $((left % 60))m"
fi

echo; echo "== Git"
echo "commits: $(git rev-list --count HEAD 2>/dev/null)  first: $(git log --reverse --format=%ad --date=iso | head -1)"
git log --format='  %ad %s' --date=format:'%m-%d %H:%M' | head -25
echo "uncommitted: $(git status --porcelain | wc -l) paths"

UE=$(find . -maxdepth 3 -name '*.uproject' -not -path './.git/*' | head -1)
UEDIR=$(dirname "${UE:-.}")
echo; echo "== Project: ${UE:-<no .uproject found>}"

echo; echo "== Code size (C++ lines per file)"
find "$UEDIR/Source" -name '*.cpp' -o -name '*.h' 2>/dev/null | xargs wc -l 2>/dev/null | sort -n | tail -15

echo; echo "== Content counts"
C="$UEDIR/Content"
for pat in Maps:'*.umap' Sounds:'*S_*' Music:'*Music*' Cues:'*Cue*' Niagara:'*NS_*' Widgets:'*WBP_*' Materials:'*M_*' Blueprints:'*BP_*'; do
  name=${pat%%:*}; glob=${pat#*:}
  echo "  $name: $(find "$C" -iname "$glob" 2>/dev/null | wc -l)"
done
echo "  .uasset total: $(find "$C" -name '*.uasset' 2>/dev/null | wc -l)"
echo "  audio source files in repo (wav/ogg/mp3): $(find . -iregex '.*\.\(wav\|ogg\|mp3\)' -not -path './.git/*' -not -path '*site-packages*' -not -path '*node_modules*' -not -path '*/.venv/*' -not -iname 'test-*' 2>/dev/null | wc -l)"

echo; echo "== Audio hooks in code"
grep -rniE 'PlaySound|UAudioComponent|SoundBase|SoundCue|MetaSound' "$UEDIR/Source" 2>/dev/null | wc -l | sed 's/^/  references: /'

echo; echo "== Sourced (third-party) content folders — must be listed at submission, not scored"
find "$C" -maxdepth 2 -type d 2>/dev/null | grep -viE '/Content$' | head -30

echo; echo "== Packaged builds"
find . -maxdepth 4 \( -iname 'Windows' -o -iname 'WindowsNoEditor' -o -iname 'Packaged*' -o -iname '*.zip' \) \
  -not -path './.git/*' -not -path '*/Intermediate/*' -not -path '*/Binaries/*' 2>/dev/null | head
find . -maxdepth 6 -iname '*.exe' -path '*Windows*' -not -path '*/Binaries/*' 2>/dev/null | head

echo; echo "== Videos / screenshots / devlog"
find . -maxdepth 3 -iregex '.*\.\(mp4\|mov\|webm\)' -not -path './.git/*' -printf '  %TY-%Tm-%Td %TH:%TM %p\n' 2>/dev/null | sort
echo "  screenshots: $(find . -maxdepth 4 -iregex '.*\(screenshot\|screens\).*\.\(png\|jpg\)' -not -path './.git/*' 2>/dev/null | wc -l)"
find . -maxdepth 3 -iname '*devlog*' -not -path './.git/*' 2>/dev/null | sed 's/^/  devlog: /'

echo; echo "== Theme keywords in code/docs (Director's Cut: cut, edit, take, timeline, rewind, saga, director)"
grep -rhoiE "\b(director|cut|edit(ing)?|take|timeline|rewind|recut|final cut|saga)\b" "$UEDIR/Source" ./*.md 2>/dev/null \
  | tr 'A-Z' 'a-z' | sort | uniq -c | sort -rn | head -12
