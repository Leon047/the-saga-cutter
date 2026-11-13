#!/bin/sh
# Packages the jam submission (Development, Win64) and fails if any Russian dev data ended up in it.
# Usage: sh Scripts/package_jam.sh <TeamName>
#        sh Scripts/package_jam.sh --gate-only <dir>   (run only the Russian gate on <dir>, no packaging)
UE_ROOT="${UE_ROOT:-C:\Programs\UE_5.9}"
ROOT="$(cd "$(dirname "$0")/.." && pwd -W 2>/dev/null || pwd)"

# gate <dir>: returns 0 if clean, 1 (with messages) if anything Russian is found under <dir>.
gate() {
	dir="$1"
	if [ ! -d "$dir" ]; then
		echo "GATE ERROR: '$dir' is not a directory" >&2
		return 1
	fi
	bad=0

	hits="$(find "$dir" -ipath '*DevLoc*' 2>/dev/null)"
	if [ -n "$hits" ]; then
		echo "GATE FAIL: path containing DevLoc:" >&2
		echo "$hits" >&2
		bad=1
	fi

	hits="$(find "$dir" -type f -iname '*.tsv' 2>/dev/null)"
	if [ -n "$hits" ]; then
		echo "GATE FAIL: .tsv file:" >&2
		echo "$hits" >&2
		bad=1
	fi

	hits="$(find "$dir" -type d \( -name ru -o -name 'ru-*' \) -path '*/Localization/*' 2>/dev/null)"
	if [ -n "$hits" ]; then
		echo "GATE FAIL: ru localization directory:" >&2
		echo "$hits" >&2
		bad=1
	fi

	# Unibyte locale so the byte-range pattern matches raw UTF-8 bytes (C/POSIX rejects -P in this grep).
	hits="$(find "$dir" -type f \( -iname '*.ini' -o -iname '*.txt' -o -iname '*.json' -o -iname '*.csv' \) -print0 2>/dev/null \
		| LC_ALL=en_US.ISO-8859-1 xargs -0 grep -lP '[\xD0-\xD3][\x80-\xBF]' 2>/dev/null)"
	rc=$?
	if [ -n "$hits" ]; then
		echo "GATE FAIL: text file containing Cyrillic UTF-8 bytes:" >&2
		echo "$hits" >&2
		bad=1
	elif [ "$rc" -ne 0 ] && [ "$rc" -ne 123 ]; then
		echo "GATE ERROR: Cyrillic scan failed (rc=$rc)" >&2
		bad=1
	fi

	if [ "$bad" -ne 0 ]; then
		echo "Russian gate FAILED for $dir - do not submit this build." >&2
		return 1
	fi
	echo "Russian gate passed for $dir"
	return 0
}

if [ "$1" = "--gate-only" ]; then
	if [ -z "$2" ]; then
		echo "Usage: $0 --gate-only <dir>" >&2
		exit 2
	fi
	gate "$2"
	exit $?
fi

TEAM="$1"
if [ -z "$TEAM" ]; then
	echo "Usage: $0 <TeamName>   |   $0 --gate-only <dir>" >&2
	exit 2
fi

OUT="$ROOT/Packaged"
# Remove the previous output so a failed run cannot pass on a stale build.
rm -rf "$OUT/Windows"
cmd.exe //c "$UE_ROOT\Engine\Build\BatchFiles\RunUAT.bat BuildCookRun -project=$ROOT/UE/TheSagaCutter.uproject -platform=Win64 -clientconfig=Development -build -cook -stage -pak -archive -archivedirectory=$OUT -nop4 -utf8output" 2>&1 | tee "$ROOT/Packaged_uat.log" | grep -E "error|Error:|BUILD|fatal"
if [ ! -d "$OUT/Windows" ] || ! grep -q "BUILD SUCCESSFUL" "$ROOT/Packaged_uat.log"; then
	echo "Packaging failed: $OUT/Windows was not produced (see Packaged_uat.log)" >&2
	exit 1
fi

gate "$OUT" || exit 1

ZIP="$OUT/${TEAM}_TheSagaCutter.zip"
rm -f "$ZIP"
powershell.exe -NoProfile -Command "Compress-Archive -Path '$OUT/Windows' -DestinationPath '$ZIP' -Force" || { echo "Zip failed" >&2; exit 1; }
BYTES="$(wc -c < "$ZIP" | tr -d ' ')"
echo "Wrote $ZIP ($((BYTES / 1048576)) MB)"
if [ "$BYTES" -ge 1073741824 ]; then
	echo "WARNING: zip is 1 GB or larger" >&2
fi
