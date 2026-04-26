#include "display/DisplayBackend.h"

#ifdef DISPLAY_HAS_SSD1331
static void applySsd1331Brightness(Adafruit_SSD1331 &oled) {
  oled.sendCommand(SSD1331_CMD_MASTERCURRENT);
  oled.sendCommand((uint8_t)OLED_SSD1331_MASTER_CURRENT);

  oled.sendCommand(SSD1331_CMD_CONTRASTA);
  oled.sendCommand((uint8_t)OLED_SSD1331_CONTRAST_A);

  oled.sendCommand(SSD1331_CMD_CONTRASTB);
  oled.sendCommand((uint8_t)OLED_SSD1331_CONTRAST_B);

  oled.sendCommand(SSD1331_CMD_CONTRASTC);
  oled.sendCommand((uint8_t)OLED_SSD1331_CONTRAST_C);
}

static uint16_t normalizeColorForSsd1331(uint16_t color) {
  // In mirror mode, SSD1306 macros are 1-bit values (WHITE=1, BLACK=0).
  // Map those to full RGB565 so the color OLED does not render near-black.
  if (color == 0) {
    return 0x0000;
  }
  if (color == 1) {
    return 0xFFFF;
  }
  return color;
}
#endif

#ifdef DISPLAY_HAS_SSD1306
#define MONO_INIT monoDisplay(DISPLAY_SCREEN_WIDTH, DISPLAY_SCREEN_HEIGHT, &Wire, -1)
#else
#define MONO_INIT
#endif

#ifdef DISPLAY_HAS_SSD1306
static uint16_t normalizeColorForSsd1306(uint16_t color) {
  if (color == 0) {
    return SSD1306_BLACK;
  }
  if (color == SSD1306_INVERSE) {
    return SSD1306_INVERSE;
  }
  return SSD1306_WHITE;
}
#endif

#ifdef DISPLAY_HAS_SSD1331
#define COLOR_INIT colorDisplay(&SPI, OLED_SPI_CS_PIN, OLED_SPI_DC_PIN, OLED_SPI_RST_PIN)
#else
#define COLOR_INIT
#endif

DisplayProxy::DisplayProxy()
#if defined(DISPLAY_HAS_SSD1306) && defined(DISPLAY_HAS_SSD1331)
  : MONO_INIT, COLOR_INIT, colorCanvas(Adafruit_SSD1331::TFTWIDTH, Adafruit_SSD1331::TFTHEIGHT)
#elif defined(DISPLAY_HAS_SSD1306)
  : MONO_INIT
#elif defined(DISPLAY_HAS_SSD1331)
  : COLOR_INIT, colorCanvas(Adafruit_SSD1331::TFTWIDTH, Adafruit_SSD1331::TFTHEIGHT)
#endif
{}

bool DisplayProxy::begin() {
  bool ok = false;

#ifdef DISPLAY_HAS_SSD1306
  ok = monoDisplay.begin(SSD1306_SWITCHCAPVCC, OLED_I2C_ADDRESS) || ok;
#endif

#ifdef DISPLAY_HAS_SSD1331
  colorDisplay.begin();
  applySsd1331Brightness(colorDisplay);
  colorCanvas.setTextWrap(false);
  colorCanvas.fillScreen(SSD1306_BLACK);
  ok = true;
#endif

  return ok;
}

void DisplayProxy::clear() {
#ifdef DISPLAY_HAS_SSD1306
  monoDisplay.clearDisplay();
#endif
#ifdef DISPLAY_HAS_SSD1331
  colorCanvas.fillScreen(SSD1306_BLACK);
#endif
}

void DisplayProxy::present() {
#ifdef DISPLAY_HAS_SSD1306
  monoDisplay.display();
#endif
#ifdef DISPLAY_HAS_SSD1331
  colorDisplay.drawRGBBitmap(0, 0, colorCanvas.getBuffer(), Adafruit_SSD1331::TFTWIDTH, Adafruit_SSD1331::TFTHEIGHT);
#endif
}

void DisplayProxy::setTextSize(uint8_t s) {
#ifdef DISPLAY_HAS_SSD1306
  monoDisplay.setTextSize(s);
#endif
#ifdef DISPLAY_HAS_SSD1331
  colorCanvas.setTextSize(s);
#endif
}

void DisplayProxy::setTextColor(uint16_t c) {
#ifdef DISPLAY_HAS_SSD1306
  monoDisplay.setTextColor(normalizeColorForSsd1306(c));
#endif
#ifdef DISPLAY_HAS_SSD1331
  colorCanvas.setTextColor(normalizeColorForSsd1331(c));
#endif
}

void DisplayProxy::setTextColor(uint16_t c, uint16_t bg) {
#ifdef DISPLAY_HAS_SSD1306
  monoDisplay.setTextColor(normalizeColorForSsd1306(c), normalizeColorForSsd1306(bg));
#endif
#ifdef DISPLAY_HAS_SSD1331
  colorCanvas.setTextColor(normalizeColorForSsd1331(c), normalizeColorForSsd1331(bg));
#endif
}

void DisplayProxy::setCursor(int16_t x, int16_t y) {
#ifdef DISPLAY_HAS_SSD1306
  monoDisplay.setCursor(x, y);
#endif
#ifdef DISPLAY_HAS_SSD1331
  colorCanvas.setCursor(x, y);
#endif
}

size_t DisplayProxy::println() {
  size_t out = 0;
#ifdef DISPLAY_HAS_SSD1306
  out = monoDisplay.println();
#endif
#ifdef DISPLAY_HAS_SSD1331
  out = colorCanvas.println();
#endif
  return out;
}

void DisplayProxy::drawFastHLine(int16_t x, int16_t y, int16_t w, uint16_t color) {
#ifdef DISPLAY_HAS_SSD1306
  monoDisplay.drawFastHLine(x, y, w, normalizeColorForSsd1306(color));
#endif
#ifdef DISPLAY_HAS_SSD1331
  colorCanvas.drawFastHLine(x, y, w, normalizeColorForSsd1331(color));
#endif
}

void DisplayProxy::drawRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
#ifdef DISPLAY_HAS_SSD1306
  monoDisplay.drawRect(x, y, w, h, normalizeColorForSsd1306(color));
#endif
#ifdef DISPLAY_HAS_SSD1331
  colorCanvas.drawRect(x, y, w, h, normalizeColorForSsd1331(color));
#endif
}

void DisplayProxy::fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
#ifdef DISPLAY_HAS_SSD1306
  monoDisplay.fillRect(x, y, w, h, normalizeColorForSsd1306(color));
#endif
#ifdef DISPLAY_HAS_SSD1331
  colorCanvas.fillRect(x, y, w, h, normalizeColorForSsd1331(color));
#endif
}

DisplayProxy display;

bool beginDisplay() {
  return display.begin();
}

void clearDisplayFrame() {
  display.clear();
}

void presentDisplayFrame() {
  display.present();
}
