#include "led_manager.h"

#include <Adafruit_NeoPixel.h>

static const uint8_t LED_DATA_PIN = 40;
static const uint8_t LED_COUNT = 2;
static const uint8_t LED_RATE_INDEX = 0;
static const uint8_t LED_RANGE_INDEX = 1;
static const uint8_t BASE_BRIGHTNESS = 45;
static const uint8_t FLASH_PEAK_BRIGHTNESS = 130;
static const uint16_t RATE_FLASH_DURATION_MS = 260;
static const uint16_t RANGE_FLASH_DURATION_MS = 440;

static Adafruit_NeoPixel pixels(LED_COUNT, LED_DATA_PIN, NEO_GRB + NEO_KHZ800);

static uint8_t currentRatePerMinute = 0;
static float currentDistanceKm = 0.0f;
static bool hasValidDistance = false;
static SemanticRgb rateColour = DEEP_BLUE_RGB;
static SemanticRgb rangeColour = DEEP_BLUE_RGB;
static uint32_t rateFlashStartMillis = 0;
static uint32_t rangeFlashStartMillis = 0;
static bool rateFlashActive = false;
static bool rangeFlashActive = false;
static bool ledsReady = false;
static bool bootLedMode = false;

static uint32_t scaledColour(SemanticRgb colour, uint8_t brightness) {
  uint8_t r = ((uint16_t)colour.r * brightness) / 255;
  uint8_t g = ((uint16_t)colour.g * brightness) / 255;
  uint8_t b = ((uint16_t)colour.b * brightness) / 255;
  return pixels.Color(r, g, b);
}

static uint8_t brightnessForPulse(uint32_t now, uint32_t startMs, uint16_t durationMs) {
  uint32_t elapsed = now - startMs;
  if (elapsed >= durationMs) return BASE_BRIGHTNESS;
  uint8_t extra = map(elapsed, 0, durationMs - 1, FLASH_PEAK_BRIGHTNESS - BASE_BRIGHTNESS, 0);
  return BASE_BRIGHTNESS + extra;
}

void initLeds() {
  pixels.begin();
  pixels.clear();
  pixels.show();
  ledsReady = true;
  updateLedMetrics(0, 0.0f, false);
  updateLeds();
}

void setBootLedsDeepBlue() {
  if (!ledsReady) return;
  bootLedMode = true;
  rateColour = DEEP_BLUE_RGB;
  rangeColour = DEEP_BLUE_RGB;
  rateFlashActive = false;
  rangeFlashActive = false;
  updateLeds();
}

void pulseBootLedsOnce() {
  if (!ledsReady) return;
  uint32_t now = millis();
  rateFlashStartMillis = now;
  rangeFlashStartMillis = now;
  rateFlashActive = true;
  rangeFlashActive = true;
  updateLeds();
}

void releaseBootLeds() {
  bootLedMode = false;
  rateFlashActive = false;
  rangeFlashActive = false;
  updateLeds();
}

void updateLedMetrics(uint8_t ratePerMinute, float distanceKm, bool validDistance) {
  currentRatePerMinute = ratePerMinute;
  currentDistanceKm = distanceKm;
  hasValidDistance = validDistance;
  if (bootLedMode) return;
  rateColour = semanticRgbForRate(currentRatePerMinute);
  rangeColour = semanticRgbForDistance(currentDistanceKm, hasValidDistance);
}

void triggerLightningFlash() {
  uint32_t now = millis();
  rateFlashStartMillis = now;
  rangeFlashStartMillis = now;
  rateFlashActive = true;
  rangeFlashActive = true;
}

void updateLeds() {
  if (!ledsReady) return;

  uint32_t now = millis();
  uint8_t rateBrightness = BASE_BRIGHTNESS;
  uint8_t rangeBrightness = BASE_BRIGHTNESS;

  if (rateFlashActive) {
    if (now - rateFlashStartMillis >= RATE_FLASH_DURATION_MS) {
      rateFlashActive = false;
    } else {
      rateBrightness = brightnessForPulse(now, rateFlashStartMillis, RATE_FLASH_DURATION_MS);
    }
  }

  if (rangeFlashActive) {
    if (now - rangeFlashStartMillis >= RANGE_FLASH_DURATION_MS) {
      rangeFlashActive = false;
    } else {
      rangeBrightness = brightnessForPulse(now, rangeFlashStartMillis, RANGE_FLASH_DURATION_MS);
    }
  }

  SemanticRgb displayRateColour = bootLedMode ? DEEP_BLUE_RGB : rateColour;
  SemanticRgb displayRangeColour = bootLedMode ? DEEP_BLUE_RGB : rangeColour;
  pixels.setPixelColor(LED_RATE_INDEX, scaledColour(displayRateColour, rateBrightness));
  pixels.setPixelColor(LED_RANGE_INDEX, scaledColour(displayRangeColour, rangeBrightness));
  pixels.show();
}

SemanticRgb currentRateLedColour() {
  return rateColour;
}

SemanticRgb currentRangeLedColour() {
  return rangeColour;
}
