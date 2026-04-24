# Roadmap

## Current Baseline (Completed)

- [x] PlatformIO project builds and uploads on KB2040
- [x] USB MIDI + DIN MIDI output
- [x] OLED display updates for switch actions
- [x] NeoPixel switch indicators
- [x] Single-chain status LEDs (power + bank)
- [x] Encoder-driven settings UI (bank, type, number, channel, behavior)
- [x] Bank switching with multi-bank mappings
- [x] EEPROM settings memory with restore on boot
- [x] Cascade bank preset layout + reset action
- [x] Adjustable NeoPixel brightness from UI
- [x] Git repository initialized and pushed

## Near-Term

### 1) Bank Switching
- [x] Implement bank up/down actions
- [x] Define bank-switch controls (encoder UI)
- [x] Persist current bank in RAM runtime state
- [x] Update OLED and bank status LED on bank change

### 2) Config Organization
- [ ] Move switch config data to dedicated module/header
- [ ] Add clear constants for channel, velocity defaults, colors
- [ ] Add optional compile-time profile selection

### 3) UX Improvements
- [ ] Add startup splash with firmware version
- [ ] Add brief MIDI send feedback on OLED
- [x] Add configurable LED brightness for stage use

## Mid-Term

### 4) Persistence
- [x] Save user settings in non-volatile storage (EEPROM emulation)
- [ ] Restore last-used bank on boot

### 5) MIDI Features
- [ ] Long-press / double-tap actions
- [ ] Optional MIDI clock/start/stop controls
- [ ] Optional expression pedal support

### 6) Reliability
- [ ] Add watchdog-safe loop structure
- [ ] Add input diagnostics mode (serial + OLED)
- [ ] Add simple test checklist for hardware bring-up

## Long-Term

### 7) Architecture Refactor
- [ ] Split monolithic source into modules:
  - input scanning/debounce
  - MIDI transport
  - display rendering
  - LED rendering
  - config and bank engine

### 8) Release Readiness
- [ ] Changelog
- [ ] Wiring diagram docs
- [ ] Tagged release artifacts and release notes

## Suggested Milestones

- `v0.1.0`: Current stable baseline
- `v0.2.0`: Bank switching + improved UX
- `v0.3.0`: Persistence + expanded MIDI actions
- `v1.0.0`: Stable modular architecture + release docs
