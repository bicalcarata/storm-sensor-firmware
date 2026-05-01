#pragma once

#include <Arduino.h>

// RGB565 colour palette tuned for a dark AMOLED dashboard.
static const uint16_t UI_BLACK = 0x0000;
static const uint16_t UI_BG = 0x0020;
static const uint16_t UI_CARD = 0x0861;
static const uint16_t UI_CARD_ALT = 0x10A2;
static const uint16_t UI_LINE = 0x2945;
static const uint16_t UI_WHITE = 0xFFFF;
static const uint16_t UI_MUTED = 0x9CF3;
static const uint16_t UI_CYAN = 0x05D6;
static const uint16_t UI_BLUE_DEEP = 0x0010;
static const uint16_t UI_GREEN = 0x05E0;
static const uint16_t UI_YELLOW = 0xCE60;
static const uint16_t UI_ORANGE = 0xD3A0;
static const uint16_t UI_RED = 0xC800;
static const uint16_t UI_RED_DEEP = 0x6800;
static const uint16_t UI_MAGENTA = 0xF81F;

struct SemanticRgb {
  uint8_t r;
  uint8_t g;
  uint8_t b;
};

static const SemanticRgb DEEP_BLUE_RGB = {0, 0, 80};
static const SemanticRgb GREEN_RGB = {0, 180, 0};
static const SemanticRgb YELLOW_RGB = {180, 150, 0};
static const SemanticRgb ORANGE_RGB = {220, 80, 0};
static const SemanticRgb RED_RGB = {220, 0, 0};
static const SemanticRgb DEEP_RED_RGB = {120, 0, 0};

inline uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b) {
  return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

inline uint16_t rgb565(SemanticRgb colour) {
  return rgb565(colour.r, colour.g, colour.b);
}

inline uint16_t blend565(uint16_t fg, uint16_t bg, uint8_t amount) {
  uint8_t fr = ((fg >> 11) & 0x1F) << 3;
  uint8_t fgG = ((fg >> 5) & 0x3F) << 2;
  uint8_t fb = (fg & 0x1F) << 3;
  uint8_t br = ((bg >> 11) & 0x1F) << 3;
  uint8_t bgG = ((bg >> 5) & 0x3F) << 2;
  uint8_t bb = (bg & 0x1F) << 3;

  uint8_t r = (fr * amount + br * (255 - amount)) / 255;
  uint8_t g = (fgG * amount + bgG * (255 - amount)) / 255;
  uint8_t b = (fb * amount + bb * (255 - amount)) / 255;
  return rgb565(r, g, b);
}

inline SemanticRgb semanticRgbForRate(float ratePerMin) {
  if (ratePerMin <= 0.1f) return DEEP_BLUE_RGB;
  if (ratePerMin < 2.0f) return GREEN_RGB;
  if (ratePerMin < 4.0f) return YELLOW_RGB;
  if (ratePerMin < 5.0f) return ORANGE_RGB;
  if (ratePerMin < 7.0f) return RED_RGB;
  return DEEP_RED_RGB;
}

inline SemanticRgb semanticRgbForDistance(float distanceKm, bool hasValidDistance) {
  if (!hasValidDistance || distanceKm <= 0.0f || distanceKm > 40.0f) return DEEP_BLUE_RGB;
  if (distanceKm >= 34.0f) return GREEN_RGB;
  if (distanceKm >= 24.0f) return YELLOW_RGB;
  if (distanceKm >= 14.0f) return ORANGE_RGB;
  if (distanceKm >= 6.0f) return RED_RGB;
  return DEEP_RED_RGB;
}

inline uint16_t severityColourForRate(float ratePerMin) {
  return rgb565(semanticRgbForRate(ratePerMin));
}

inline uint16_t severityColourForDistance(float distanceKm, bool hasValidDistance = true) {
  return rgb565(semanticRgbForDistance(distanceKm, hasValidDistance));
}
