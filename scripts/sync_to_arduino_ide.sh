#!/bin/zsh

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
DEFAULT_TARGET="/Users/robertw/Documents/Arduino/Hexboard-ArduinoIDE/HexBoard"
TARGET_DIR="${1:-$DEFAULT_TARGET}"
SKETCH_FILE="$TARGET_DIR/HexBoard.ino"
ARDUINO_READY_DELAY_SECONDS=4

echo "Run started: $(date '+%Y-%m-%d %H:%M:%S')"

mkdir -p "$TARGET_DIR"

sync_file() {
  local source_file="$1"
  local target_file="$2"
  local temp_file="${target_file}.tmp.$$"

  cp "$source_file" "$temp_file"

  if ! cmp -s "$source_file" "$temp_file"; then
    rm -f "$temp_file"
    echo "Failed to verify copied file: $target_file" >&2
    exit 1
  fi

  mv "$temp_file" "$target_file"

  if ! cmp -s "$source_file" "$target_file"; then
    echo "Failed to verify final file contents: $target_file" >&2
    exit 1
  fi
}

sync_file "$REPO_ROOT/src/HexBoard.ino" "$TARGET_DIR/HexBoard.ino"
sync_file "$REPO_ROOT/src/SequencerMode.cpp" "$TARGET_DIR/SequencerMode.cpp"
sync_file "$REPO_ROOT/src/SequencerMode.h" "$TARGET_DIR/SequencerMode.h"

echo "Synced sketch files to: $TARGET_DIR"

osascript - "$SKETCH_FILE" "$ARDUINO_READY_DELAY_SECONDS" <<'APPLESCRIPT'
on run argv
set sketchPath to item 1 of argv
set readyDelaySeconds to (item 2 of argv) as integer
set sketchAlias to POSIX file sketchPath
set actionChoices to {"Verify/Compile", "Export Compiled Binary", "Upload Using Programmer"}
tell application "System Events" to set ideRunning to exists process "Arduino IDE"

if ideRunning then
  tell application "Arduino IDE" to activate
else
  tell application "Arduino IDE" to open sketchAlias
  tell application "Arduino IDE" to activate
end if

set chosenActionList to (choose from list actionChoices)

if chosenActionList is false then
  return
end if

set chosenAction to item 1 of chosenActionList

tell application "System Events"
  tell process "Arduino IDE"
    set frontmost to true

    repeat 120 times
      try
        if exists menu bar item "Sketch" of menu bar 1 then
          exit repeat
        end if
      end try
      delay 0.5
    end repeat

    if not (exists menu bar item "Sketch" of menu bar 1) then
      error "Arduino IDE Sketch menu did not become available."
    end if

    repeat 120 times
      try
        if (count of windows) > 0 then
          set frontWindowName to name of front window as text
          if frontWindowName contains "HexBoard" then
            exit repeat
          end if
        end if
      end try
      delay 0.25
    end repeat

    delay readyDelaySeconds

    if chosenAction is "Verify/Compile" then
      click menu item "Verify/Compile" of menu "Sketch" of menu bar item "Sketch" of menu bar 1
    else if chosenAction is "Export Compiled Binary" then
      click menu item "Export Compiled Binary" of menu "Sketch" of menu bar item "Sketch" of menu bar 1
    else if chosenAction is "Upload Using Programmer" then
      click menu item "Upload Using Programmer" of menu "Sketch" of menu bar item "Sketch" of menu bar 1
    end if
  end tell
end tell
end run
APPLESCRIPT
