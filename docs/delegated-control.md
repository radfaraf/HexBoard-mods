# Delegated Control Host Protocol Guide

Delegated Control lets a computer program use the HexBoard as a raw MIDI
button-and-LED surface.

In this mode, the host program owns the interaction:

- the HexBoard sends button presses and releases as MIDI note messages
- the host sends SysEx messages to set LED colors
- the host decides what every button means
- the host is responsible for restoring any LED colors it temporarily changes

Delegated mode does not show the normal keyboard palette, scale colors, or
pressed-key brightness by default. Entering delegated mode clears the delegated
LEDs to black/off, and the host is responsible for every visible LED color from
that point on. Send an initial LED frame after entering delegated mode if you
want any keys lit.

This guide is for software that controls an existing HexBoard over MIDI. It
does not require modifying the firmware.

## Quick Start

1. Open the HexBoard MIDI input and output ports from your host program.
2. Put the board in Keyboard mode if you plan to enter delegated mode over
   SysEx.
3. Send the delegated-enter SysEx:

```text
F0 7D 01 F7
```

4. Send LED SysEx messages to draw your initial surface; this is required if
   you want any keys lit after entering delegated mode.
5. Listen for MIDI Note On and Note Off messages from the HexBoard.
6. Decode each note message into a HexBoard button index.
7. Send LED SysEx updates when your application state changes.
8. Send the delegated-exit SysEx when your program is done:

```text
F0 7D 02 F7
```

Delegated mode can also be enabled or disabled from the HexBoard's Advanced
menu with `Delegate Control`.

## SysEx Message Format

The HexBoard currently uses the development/educational manufacturer ID
`0x7D`.

Delegated Control SysEx messages use this general form:

```text
F0 7D <command> <payload...> F7
```

Commands:

| Command | Direction | Meaning |
| --- | --- | --- |
| `01` | host to HexBoard | Enter delegated mode |
| `02` | host to HexBoard | Exit delegated mode |
| `03` | host to HexBoard | Set one or more LED colors |

Important behavior:

- Sending `F0 7D 01 F7` enters delegated mode and clears all delegated LED
  colors to off.
- While delegated mode is active, sending command `01` again does nothing.
- While delegated mode is active, sending `F0 7D 02 F7` exits delegated mode.
- If delegated-enter SysEx is not being recognized, make sure the HexBoard is
  in Keyboard mode before sending it.

## Optional Identity Request

Your program can ask the HexBoard to identify itself with the standard MIDI
device identity request:

```text
F0 7E <device-id> 06 01 F7
```

The HexBoard responds with an identity reply whose payload is:

```text
7E 00 06 02 7D 01 00 01 00 <hardware-version> 00 00 00
```

Most MIDI libraries include the surrounding `F0` and `F7` when they deliver the
reply to your application. The manufacturer byte is currently `7D`.

## Button Events From The HexBoard

In delegated mode, the visible HexBoard buttons are numbered `0` through `139`.
The matching LEDs use the same indices.

The HexBoard sends button events as MIDI note messages:

```text
button press   -> Note On,  velocity 127
button release -> Note Off, velocity 0
```

The button index is split across MIDI channel and note number:

```text
channel = (buttonIndex / 100) + 1
note    = buttonIndex % 100
```

The channel values above are normal one-based MIDI channels, `1` through `16`.
If your MIDI library reports channels as zero-based numbers, convert them to
one-based channel numbers before decoding.

Host-side decode:

```cpp
uint16_t buttonIndex = ((channel - 1) * 100) + note;
```

Examples:

| Button | Event | MIDI meaning | Raw MIDI bytes |
| --- | --- | --- | --- |
| `0` | press | Note On, channel 1, note 0, velocity 127 | `90 00 7F` |
| `0` | release | Note Off, channel 1, note 0, velocity 0 | `80 00 00` |
| `60` | press | Note On, channel 1, note 60, velocity 127 | `90 3C 7F` |
| `130` | press | Note On, channel 2, note 30, velocity 127 | `91 1E 7F` |
| `130` | release | Note Off, channel 2, note 30, velocity 0 | `81 1E 00` |

For defensive MIDI handling, your host can also treat Note On with velocity `0`
as a release, even though the HexBoard's delegated release message is Note Off.

## LED Color Messages

LED updates use command `03`:

```text
F0 7D 03 <led-record> [<led-record> ...] F7
```

Each LED record is 5 bytes:

| Byte | Field | Meaning |
| --- | --- | --- |
| `0` | `led_hi` | LED index high 7 bits |
| `1` | `led_lo` | LED index low 7 bits |
| `2` | `hue` | Hue, `0..127` |
| `3` | `sat` | Saturation, `0..127` |
| `4` | `val` | Value/brightness, `0..127` |

Encode the LED index as a 14-bit value:

```cpp
uint8_t led_hi = (ledIndex >> 7) & 0x7F;
uint8_t led_lo = ledIndex & 0x7F;
```

Valid LED indices are `0` through `139`.

Color fields are 7-bit HSV:

- `hue` maps across the color wheel from `0..127`
- `sat` maps from grayscale or white at `0` to fully saturated at `127`
- `val` maps from off at `0` to brightest at `127`

The board applies its own brightness and LED color handling after receiving
these values, so `val = 127` means "full requested delegated brightness," not
necessarily maximum electrical LED output.

### LED Examples

Turn LED `42` full red:

```text
F0 7D 03 00 2A 00 7F 7F F7
```

Turn LED `42` off:

```text
F0 7D 03 00 2A 00 00 00 F7
```

Turn LED `130` full red:

```text
F0 7D 03 01 02 00 7F 7F F7
```

Set LED `42` full red and LED `43` medium greenish in one message:

```text
F0 7D 03 00 2A 00 7F 7F 00 2B 2A 7F 40 F7
```

The board accepts multiple 5-byte records in one LED SysEx message. If you are
updating many LEDs at once, your MIDI library or operating system may impose a
SysEx buffer size, so be ready to split a full-board frame into smaller
messages if needed.

## Host-Side Helper Code

Example C++ helper for one LED record:

```cpp
#include <array>
#include <cstdint>

std::array<uint8_t, 5> makeLedRecord(
    uint16_t ledIndex,
    uint8_t hue,
    uint8_t sat,
    uint8_t val) {
  return {
    static_cast<uint8_t>((ledIndex >> 7) & 0x7F),
    static_cast<uint8_t>(ledIndex & 0x7F),
    static_cast<uint8_t>(hue & 0x7F),
    static_cast<uint8_t>(sat & 0x7F),
    static_cast<uint8_t>(val & 0x7F),
  };
}
```

Example event loop outline:

```cpp
sendSysEx({0xF0, 0x7D, 0x01, 0xF7});
sendInitialLedFrame();

while (running) {
  MidiMessage msg = readMidi();

  if (msg.isNoteOn() && msg.velocity > 0) {
    uint16_t button = ((msg.channel - 1) * 100) + msg.note;
    onHexButtonPressed(button);
  } else if (msg.isNoteOff() || (msg.isNoteOn() && msg.velocity == 0)) {
    uint16_t button = ((msg.channel - 1) * 100) + msg.note;
    onHexButtonReleased(button);
  }
}

sendSysEx({0xF0, 0x7D, 0x02, 0xF7});
```

## Temporary Brighten And Restore

Delegated mode can brighten a key and restore it later, but your host program
must remember what "normal" means for that key.

Example:

1. Your app decides LED `42` normally uses `hue=0`, `sat=127`, `val=32`.
2. An event starts, so your app sends LED `42` at `val=127`.
3. The event ends, so your app sends LED `42` again at `val=32`.

Messages:

```text
normal: F0 7D 03 00 2A 00 7F 20 F7
bright: F0 7D 03 00 2A 00 7F 7F F7
normal: F0 7D 03 00 2A 00 7F 20 F7
```

The HexBoard does not currently provide a delegated SysEx command that means
"make this key brighter than its current normal color" or "restore this key to
whatever the firmware would normally draw." In delegated mode, the host owns
the LED color state.

## Practical Notes

- Enter delegated mode at the start of each host-controlled session instead of
  assuming it was already active.
- Send an exit message when your program closes cleanly.
- Keep a host-side copy of your desired LED state so you can redraw the surface
  after entering delegated mode or after a connection hiccup.
- Use LED indices `0..139`; button and LED indices match for the visible board.
- Command buttons are reported as ordinary indexed buttons in delegated mode.
- If the HexBoard menu is being operated locally, some menu shortcut button
  presses may be consumed by the board instead of sent to the host. For the
  most predictable host control, leave the board idle after entering delegated
  mode.
