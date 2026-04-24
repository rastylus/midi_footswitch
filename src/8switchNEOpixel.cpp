#include <Arduino.h>
#include "pico/bootrom.h"
#include <Adafruit_NeoPixel.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <MIDIUSB.h>
#include <EEPROM.h>

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

// ---------- Rotary encoder ----------
#define ENC_CLK 19
#define ENC_DT  20
#define ENC_SW  18
#define NUM_BANKS 8

#define EEPROM_SIZE_BYTES 512
#define EEPROM_MAGIC 0x4D465357u
#define EEPROM_VERSION 3u

// ---------- NeoPixel settings ----------
#define PIXEL_PIN 10
#define NUM_SWITCH_PIXELS 8
#define TOTAL_PIXELS 10
#define PIXEL_BRIGHTNESS_LEVEL 5

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

enum EditState {
  EDIT_NAVIGATE,
  EDIT_VALUE
};

enum EditField {
  FIELD_BANK = 0,
  FIELD_SWITCH,
  FIELD_SWITCH_CHANNEL,
  FIELD_BEHAVIOR,
  FIELD_TYPE,
  FIELD_NUMBER,
  FIELD_BRIGHTNESS,
  FIELD_CHANNEL,
  FIELD_RESET,
  FIELD_COUNT
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

SwitchConfig defaultSwitches[numSwitches] = {
  {SW_1, MIDI_CC,   BEHAVIOR_MOMENTARY, 20, 1, 127, 0, {0, 50, 0}, {0, 0, 0}},
  {SW_2, MIDI_CC,   BEHAVIOR_TOGGLE,    21, 1, 127, 0, {50, 0, 0}, {0, 0, 0}},
  {SW_3, MIDI_NOTE, BEHAVIOR_MOMENTARY, 62, 1, 127, 0, {0, 50, 0}, {0, 0, 0}},
  {SW_4, MIDI_NOTE, BEHAVIOR_MOMENTARY, 63, 1, 127, 0, {0, 50, 0}, {0, 0, 0}},
  {SW_5, MIDI_NOTE, BEHAVIOR_MOMENTARY, 64, 1, 127, 0, {0, 50, 0}, {0, 0, 0}},
  {SW_6, MIDI_NOTE, BEHAVIOR_MOMENTARY, 65, 1, 127, 0, {0, 50, 0}, {0, 0, 0}},
  {SW_7, MIDI_NOTE, BEHAVIOR_MOMENTARY, 66, 1, 127, 0, {0, 50, 0}, {0, 0, 0}},
  {SW_8, MIDI_NOTE, BEHAVIOR_MOMENTARY, 67, 1, 127, 0, {0, 50, 0}, {0, 0, 0}}
};

SwitchConfig bankSwitches[NUM_BANKS][numSwitches];

struct PersistHeader {
  uint32_t magic;
  uint16_t version;
  uint16_t banks;
  uint16_t switches;
  uint16_t reserved;
  uint32_t checksum;
};

struct PersistData {
  uint8_t globalChannel;
  uint8_t pixelBrightness;
  uint8_t reserved[2];
  uint8_t behavior[NUM_BANKS][numSwitches];
  uint8_t channel[NUM_BANKS][numSwitches];
  uint8_t midiType[NUM_BANKS][numSwitches];
  uint8_t number[NUM_BANKS][numSwitches];
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

// Encoder state (updated in ISR)
volatile int encoderDelta = 0;   // accumulated +/- ticks
static volatile bool lastClk = HIGH;
const int encoderStepsPerDetent = 2;

// Encoder button debounce
bool encSwLastReading = HIGH;
bool encSwStableState = HIGH;
unsigned long encSwLastChangeTime = 0;
const unsigned long encSwDebounce = 30;

bool resetHoldActive = false;
bool resetHoldTriggered = false;
unsigned long resetHoldStartMs = 0;
const unsigned long resetHoldMs = 2400;
int resetHoldProgressPx = -1;

const int holdBarX = 12;
const int holdBarY = 56;
const int holdBarW = 116;
const int holdBarH = 8;
const int resetTextY = 48;

unsigned long resetMsgUntilMs = 0;
const unsigned long resetMsgDurationMs = 900;

// Editor state
EditState editState = EDIT_NAVIGATE;
int selectedField = FIELD_BANK;
int globalMidiChannel = 1;
int selectedSwitch = 0;
int pixelBrightness = PIXEL_BRIGHTNESS_LEVEL;

// Last pressed switch (for display)
int lastPressedSwitch = -1;
bool lastPressedIsOn = false;

// EEPROM persistence state
bool configDirty = false;
unsigned long lastConfigChangeMs = 0;
const unsigned long configSaveDelayMs = 1500;

const EditField navigationOrder[] = {
  FIELD_BANK,
  FIELD_CHANNEL,
  FIELD_RESET,
  FIELD_SWITCH,
  FIELD_SWITCH_CHANNEL,
  FIELD_BEHAVIOR,
  FIELD_TYPE,
  FIELD_NUMBER,
  FIELD_BRIGHTNESS
};
const int navigationFieldCount = sizeof(navigationOrder) / sizeof(navigationOrder[0]);

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
void changeBank(int newBank);
void encoderISR();
void redrawDisplay();
void drawField(int16_t x, int16_t y, const char* label, int value, bool isSelected, bool isEditing);
void drawFieldText(int16_t x, int16_t y, const char* label, const char* value, bool isSelected, bool isEditing);
void adjustSelectedField(int delta);
const char* midiTypeName(MidiActionType type);
const char* behaviorName(SwitchBehavior behavior);
SwitchConfig& activeSwitch(int index);
void buildCascadeBankPresets();
void applyCascadeFactoryReset();
uint8_t brightnessValueFromLevel(int level);
int brightnessLevelFromStored(uint8_t storedValue);
void applyPixelBrightness();
uint32_t calcChecksum(const uint8_t* data, size_t len);
void markConfigDirty();
void saveConfigToEeprom();
bool loadConfigFromEeprom();
EditField stepNavigationField(EditField current, int step);

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

  buildCascadeBankPresets();

  for (int i = 0; i < numSwitches; i++) {
    pinMode(defaultSwitches[i].pin, INPUT_PULLUP);

    bool initialState = digitalRead(defaultSwitches[i].pin);
    lastPhysicalState[i] = initialState;
    currentPhysicalState[i] = initialState;
    lastDebounceTime[i] = 0;
    toggleState[i] = false;
  }

  EEPROM.begin(EEPROM_SIZE_BYTES);
  loadConfigFromEeprom();

  // Encoder setup
  pinMode(ENC_CLK, INPUT_PULLUP);
  pinMode(ENC_DT,  INPUT_PULLUP);
  pinMode(ENC_SW,  INPUT_PULLUP);
  lastClk = digitalRead(ENC_CLK);
  attachInterrupt(digitalPinToInterrupt(ENC_CLK), encoderISR, CHANGE);

  pixels.begin();
  applyPixelBrightness();
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
    redrawDisplay();
  }
}

// ============================================================
// MAIN LOOP
// ============================================================

void loop() {
  // --- Encoder rotation ---
  if (encoderDelta != 0) {
    noInterrupts();
    int rawDelta = encoderDelta;
    encoderDelta = 0;
    interrupts();

    static int partialSteps = 0;
    partialSteps += rawDelta;

    while (partialSteps >= encoderStepsPerDetent || partialSteps <= -encoderStepsPerDetent) {
      int step = (partialSteps > 0) ? 1 : -1;
      partialSteps -= step * encoderStepsPerDetent;

      if (editState == EDIT_NAVIGATE) {
        selectedField = (int)stepNavigationField((EditField)selectedField, step);
        redrawDisplay();
      } else {
        adjustSelectedField(step);
      }
    }
  }

  // --- Encoder button: short press toggles edit, reset symbol uses hold ---
  bool encSwReading = digitalRead(ENC_SW);
  if (encSwReading != encSwLastReading) {
    encSwLastReading = encSwReading;
    encSwLastChangeTime = millis();
  }

  if ((millis() - encSwLastChangeTime) > encSwDebounce && encSwStableState != encSwLastReading) {
    encSwStableState = encSwLastReading;
    if (encSwStableState == LOW) {
      if (editState == EDIT_NAVIGATE && selectedField == FIELD_RESET) {
        resetHoldActive = true;
        resetHoldTriggered = false;
        resetHoldStartMs = millis();
        resetHoldProgressPx = 0;
        redrawDisplay();
      } else {
        if (editState == EDIT_VALUE) {
          markConfigDirty();
        }
        editState = (editState == EDIT_NAVIGATE) ? EDIT_VALUE : EDIT_NAVIGATE;
        redrawDisplay();
      }
    } else {
      if (resetHoldActive && !resetHoldTriggered) {
        resetHoldProgressPx = -1;
        redrawDisplay();
      }
      resetHoldActive = false;
    }
  }

  if (resetHoldActive && !resetHoldTriggered && encSwStableState == LOW) {
    int innerW = holdBarW - 2;
    unsigned long heldMs = millis() - resetHoldStartMs;
    int progressPx = (int)((heldMs * (unsigned long)innerW) / resetHoldMs);
    if (progressPx > innerW) progressPx = innerW;
    if (progressPx != resetHoldProgressPx) {
      resetHoldProgressPx = progressPx;
      redrawDisplay();
    }

    if (heldMs >= resetHoldMs) {
      resetHoldTriggered = true;
      resetHoldActive = false;
      resetHoldProgressPx = -1;
      applyCascadeFactoryReset();
    }
  }

  if (configDirty && (millis() - lastConfigChangeMs) > configSaveDelayMs) {
    saveConfigToEeprom();
  }

  for (int i = 0; i < numSwitches; i++) {
    bool reading = digitalRead(defaultSwitches[i].pin);

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
  SwitchConfig &sw = activeSwitch(index);

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
  SwitchConfig &sw = activeSwitch(index);

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
  (void)sw;
  selectedSwitch = index;
  lastPressedSwitch = index;
  lastPressedIsOn = isOn;
  redrawDisplay();
}

// ============================================================
// ENCODER
// ============================================================

void encoderISR() {
  bool clk = digitalRead(ENC_CLK);
  if (clk != lastClk) {
    bool dt = digitalRead(ENC_DT);
    encoderDelta += (dt != clk) ? 1 : -1;
    lastClk = clk;
  }
}

void drawField(int16_t x, int16_t y, const char* label, int value, bool isSelected, bool isEditing) {
  char buf[20];
  snprintf(buf, sizeof(buf), "%s%d", label, value);

  if (isSelected && isEditing) {
    display.setTextColor(SSD1306_BLACK, SSD1306_WHITE);
    display.setCursor(x, y);
    display.print(buf);
    display.setTextColor(SSD1306_WHITE, SSD1306_BLACK);
  } else {
    display.setTextColor(SSD1306_WHITE, SSD1306_BLACK);
    display.setCursor(x, y);
    display.print(buf);
    if (isSelected) {
      display.drawFastHLine(x, y + 7, (int16_t)(strlen(buf) * 6), SSD1306_WHITE);
    }
  }
}

void drawFieldText(int16_t x, int16_t y, const char* label, const char* value, bool isSelected, bool isEditing) {
  char buf[24];
  snprintf(buf, sizeof(buf), "%s%s", label, value);

  if (isSelected && isEditing) {
    display.setTextColor(SSD1306_BLACK, SSD1306_WHITE);
    display.setCursor(x, y);
    display.print(buf);
    display.setTextColor(SSD1306_WHITE, SSD1306_BLACK);
  } else {
    display.setTextColor(SSD1306_WHITE, SSD1306_BLACK);
    display.setCursor(x, y);
    display.print(buf);
    if (isSelected) {
      display.drawFastHLine(x, y + 7, (int16_t)(strlen(buf) * 6), SSD1306_WHITE);
    }
  }
}

const char* midiTypeName(MidiActionType type) {
  switch (type) {
    case MIDI_NOTE: return "NOTE";
    case MIDI_CC:   return "CC";
    case MIDI_PC:   return "PC";
    default:        return "?";
  }
}

const char* behaviorName(SwitchBehavior behavior) {
  switch (behavior) {
    case BEHAVIOR_MOMENTARY: return "MOMENTARY";
    case BEHAVIOR_TOGGLE:    return "TOGGLE";
    default:                 return "?";
  }
}

SwitchConfig& activeSwitch(int index) {
  return bankSwitches[currentBank - 1][index];
}

uint8_t brightnessValueFromLevel(int level) {
  static const uint8_t brightnessTable[11] = {0, 4, 8, 14, 24, 40, 64, 96, 136, 184, 255};

  if (level < 0) {
    level = 0;
  }
  if (level > 10) {
    level = 10;
  }

  return brightnessTable[level];
}

int brightnessLevelFromStored(uint8_t storedValue) {
  if (storedValue <= 10) {
    return storedValue;
  }

  int bestLevel = 0;
  int bestDiff = 255;
  for (int level = 0; level <= 10; level++) {
    int diff = abs((int)brightnessValueFromLevel(level) - (int)storedValue);
    if (diff < bestDiff) {
      bestDiff = diff;
      bestLevel = level;
    }
  }

  return bestLevel;
}

void applyPixelBrightness() {
  pixels.setBrightness(brightnessValueFromLevel(pixelBrightness));
  updateStatusPixels();
}

void buildCascadeBankPresets() {
  // Each bank starts from defaults, then cascades MIDI numbers by 8 per bank.
  for (int bank = 0; bank < NUM_BANKS; bank++) {
    int offset = bank * numSwitches;
    for (int i = 0; i < numSwitches; i++) {
      bankSwitches[bank][i] = defaultSwitches[i];

      int shifted = (int)defaultSwitches[i].number + offset;
      while (shifted > 127) {
        shifted -= 128;
      }
      bankSwitches[bank][i].number = (uint8_t)shifted;
    }
  }
}

void applyCascadeFactoryReset() {
  buildCascadeBankPresets();
  globalMidiChannel = 1;
  currentBank = 1;
  selectedSwitch = 0;
  selectedField = FIELD_BANK;
  editState = EDIT_NAVIGATE;
  lastPressedSwitch = -1;
  lastPressedIsOn = false;

  for (int i = 0; i < numSwitches; i++) {
    toggleState[i] = false;
  }

  markConfigDirty();
  saveConfigToEeprom();
  resetMsgUntilMs = millis() + resetMsgDurationMs;
  updateStatusPixels();
  redrawDisplay();
}

uint32_t calcChecksum(const uint8_t* data, size_t len) {
  uint32_t hash = 2166136261u;
  for (size_t i = 0; i < len; i++) {
    hash ^= data[i];
    hash *= 16777619u;
  }
  return hash;
}

void markConfigDirty() {
  configDirty = true;
  lastConfigChangeMs = millis();
}

void saveConfigToEeprom() {
  PersistHeader header;
  PersistData data;

  data.globalChannel = (uint8_t)globalMidiChannel;
  data.pixelBrightness = (uint8_t)pixelBrightness;
  data.reserved[0] = 0;
  data.reserved[1] = 0;

  for (int bank = 0; bank < NUM_BANKS; bank++) {
    for (int i = 0; i < numSwitches; i++) {
      data.behavior[bank][i] = (uint8_t)bankSwitches[bank][i].behavior;
      data.channel[bank][i] = bankSwitches[bank][i].channel;
      data.midiType[bank][i] = (uint8_t)bankSwitches[bank][i].midiType;
      data.number[bank][i] = bankSwitches[bank][i].number;
    }
  }

  header.magic = EEPROM_MAGIC;
  header.version = EEPROM_VERSION;
  header.banks = NUM_BANKS;
  header.switches = numSwitches;
  header.reserved = 0;
  header.checksum = calcChecksum((const uint8_t*)&data, sizeof(data));

  EEPROM.put(0, header);
  EEPROM.put((int)sizeof(header), data);
  EEPROM.commit();
  configDirty = false;
}

bool loadConfigFromEeprom() {
  PersistHeader header;
  PersistData data;

  EEPROM.get(0, header);
  if (header.magic != EEPROM_MAGIC ||
      header.version != EEPROM_VERSION ||
      header.banks != NUM_BANKS ||
      header.switches != numSwitches) {
    return false;
  }

  EEPROM.get((int)sizeof(header), data);
  uint32_t checksum = calcChecksum((const uint8_t*)&data, sizeof(data));
  if (checksum != header.checksum) {
    return false;
  }

  if (data.globalChannel >= 1 && data.globalChannel <= 16) {
    globalMidiChannel = data.globalChannel;
  }
  if (data.pixelBrightness <= 255) {
    pixelBrightness = brightnessLevelFromStored(data.pixelBrightness);
  }

  for (int bank = 0; bank < NUM_BANKS; bank++) {
    for (int i = 0; i < numSwitches; i++) {
      uint8_t bh = data.behavior[bank][i];
      if (bh <= (uint8_t)BEHAVIOR_TOGGLE) {
        bankSwitches[bank][i].behavior = (SwitchBehavior)bh;
      }

      uint8_t ch = data.channel[bank][i];
      if (ch >= 1 && ch <= 16) {
        bankSwitches[bank][i].channel = ch;
      }

      uint8_t mt = data.midiType[bank][i];
      if (mt <= (uint8_t)MIDI_PC) {
        bankSwitches[bank][i].midiType = (MidiActionType)mt;
      }
      bankSwitches[bank][i].number = data.number[bank][i];
    }
  }

  return true;
}

EditField stepNavigationField(EditField current, int step) {
  int idx = 0;
  for (int i = 0; i < navigationFieldCount; i++) {
    if (navigationOrder[i] == current) {
      idx = i;
      break;
    }
  }

  idx += (step > 0) ? 1 : -1;
  if (idx < 0) idx = navigationFieldCount - 1;
  if (idx >= navigationFieldCount) idx = 0;
  return navigationOrder[idx];
}

void redrawDisplay() {
  if (!hasDisplay) return;

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE, SSD1306_BLACK);

  bool editing = (editState == EDIT_VALUE);
  SwitchConfig &selectedSw = activeSwitch(selectedSwitch);

  // Top line: bank + global channel
  drawField(0, 0, "BANK:", currentBank, selectedField == FIELD_BANK, editing);
  drawField(56, 0, "GCH:", globalMidiChannel, selectedField == FIELD_CHANNEL, editing);
  drawFieldText(112, 0, "", "*", selectedField == FIELD_RESET, editing);

  // Second line: selected switch + per-switch channel + behavior
  drawField(0, 16, "SW:", selectedSwitch + 1, selectedField == FIELD_SWITCH, editing);
  drawField(30, 16, "SCH:", selectedSw.channel, selectedField == FIELD_SWITCH_CHANNEL, editing);
  drawFieldText(72, 16, "", behaviorName(selectedSw.behavior), selectedField == FIELD_BEHAVIOR, editing);

  // Third line: TYPE left of NUM
  drawFieldText(0, 32, "TYPE:", midiTypeName(selectedSw.midiType), selectedField == FIELD_TYPE, editing);
  drawField(72, 32, "NUM:", selectedSw.number, selectedField == FIELD_NUMBER, editing);

  // Bottom line: reset message or last pressed state
  if (millis() < resetMsgUntilMs) {
    display.setTextColor(SSD1306_WHITE, SSD1306_BLACK);
    display.setCursor(0, resetTextY);
    display.print("* CASCADE RESET");
  } else if (resetHoldActive && !resetHoldTriggered && encSwStableState == LOW) {
    display.setTextColor(SSD1306_WHITE, SSD1306_BLACK);
    display.setCursor(0, resetTextY);
    display.print("RESET LAYOUT");
    display.drawRect(holdBarX, holdBarY, holdBarW, holdBarH, SSD1306_WHITE);
    if (resetHoldProgressPx > 0) {
      display.fillRect(holdBarX + 1, holdBarY + 1, resetHoldProgressPx, holdBarH - 2, SSD1306_WHITE);
    }
  } else {
    display.setTextColor(SSD1306_WHITE, SSD1306_BLACK);
    display.setCursor(0, 56);
    if (lastPressedSwitch >= 0) {
      display.print("LAST SW");
      display.print(lastPressedSwitch + 1);
      display.print(" ");
      display.print(lastPressedIsOn ? "ON" : "OFF");
    } else {
      display.print("LAST: NONE");
    }

    drawField(84, 56, "BRI:", pixelBrightness, selectedField == FIELD_BRIGHTNESS, editing);
  }

  display.display();
}

void adjustSelectedField(int delta) {
  if (selectedField == FIELD_BANK) {
    int newBank = currentBank + delta;
    if (newBank < 1) newBank = NUM_BANKS;
    if (newBank > NUM_BANKS) newBank = 1;
    changeBank(newBank);
  } else if (selectedField == FIELD_SWITCH) {
    selectedSwitch += delta;
    if (selectedSwitch < 0) selectedSwitch = numSwitches - 1;
    if (selectedSwitch >= numSwitches) selectedSwitch = 0;
    redrawDisplay();
  } else if (selectedField == FIELD_SWITCH_CHANNEL) {
    SwitchConfig &sw = activeSwitch(selectedSwitch);
    int channel = (int)sw.channel + delta;
    if (channel < 1) channel = 16;
    if (channel > 16) channel = 1;
    sw.channel = (uint8_t)channel;
    markConfigDirty();
    redrawDisplay();
  } else if (selectedField == FIELD_BEHAVIOR) {
    SwitchConfig &sw = activeSwitch(selectedSwitch);
    sw.behavior = (sw.behavior == BEHAVIOR_MOMENTARY) ? BEHAVIOR_TOGGLE : BEHAVIOR_MOMENTARY;
    markConfigDirty();
    redrawDisplay();
  } else if (selectedField == FIELD_TYPE) {
    SwitchConfig &sw = activeSwitch(selectedSwitch);
    int type = (int)sw.midiType + delta;
    if (type < (int)MIDI_NOTE) type = (int)MIDI_PC;
    if (type > (int)MIDI_PC) type = (int)MIDI_NOTE;
    sw.midiType = (MidiActionType)type;
    markConfigDirty();
    redrawDisplay();
  } else if (selectedField == FIELD_NUMBER) {
    SwitchConfig &sw = activeSwitch(selectedSwitch);
    int number = (int)sw.number + delta;
    if (number < 0) number = 127;
    if (number > 127) number = 0;
    sw.number = (uint8_t)number;
    markConfigDirty();
    redrawDisplay();
  } else if (selectedField == FIELD_BRIGHTNESS) {
    pixelBrightness += delta;
    if (pixelBrightness < 0) pixelBrightness = 10;
    if (pixelBrightness > 10) pixelBrightness = 0;
    applyPixelBrightness();
    markConfigDirty();
    redrawDisplay();
  } else if (selectedField == FIELD_CHANNEL) {
    globalMidiChannel += delta;
    if (globalMidiChannel < 1)  globalMidiChannel = 16;
    if (globalMidiChannel > 16) globalMidiChannel = 1;
    markConfigDirty();
    redrawDisplay();
  }
}

void changeBank(int newBank) {
  if (newBank == currentBank) return;
  currentBank = newBank;
  updateStatusPixels();
  redrawDisplay();
}

void updateStatusPixels() {
  static const RgbColor bankColors[NUM_BANKS] = {
    {0, 50, 0},   // 1: Green
    {50, 50, 0},  // 2: Yellow
    {0, 0, 50},   // 3: Blue
    {50, 0, 50},  // 4: Magenta
    {0, 50, 50},  // 5: Cyan
    {50, 20, 0},  // 6: Orange
    {40, 40, 40}, // 7: White
    {50, 0, 0}    // 8: Red
  };

  RgbColor c = bankColors[currentBank - 1];
  uint32_t bankColor = pixels.Color(c.r, c.g, c.b);

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
      sendNoteOnBoth(sw.number, sw.onValue, globalMidiChannel);
      break;

    case MIDI_CC:
      sendCcBoth(sw.number, sw.onValue, globalMidiChannel);
      break;

    case MIDI_PC:
      sendProgramChangeBoth(sw.number, globalMidiChannel);
      break;
  }
}

void sendMidiOff(const SwitchConfig &sw) {
  switch (sw.midiType) {
    case MIDI_NOTE:
      sendNoteOffBoth(sw.number, sw.offValue, globalMidiChannel);
      break;

    case MIDI_CC:
      sendCcBoth(sw.number, sw.offValue, globalMidiChannel);
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
