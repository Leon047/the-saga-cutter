#!/bin/sh
# Builds the TheSagaCutterEditor target (close the editor first). Usage: sh Scripts/build_editor.sh
UE_ROOT="${UE_ROOT:-C:\Programs\UE_5.9}"
ROOT="$(cd "$(dirname "$0")/.." && pwd -W 2>/dev/null || pwd)"
cmd.exe //c "$UE_ROOT\Engine\Build\BatchFiles\Build.bat TheSagaCutterEditor Win64 Development -Project=$ROOT/UE/TheSagaCutter.uproject -WaitMutex" 2>&1 | grep -E "error|warning C|Result|fatal"
