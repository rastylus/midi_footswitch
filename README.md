# MIDI Footswitch (KB2040)

Custom RP2040 MIDI footswitch firmware built with PlatformIO for Adafruit KB2040.

Current capabilities:
- 8 footswitch inputs with debounce
- Per-switch MIDI action types: Note, CC, Program Change, Transport
- Momentary and toggle behaviors
- USB MIDI + DIN MIDI output
- OLED status display (SSD1306 128x64, I2C)
- NeoPixel switch LEDs + 2 status LEDs on same chain
- Chord bank switching (SW1+SW2 down, SW7+SW8 up)

## Default Switch Mapping (Factory Reset)

Factory reset applies an Ableton-first profile:

- Bank 1 (control bank): all 8 switches are toggle CC messages on channel 1.
  - SW1-SW8 send CC20-CC27, ON=127 and OFF=0.
- Banks 2-7 (clip launch): all 8 switches are momentary notes on channel 1.
  - Bank 2: 60-67 (C4-G4)
  - Bank 3: 68-75 (G#4-D#5)
  - Bank 4: 76-83 (E5-B5)
  - Bank 5: 84-91 (C6-G6)
  - Bank 6: 92-99 (G#6-D#7)
  - Bank 7: 100-107 (E7-B7)
- Bank 8 (transport): fixed mirrored transport layout.
  - SW1..SW8 = PLAY, STOP, CONT, PANIC, PANIC, CONT, STOP, PLAY.

Notes:
- Existing saved EEPROM settings load at boot and override factory defaults.
- To re-apply this mapping, use the on-device reset-hold action (CASCADE RESET).

## Hardware

Target board:
- Adafruit KB2040 (RP2040)

Peripherals currently used:
- 8 switches on GPIO 2-9 (INPUT_PULLUP)
- NeoPixel chain on GPIO 10
  - Pixels 0-7: switch indicators
  - Pixel 8: power/status (red)
  - Pixel 9: bank color indicator
- SSD1306 OLED at I2C address 0x3C
- DIN MIDI out on Serial1 @ 31250 baud

## Current Pinout (Adafruit Labels)

KB2040 silk labels are shown with the active firmware assignment.

- `GP2`  -> `SW_1`
- `GP3`  -> `SW_2`
- `GP4`  -> `SW_3`
- `GP5`  -> `SW_4`
- `GP6`  -> `SW_5`
- `GP7`  -> `SW_6`
- `GP8`  -> `SW_7`
- `GP9`  -> `SW_8`
- `GP10` -> NeoPixel data (10-pixel chain)
- `GP19` -> Encoder CLK (`ENC_CLK`)
- `GP20` -> Encoder DT (`ENC_DT`)
- `GP18` -> Encoder push switch (`ENC_SW`)
- `GP12` / `SDA` -> OLED SDA
- `GP13` / `SCL` -> OLED SCL

Notes:
- Switches are configured as `INPUT_PULLUP` (pressed = `LOW`).
- NeoPixel status indices are currently `8` (power) and `9` (bank).
- USB MIDI is over the native USB port; DIN MIDI uses `Serial1` in firmware.

## Software Stack

- PlatformIO
- Arduino framework (RP2040, Earle Philhower core via maxgerhardt platform)
- Libraries:
  - MIDIUSB
  - MIDI Library
  - Adafruit NeoPixel
  - Adafruit SSD1306
  - Adafruit GFX

## Build and Upload

From repository root:

```powershell
& "C:\Users\ryanp\.platformio\penv\Scripts\platformio.exe" run
& "C:\Users\ryanp\.platformio\penv\Scripts\platformio.exe" run -e kb2040 -t upload
```

VS Code PlatformIO Build/Upload tasks are also configured and working.

## Git Workflow

Primary branch:
- `main`

Recommended feature workflow:
1. Create feature branch from main
2. Make small commits
3. Push feature branch
4. Open PR (even for solo development)

Example:

```powershell
git checkout main
git pull
git checkout -b feature/bank-switching
```

## Versioning

This project uses lightweight milestones via Git tags.

Suggested pattern:
- `v0.1.0` - stable baseline
- `v0.2.0` - feature expansion
- `v1.0.0` - first fully stable release

## Notes

- If status LEDs do not light, verify physical pixel order matches indices in `src/8switchNEOpixel.cpp`.
- USB upload uses picotool and may require correct Windows driver binding for RP2 Boot interface.
