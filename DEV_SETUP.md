# Development Setup Guide

This guide covers setting up the MIDI footswitch project on Windows, macOS, and Linux.

## Quick Summary

- **Hardware**: Adafruit KB2040 (RP2040 microcontroller)
- **Build System**: PlatformIO
- **Display**: Monochrome SSD1306 OLED (128x64)
- **Expansion**: Prepared for future SSD1331 color OLED migration
- **Firmware**: C++ with Arduino framework

## Prerequisites

### Required
- **Python 3.6+** (check: `python3 --version`)
- **Visual Studio Code** (https://code.microsoft.com)
- **Git** (for cloning and version control)

### Optional (for hardware upload)
- **USB cable** to connect KB2040 to computer

## Installation Steps

### 1. Install VSCode & PlatformIO Extension

1. Download and install [Visual Studio Code](https://code.microsoft.com)
2. Open VSCode → Extensions (left sidebar or `Ctrl+Shift+X` / `Cmd+Shift+X`)
3. Search for **"PlatformIO IDE"** (by PlatformIO)
4. Click Install
5. PlatformIO will auto-install dependencies on first use (~5 min)
6. Reload VSCode after installation completes

### 2. Clone the Repository

```bash
git clone https://github.com/rastylus/midi_footswitch.git
cd midi_footswitch
```

### 3. Open Project in VSCode

```bash
code .
```

Or from VSCode: File → Open Folder → select `midi_footswitch`

### 4. Build the Project

PlatformIO will automatically:
- Detect `platformio.ini`
- Install the board framework (Raspberry Pi RP2040)
- Fetch all library dependencies (listed in `lib_deps`)

To build:
- **Via UI**: Click PlatformIO icon (left sidebar) → Project Tasks → `kb2040` → Build
- **Via Terminal**: `platformio run -e kb2040`

Expected output on success:
```
Building .pio/build/kb2040/firmware.elf
...
[SUCCESS] Took XX.XX seconds
```

### 5. Upload to Hardware

**Prerequisites:**
- Connect KB2040 via USB
- Enter bootloader mode: hold **SW1** while connecting USB (or hold during power-up)

**Upload:**
- **Via UI**: PlatformIO → Project Tasks → `kb2040` → Upload
- **Via Terminal**: `platformio run -e kb2040 --target upload`

The device will auto-reboot after upload completes.

## Library Management

All dependencies are defined in `platformio.ini` under `lib_deps`:

```ini
lib_deps =
  fortyseveneffects/MIDI Library
  adafruit/Adafruit GFX Library
  adafruit/Adafruit SSD1306
  adafruit/Adafruit SSD1331 OLED Driver Library for Arduino
  adafruit/Adafruit NeoPixel
```

PlatformIO fetches these automatically on first build. No manual installation needed.

**To update library versions:**
1. Edit `platformio.ini` with a specific version: `adafruit/Adafruit SSD1306@1.10.0`
2. Rebuild — PlatformIO will fetch the specified version

## Project Structure

```
midi_footswitch/
├── src/
│   └── 8switchNEOpixel.cpp          # Main firmware
├── include/
│   └── README
├── lib/
│   └── README
├── test/
│   └── README
├── platformio.ini                    # Build configuration & dependencies
├── README.md                         # Project overview
├── ROADMAP.md                        # Feature roadmap
└── DEV_SETUP.md                      # This file
```

## Key Features

- **8 Momentary Footswitches** with configurable MIDI actions (NOTE/CC/PC/TRANSPORT)
- **8 Banks** with preset layouts (Ableton-optimized defaults included)
- **Bank Cycling**: Two-left chord (SW1+SW2) to bank down, two-right chord (SW7+SW8) to bank up
- **OLED Display**: Real-time status, editing interface, performance mode
- **NeoPixel LEDs**: Per-MIDI-type colors (green=NOTE, blue=CC, amber=PC, magenta=TRANSPORT)
- **MIDI Output**: USB + DIN serial (31250 baud)
- **Persistence**: EEPROM-backed config with versioning

## Troubleshooting

### Build Fails: "UnknownPackageError"
- Library name/version in `platformio.ini` is incorrect
- Check exact package IDs: `platformio pkg search <name>`
- Ensure internet connection for first-time library fetch

### Upload Fails: "No boards found"
- KB2040 not detected by system
- **Solution**: Hold SW1 while plugging in USB to enter UF2 bootloader mode
- Check Device Manager (Windows) or `lsusb` (macOS/Linux) for "RPI-RP2" device

### Changes Not Taking Effect
- Always rebuild after edits: `platformio run -e kb2040`
- Don't just upload old builds

## For Mac Users Specifically

After cloning on your Mac:

1. **Install Homebrew** (if needed): `/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"`
2. **Install Python**: `brew install python3`
3. **Install VSCode** via Homebrew or download directly
4. Follow the standard setup above — everything else is identical

## Next Steps

- Read `README.md` for feature overview and pinout
- Check `ROADMAP.md` for planned features and completed milestones
- Explore the firmware in `src/8switchNEOpixel.cpp` for customization options

## Questions?

Refer to:
- [PlatformIO Docs](https://docs.platformio.org)
- [Adafruit KB2040 Board Support](https://github.com/adafruit/ArduinoCore-RP2040)
- [RP2040 Datasheet](https://datasheets.raspberrypi.com/rp2040/rp2040-datasheet.pdf)
