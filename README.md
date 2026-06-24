# XWayland / Wayland Keylogger

Linux keyboard logger using **evdev** (`/dev/input/event*`). Captures all keyboard input system-wide at the kernel level — no Wayland or X11 dependency.

## Why evdev?

X11-based keylogging (XRecord, XGrabKeyboard, XI2) only captures keystrokes destined for X11 windows. On modern Wayland desktops, most apps are Wayland-native, so their keystrokes never reach XWayland.

evdev reads directly from the kernel's input subsystem, before any display server touches the data. Every key press, release, and repeat is captured regardless of which app is focused.

## Requirements

- Linux with `/dev/input/event*` access
- Read access to those devices:
  - **root** (`sudo ./xwayland_keylogger`), or
  - **input group** (`sudo usermod -a -G input $USER`, then log out and back in)

## Build

```
make
```

Produces two identical binaries (just different default log paths):

| binary | default log |
|---|---|
| `xwayland_keylogger` | `xwayland_keys.log` |
| `wayland_keylogger` | `wayland_keys.log` |

## Usage

```
./xwayland_keylogger [logfile]
./xwayland_keylogger --help
```

Press **Ctrl+C** to stop.

## Output

Each line: `timestamp key PRESS|RELEASE|REPEAT`

```
1782336704.620539 LEFTCTRL REPEAT
1782336704.633573 A PRESS
1782336704.785348 A RELEASE
1782336704.836883 LEFTCTRL RELEASE
```

`REPEAT` events come from the kernel's key repeat (holding a key down).

## How it works

1. Scans `/dev/input/event*` for devices with letter keys (KEY_A or KEY_1)
2. Polls all found keyboards with `poll()`
3. Reads `struct input_event` from any device with data
4. Logs `EV_KEY` events (press/release/repeat) with human-readable names
5. No X11, no Wayland protocol, no root persistence — pure evdev
