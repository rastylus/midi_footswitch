#pragma once

#include <Arduino.h>
#include <Adafruit_GFX.h>

// Default to the current monochrome OLED path unless explicitly overridden.
#if !defined(DISPLAY_BACKEND_SSD1306) && !defined(DISPLAY_BACKEND_SSD1331) && !defined(DISPLAY_BACKEND_MIRROR)
#define DISPLAY_BACKEND_SSD1306 1
#endif

#if ((defined(DISPLAY_BACKEND_SSD1306) && DISPLAY_BACKEND_SSD1306) + \
	 (defined(DISPLAY_BACKEND_SSD1331) && DISPLAY_BACKEND_SSD1331) + \
	 (defined(DISPLAY_BACKEND_MIRROR) && DISPLAY_BACKEND_MIRROR)) > 1
#error "Select only one display backend: SSD1306, SSD1331, or MIRROR"
#endif

#if (defined(DISPLAY_BACKEND_SSD1306) && DISPLAY_BACKEND_SSD1306) || \
	(defined(DISPLAY_BACKEND_MIRROR) && DISPLAY_BACKEND_MIRROR)
#define DISPLAY_HAS_SSD1306 1
#include <Wire.h>
#include <Adafruit_SSD1306.h>
#endif

#if (defined(DISPLAY_BACKEND_SSD1331) && DISPLAY_BACKEND_SSD1331) || \
	(defined(DISPLAY_BACKEND_MIRROR) && DISPLAY_BACKEND_MIRROR)
#define DISPLAY_HAS_SSD1331 1
#include <SPI.h>
#include <Adafruit_SSD1331.h>
#endif

// SSD1331 wiring (hardware SPI mode).
#ifndef OLED_SPI_CS_PIN
#define OLED_SPI_CS_PIN 26
#endif
#ifndef OLED_SPI_DC_PIN
#define OLED_SPI_DC_PIN 20
#endif
#ifndef OLED_SPI_RST_PIN
#define OLED_SPI_RST_PIN 1
#endif

// SSD1331 brightness tuning (project-level override without editing libdeps).
// Valid ranges are 0x00-0x0F for master current and 0x00-0xFF per channel contrast.
#ifndef OLED_SSD1331_MASTER_CURRENT
#define OLED_SSD1331_MASTER_CURRENT 0x0A
#endif
#ifndef OLED_SSD1331_CONTRAST_A
#define OLED_SSD1331_CONTRAST_A 0xC0
#endif
#ifndef OLED_SSD1331_CONTRAST_B
#define OLED_SSD1331_CONTRAST_B 0x80
#endif
#ifndef OLED_SSD1331_CONTRAST_C
#define OLED_SSD1331_CONTRAST_C 0xC0
#endif

#ifndef OLED_I2C_ADDRESS
#define OLED_I2C_ADDRESS 0x3C
#endif

#if defined(DISPLAY_BACKEND_SSD1331) && DISPLAY_BACKEND_SSD1331
constexpr int16_t DISPLAY_SCREEN_WIDTH = Adafruit_SSD1331::TFTWIDTH;
#else
constexpr int16_t DISPLAY_SCREEN_WIDTH = 128;
#endif
constexpr int16_t DISPLAY_SCREEN_HEIGHT = 64;

// Keep existing monochrome constant names so the current rendering code compiles
// unchanged while we support both display families.
#ifndef SSD1306_BLACK
#define SSD1306_BLACK 0x0000
#endif
#ifndef SSD1306_WHITE
#define SSD1306_WHITE 0xFFFF
#endif

class DisplayProxy {
public:
	DisplayProxy();

	bool begin();
	void clear();
	void present();

	void setTextSize(uint8_t s);
	void setTextColor(uint16_t c);
	void setTextColor(uint16_t c, uint16_t bg);
	void setCursor(int16_t x, int16_t y);

	template <typename T>
	size_t print(const T &value) {
		size_t out = 0;
#ifdef DISPLAY_HAS_SSD1306
		out = monoDisplay.print(value);
#endif
#ifdef DISPLAY_HAS_SSD1331
		out = colorCanvas.print(value);
#endif
		return out;
	}

	template <typename T>
	size_t println(const T &value) {
		size_t out = 0;
#ifdef DISPLAY_HAS_SSD1306
		out = monoDisplay.println(value);
#endif
#ifdef DISPLAY_HAS_SSD1331
		out = colorCanvas.println(value);
#endif
		return out;
	}

	size_t println();

	void drawFastHLine(int16_t x, int16_t y, int16_t w, uint16_t color);
	void drawRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color);
	void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color);

private:
#ifdef DISPLAY_HAS_SSD1306
	Adafruit_SSD1306 monoDisplay;
#endif
#ifdef DISPLAY_HAS_SSD1331
	Adafruit_SSD1331 colorDisplay;
	GFXcanvas16 colorCanvas;
#endif
};

extern DisplayProxy display;

bool beginDisplay();
void clearDisplayFrame();
void presentDisplayFrame();
