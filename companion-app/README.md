# MIDI Footswitch Companion (Vite)

Web companion for editing bank/switch mappings used by the KB2040 firmware.

## Features

- 8 banks x 8 switches editor
- MIDI type, behavior, number, channel, on/off value controls
- Factory default profile generator matching firmware defaults
- JSON export/import for backup and sharing
- Local autosave in browser storage

## Run

```bash
npm install
npm run dev
```

Open the URL shown by Vite (typically http://localhost:5173).

## Build

```bash
npm run build
npm run preview
```

## Notes

- The app is currently offline and browser-only.
- JSON schema is intentionally simple so it can be integrated into future firmware tooling.
