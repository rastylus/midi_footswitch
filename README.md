# MIDI Footswitch (KB2040)

Custom RP2040 MIDI footswitch firmware built with PlatformIO for Adafruit KB2040.

Current capabilities:
- 8 footswitch inputs with debounce
- Per-switch MIDI action types: Note, CC, Program Change, Transport
- Momentary and toggle behaviors
- USB MIDI + DIN MIDI output
- OLED status display with selectable backends:
  - SSD1306 128x64 (I2C)
  - SSD1331 96x64 (SPI color)
  - Mirror mode (SSD1306 + SSD1331 together)
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
- OLED backend support:
  - SSD1306 (I2C, default) at address 0x3C
  - SSD1331 (SPI, optional via build flag)
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
- `GP27` -> Encoder CLK (`ENC_CLK`)
- `GP28` -> Encoder DT (`ENC_DT`)
- `GP29` -> Encoder push switch (`ENC_SW`)

OLED wiring (SSD1306 I2C, default backend):
- `GP12` / `SDA` -> OLED SDA
- `GP13` / `SCL` -> OLED SCL

OLED wiring (SSD1331 SPI backend):
- `GP18` / `SCK` -> OLED `SCL` (clock)
- `GP19` / `MOSI` -> OLED `SDA` (data)
- `GP20` -> OLED `DC`
- `GP26` -> OLED `CS`
- `GP1` -> OLED `RST`

Notes:
- Switches are configured as `INPUT_PULLUP` (pressed = `LOW`).
- NeoPixel status indices are currently `8` (power) and `9` (bank).
- USB MIDI is over the native USB port; DIN MIDI uses `Serial1` in firmware.
- Encoder pins were moved from `GP19/GP20/GP18` to `GP27/GP28/GP29` to free SPI-capable pins for upcoming OLED options.
- SSD1331 SPI pins above match `src/display/DisplayBackend.h` defaults.

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

### Display Backend Selection

Set one backend flag in [platformio.ini](platformio.ini):

- `-D DISPLAY_BACKEND_SSD1306=1` (default, I2C mono OLED)
- `-D DISPLAY_BACKEND_SSD1331=1` (SPI color OLED only)
- `-D DISPLAY_BACKEND_MIRROR=1` (drive both OLEDs at once)

Use only one of the three at a time.

Notes:
- `DISPLAY_BACKEND_MIRROR` drives both displays with the same UI frame.
- SSD1331 rendering includes an offscreen canvas flush path to reduce visible redraw flicker during rapid updates.

### SSD1331 Brightness Tuning

When using `DISPLAY_BACKEND_SSD1331` or `DISPLAY_BACKEND_MIRROR`, brightness is tuned in [platformio.ini](platformio.ini) via build flags:

- `OLED_SSD1331_MASTER_CURRENT` range `0x00..0x0F` (primary brightness control)
- `OLED_SSD1331_CONTRAST_A` range `0x00..0xFF`
- `OLED_SSD1331_CONTRAST_B` range `0x00..0xFF`
- `OLED_SSD1331_CONTRAST_C` range `0x00..0xFF`

Yes: this is a simple number increase. Start by increasing only `OLED_SSD1331_MASTER_CURRENT`.

Example progression:

1. `0x0A` (current default)
2. `0x0C`
3. `0x0F` (max)

If still dim at `0x0F`, raise contrast values in small steps (for example `+0x10`).

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
