#!/bin/zsh

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
DEFAULT_TARGET="/Users/robertw/Documents/Arduino/Hexboard-ArduinoIDE/HexBoard"
TARGET_DIR="${1:-$DEFAULT_TARGET}"

mkdir -p "$TARGET_DIR"

cp "$REPO_ROOT/src/HexBoard.ino" "$TARGET_DIR/HexBoard.ino"
cp "$REPO_ROOT/src/SequencerMode.cpp" "$TARGET_DIR/SequencerMode.cpp"
cp "$REPO_ROOT/src/SequencerMode.h" "$TARGET_DIR/SequencerMode.h"
sleep 1

echo "Synced sketch files to: $TARGET_DIR"
open "$TARGET_DIR/HexBoard.ino"

osascript <<'APPLESCRIPT'
set actionChoices to {"Verify/Compile", "Export Compiled Binary", "Upload Using Programmer"}
set chosenActionList to choose from list actionChoices with prompt "Choose Arduino IDE action" default items {"Verify/Compile"}

if chosenActionList is false then
  return
end if

set chosenAction to item 1 of chosenActionList

tell application "Arduino IDE" to activate
tell application "System Events"
  tell process "Arduino IDE"
    repeat 60 times
      if exists menu bar item "Sketch" of menu bar 1 then
        exit repeat
      end if
      delay 0.5
    end repeat

    if not (exists menu bar item "Sketch" of menu bar 1) then
      error "Arduino IDE Sketch menu did not become available."
    end if
    delay 2
    if chosenAction is "Verify/Compile" then
      click menu item "Verify/Compile" of menu "Sketch" of menu bar item "Sketch" of menu bar 1
    else if chosenAction is "Export Compiled Binary" then
      click menu item "Export Compiled Binary" of menu "Sketch" of menu bar item "Sketch" of menu bar 1
    else if chosenAction is "Upload Using Programmer" then
      click menu item "Upload Using Programmer" of menu "Sketch" of menu bar item "Sketch" of menu bar 1
    end if
  end tell
end tell
APPLESCRIPT
