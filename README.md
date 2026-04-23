# MIDI Footswitch (KB2040)

Custom RP2040 MIDI footswitch firmware built with PlatformIO for Adafruit KB2040.

Current capabilities:
- 8 footswitch inputs with debounce
- Per-switch MIDI action types: Note, CC, Program Change
- Momentary and toggle behaviors
- USB MIDI + DIN MIDI output
- OLED status display (SSD1306 128x64, I2C)
- NeoPixel switch LEDs + 2 status LEDs on same chain

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
