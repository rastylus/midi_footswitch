#include <Arduino.h>
#include "pico/bootrom.h"
#include <Adafruit_NeoPixel.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <MIDIUSB.h>

// ============================================================
// USER SETTINGS
// ============================================================

const int numSwitches = 8;

// ---------- Switch pin definitions ----------
#define SW_1 2
#define SW_2 3
#define SW_3 4
#define SW_4 5
#define SW_5 6
#define SW_6 7
#define SW_7 8
#define SW_8 9

// ---------- NeoPixel settings ----------
#define PIXEL_PIN 10
#define NUM_SWITCH_PIXELS 8
#define TOTAL_PIXELS 10
#define PIXEL_BRIGHTNESS 40

// ---------- Status NeoPixel indexes on same chain ----------
// If status LEDs are physically first in chain, set these to 0 and 1.
// If status LEDs are last in chain (after 8 switch LEDs), leave as 8 and 9.
#define STATUS_POWER_INDEX 8
#define STATUS_BANK_INDEX 9

// ============================================================
// TYPES / MODES
// ============================================================

enum MidiActionType {
  MIDI_NOTE,
  MIDI_CC,
  MIDI_PC
};

enum SwitchBehavior {
  BEHAVIOR_MOMENTARY,
  BEHAVIOR_TOGGLE
};

struct RgbColor {
  uint8_t r;
  uint8_t g;
  uint8_t b;
};

struct SwitchConfig {
  uint8_t pin;
  MidiActionType midiType;
  SwitchBehavior behavior;
  uint8_t number;      // note number, CC number, or PC number
  uint8_t channel;     // 1-16
  uint8_t onValue;     // velocity or CC on value
  uint8_t offValue;    // velocity or CC off value
  RgbColor onColor;
  RgbColor offColor;
};

// ============================================================
// SWITCH CONFIGURATION
// ============================================================

SwitchConfig switches[numSwitches] = {
  {SW_1, MIDI_CC,   BEHAVIOR_MOMENTARY, 20, 1, 127, 0, {0, 50, 0}, {0, 0, 0}},
  {SW_2, MIDI_CC,   BEHAVIOR_TOGGLE,    21, 1, 127, 0, {50, 0, 0}, {0, 0, 0}},
  {SW_3, MIDI_NOTE, BEHAVIOR_MOMENTARY, 62, 1, 127, 0, {0, 50, 0}, {0, 0, 0}},
  {SW_4, MIDI_NOTE, BEHAVIOR_MOMENTARY, 63, 1, 127, 0, {0, 50, 0}, {0, 0, 0}},
  {SW_5, MIDI_NOTE, BEHAVIOR_MOMENTARY, 64, 1, 127, 0, {0, 50, 0}, {0, 0, 0}},
  {SW_6, MIDI_NOTE, BEHAVIOR_MOMENTARY, 65, 1, 127, 0, {0, 50, 0}, {0, 0, 0}},
  {SW_7, MIDI_NOTE, BEHAVIOR_MOMENTARY, 66, 1, 127, 0, {0, 50, 0}, {0, 0, 0}},
  {SW_8, MIDI_NOTE, BEHAVIOR_MOMENTARY, 67, 1, 127, 0, {0, 50, 0}, {0, 0, 0}}
};

// ============================================================
// GLOBALS
// ============================================================

Adafruit_NeoPixel pixels(TOTAL_PIXELS, PIXEL_PIN, NEO_GRB + NEO_KHZ800);

bool lastPhysicalState[numSwitches];
bool currentPhysicalState[numSwitches];
bool toggleState[numSwitches];

unsigned long lastDebounceTime[numSwitches];
const unsigned long debounceDelay = 20; // ms

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

int currentBank = 1;
bool hasDisplay = false;

// ============================================================
// FORWARD DECLARATIONS (needed in .cpp, unlike .ino)
// ============================================================

void handlePress(int index);
void handleRelease(int index);
void setPixel(int index, const RgbColor &color);
void updateDisplay(int index, const SwitchConfig &sw, bool isOn);
void sendMidiOn(const SwitchConfig &sw);
void sendMidiOff(const SwitchConfig &sw);
void sendNoteOnBoth(uint8_t note, uint8_t velocity, uint8_t channel);
void sendNoteOffBoth(uint8_t note, uint8_t velocity, uint8_t channel);
void sendCcBoth(uint8_t ccNumber, uint8_t value, uint8_t channel);
void sendProgramChangeBoth(uint8_t programNumber, uint8_t channel);
void updateStatusPixels();

// ============================================================
// SETUP
// ============================================================

void setup() {
  pinMode(SW_1, INPUT_PULLUP);
  delay(50);

  // Hold SW1 during reset/power-up to enter UF2 bootloader
  if (digitalRead(SW_1) == LOW) {
    reset_usb_boot(0, 0);
  }

  for (int i = 0; i < numSwitches; i++) {
    pinMode(switches[i].pin, INPUT_PULLUP);

    bool initialState = digitalRead(switches[i].pin);
    lastPhysicalState[i] = initialState;
    currentPhysicalState[i] = initialState;
    lastDebounceTime[i] = 0;
    toggleState[i] = false;
  }

  pixels.begin();
  pixels.setBrightness(PIXEL_BRIGHTNESS);
  pixels.clear();
  pixels.show();
  updateStatusPixels();

  MidiUSB.begin();
  Serial1.begin(31250);
  delay(1000);

  hasDisplay = display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  if (hasDisplay) {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.println("FOOTSWITCH READY");
    display.display();
    delay(1000);
  }
}

// ============================================================
// MAIN LOOP
// ============================================================

void loop() {
  for (int i = 0; i < numSwitches; i++) {
    bool reading = digitalRead(switches[i].pin);

    // If raw reading changed, reset debounce timer
    if (reading != currentPhysicalState[i]) {
      currentPhysicalState[i] = reading;
      lastDebounceTime[i] = millis();
    }

    // If the reading has stayed stable long enough, accept it
    if ((millis() - lastDebounceTime[i]) > debounceDelay) {
      if (currentPhysicalState[i] != lastPhysicalState[i]) {
        lastPhysicalState[i] = currentPhysicalState[i];

        // INPUT_PULLUP: LOW = pressed, HIGH = released
        if (lastPhysicalState[i] == LOW) {
          handlePress(i);
        } else {
          handleRelease(i);
        }
      }
    }
  }
}

// ============================================================
// EVENT HANDLERS
// ============================================================

void handlePress(int index) {
  SwitchConfig &sw = switches[index];

  if (sw.behavior == BEHAVIOR_MOMENTARY) {
    sendMidiOn(sw);
    setPixel(index, sw.onColor);
    updateDisplay(index, sw, true);
  } else if (sw.behavior == BEHAVIOR_TOGGLE) {
    toggleState[index] = !toggleState[index];

    if (toggleState[index]) {
      sendMidiOn(sw);
      setPixel(index, sw.onColor);
    } else {
      sendMidiOff(sw);
      setPixel(index, sw.offColor);
    }

    updateDisplay(index, sw, toggleState[index]);
  }
}

void handleRelease(int index) {
  SwitchConfig &sw = switches[index];

  if (sw.behavior == BEHAVIOR_MOMENTARY) {
    sendMidiOff(sw);
    setPixel(index, sw.offColor);
    updateDisplay(index, sw, false);
  }
  // Toggle behavior ignores release
}

// ============================================================
// PIXEL HELPERS
// ============================================================

void setPixel(int index, const RgbColor &color) {
  if (index < NUM_SWITCH_PIXELS) {
    pixels.setPixelColor(index, pixels.Color(color.r, color.g, color.b));
    pixels.show();
  }
}

void updateDisplay(int index, const SwitchConfig &sw, bool isOn) {
  if (!hasDisplay) {
    return;
  }

  display.clearDisplay();

  display.setCursor(0, 0);
  display.print("BANK ");
  display.println(currentBank);

  display.setCursor(0, 16);
  display.print("SW");
  display.print(index + 1);
  display.print(" ");

  if (sw.midiType == MIDI_NOTE) {
    display.print("NOTE ");
    display.print(sw.number);
  } else if (sw.midiType == MIDI_CC) {
    display.print("CC ");
    display.print(sw.number);
  } else if (sw.midiType == MIDI_PC) {
    display.print("PC ");
    display.print(sw.number);
  }

  display.setCursor(0, 32);
  display.println(isOn ? "ON" : "OFF");

  display.display();
}

void updateStatusPixels() {
  uint32_t bankColor = pixels.Color(0, 0, 0);

  if (currentBank == 1) {
    bankColor = pixels.Color(0, 50, 0);   // Green
  } else if (currentBank == 2) {
    bankColor = pixels.Color(50, 50, 0);  // Yellow
  } else if (currentBank == 3) {
    bankColor = pixels.Color(0, 0, 50);   // Blue
  }

  pixels.setPixelColor(STATUS_POWER_INDEX, pixels.Color(50, 0, 0));
  pixels.setPixelColor(STATUS_BANK_INDEX, bankColor);
  pixels.show();
}

// ============================================================
// MIDI DISPATCH
// ============================================================

void sendMidiOn(const SwitchConfig &sw) {
  switch (sw.midiType) {
    case MIDI_NOTE:
      sendNoteOnBoth(sw.number, sw.onValue, sw.channel);
      break;

    case MIDI_CC:
      sendCcBoth(sw.number, sw.onValue, sw.channel);
      break;

    case MIDI_PC:
      sendProgramChangeBoth(sw.number, sw.channel);
      break;
  }
}

void sendMidiOff(const SwitchConfig &sw) {
  switch (sw.midiType) {
    case MIDI_NOTE:
      sendNoteOffBoth(sw.number, sw.offValue, sw.channel);
      break;

    case MIDI_CC:
      sendCcBoth(sw.number, sw.offValue, sw.channel);
      break;

    case MIDI_PC:
      // Usually no "off" for program change
      break;
  }
}

// ============================================================
// LOW-LEVEL MIDI SENDERS
// ============================================================

void sendNoteOnBoth(uint8_t note, uint8_t velocity, uint8_t channel) {
  midiEventPacket_t packet = {
    0x09,
    (uint8_t)(0x90 | ((channel - 1) & 0x0F)),
    note,
    velocity
  };
  MidiUSB.sendMIDI(packet);
  MidiUSB.flush();

  Serial1.write((uint8_t)(0x90 | ((channel - 1) & 0x0F)));
  Serial1.write(note);
  Serial1.write(velocity);
}

void sendNoteOffBoth(uint8_t note, uint8_t velocity, uint8_t channel) {
  midiEventPacket_t packet = {
    0x08,
    (uint8_t)(0x80 | ((channel - 1) & 0x0F)),
    note,
    velocity
  };
  MidiUSB.sendMIDI(packet);
  MidiUSB.flush();

  Serial1.write((uint8_t)(0x80 | ((channel - 1) & 0x0F)));
  Serial1.write(note);
  Serial1.write(velocity);
}

void sendCcBoth(uint8_t ccNumber, uint8_t value, uint8_t channel) {
  midiEventPacket_t packet = {
    0x0B,
    (uint8_t)(0xB0 | ((channel - 1) & 0x0F)),
    ccNumber,
    value
  };
  MidiUSB.sendMIDI(packet);
  MidiUSB.flush();

  Serial1.write((uint8_t)(0xB0 | ((channel - 1) & 0x0F)));
  Serial1.write(ccNumber);
  Serial1.write(value);
}

void sendProgramChangeBoth(uint8_t programNumber, uint8_t channel) {
  midiEventPacket_t packet = {
    0x0C,
    (uint8_t)(0xC0 | ((channel - 1) & 0x0F)),
    programNumber,
    0x00
  };
  MidiUSB.sendMIDI(packet);
  MidiUSB.flush();

  Serial1.write((uint8_t)(0xC0 | ((channel - 1) & 0x0F)));
  Serial1.write(programNumber);
}
