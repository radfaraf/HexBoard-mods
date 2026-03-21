#!/bin/zsh

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
DEFAULT_TARGET="/Users/robertw/Documents/Arduino/Hexboard-ArduinoIDE/HexBoard"
TARGET_DIR="${1:-$DEFAULT_TARGET}"

mkdir -p "$TARGET_DIR"

cp "$REPO_ROOT/src/HexBoard.ino" "$TARGET_DIR/HexBoard.ino"
cp "$REPO_ROOT/src/SequencerMode.cpp" "$TARGET_DIR/SequencerMode.cpp"
cp "$REPO_ROOT/src/SequencerMode.h" "$TARGET_DIR/SequencerMode.h"

echo "Synced sketch files to: $TARGET_DIR"
open "$TARGET_DIR/HexBoard.ino"

osascript <<'APPLESCRIPT'
tell application "Arduino IDE" to activate
tell application "System Events"
  tell process "Arduino IDE"
    repeat 60 times
      if exists menu bar item "Sketch" of menu bar 1 then
        exit repeat
      end if
      delay 0.5
    end repeat
    if exists menu bar item "Sketch" of menu bar 1 then
      delay 5
      click menu item "Verify/Compile" of menu "Sketch" of menu bar item "Sketch" of menu bar 1
    else
      keystroke "r" using command down
    end if
  end tell
end tell
APPLESCRIPT
