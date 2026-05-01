#include "ui.h"

#include <math.h>
#include "colours.h"
#include "app_helpers.h"

static Arduino_GFX *gfx = nullptr;
static uint32_t lastRenderMs = 0;
static AppPage lastRenderedPage = PAGE_COUNT;
static uint32_t pageChangeMs = 0;
static int contentOffsetX = 0;

static const int M = 24;
static const int G = 12;
static const int HEADER_H = 58;
static const int FOOTER_H = 28;
static const int CONTENT_Y = 76;
static const uint8_t TEXT_LABEL = 1;
static const uint8_t TEXT_VALUE = 2;
static const uint8_t TEXT_HERO = 3;

static uint32_t lastTrendShiftMs = 0;

static int screenW() { return gfx ? gfx->width() : 600; }
static int screenH() { return gfx ? gfx->height() : 450; }
static int sx(int x) { return x + contentOffsetX; }

static uint16_t dimAccent(uint16_t colour) {
  return blend565(colour, UI_CARD, 95);
}

static uint16_t softAccent(uint16_t colour) {
  return blend565(colour, UI_BG, 185);
}

static uint8_t severityRankForRate(float ratePerMin) {
  if (ratePerMin <= 0.1f) return 0;
  if (ratePerMin < 2.0f) return 1;
  if (ratePerMin < 4.0f) return 2;
  if (ratePerMin < 5.0f) return 3;
  if (ratePerMin < 7.0f) return 4;
  return 5;
}

static uint8_t severityRankForDistance(float distanceKm) {
  if (distanceKm <= 0.0f || distanceKm > 40.0f) return 0;
  if (distanceKm >= 34.0f) return 1;
  if (distanceKm >= 24.0f) return 2;
  if (distanceKm >= 14.0f) return 3;
  if (distanceKm >= 6.0f) return 4;
  return 5;
}

static void textAt(int x, int y, const char *text, uint16_t colour, uint8_t size) {
  gfx->setTextColor(colour);
  gfx->setTextSize(size);
  gfx->setCursor(sx(x), y);
  gfx->print(text);
}

static void rawTextAt(int x, int y, const char *text, uint16_t colour, uint8_t size) {
  gfx->setTextColor(colour);
  gfx->setTextSize(size);
  gfx->setCursor(x, y);
  gfx->print(text);
}

static void drawBluetoothIconRaw(int x, int y, int s, uint16_t colour) {
  int cx = x + s / 2;
  int top = y;
  int mid = y + s / 2;
  int bottom = y + s;
  int right = x + s - 2;
  gfx->drawLine(cx, top, cx, bottom, colour);
  gfx->drawLine(cx, top, right, y + s / 4, colour);
  gfx->drawLine(right, y + s / 4, cx, mid, colour);
  gfx->drawLine(cx, mid, right, y + (s * 3) / 4, colour);
  gfx->drawLine(right, y + (s * 3) / 4, cx, bottom, colour);
  gfx->drawLine(cx, mid, x + 3, y + s / 4, colour);
  gfx->drawLine(cx, mid, x + 3, y + (s * 3) / 4, colour);
}

static void drawDoorIconRaw(int x, int y, int w, int h, bool open, uint16_t colour) {
  gfx->drawRect(x, y, w, h, dimAccent(colour));
  if (open) {
    gfx->drawLine(x + w - 3, y + 4, x + 10, y + 12, colour);
    gfx->drawLine(x + 10, y + 12, x + 10, y + h - 8, colour);
    gfx->drawLine(x + 10, y + h - 8, x + w - 3, y + h - 4, colour);
    gfx->fillCircle(x + 15, y + h / 2, 2, colour);
  } else {
    gfx->drawRect(x + 7, y + 4, w - 14, h - 8, colour);
    gfx->fillCircle(x + w - 13, y + h / 2, 2, colour);
  }
}

static void drawWifiArcRaw(int cx, int cy, int r, uint16_t colour) {
  int lastX = 0;
  int lastY = 0;
  bool hasLast = false;
  for (int deg = 215; deg <= 325; deg += 6) {
    float radians = deg * 0.0174533f;
    int x = cx + (int)(cosf(radians) * r);
    int y = cy + (int)(sinf(radians) * r);
    if (hasLast) {
      gfx->drawLine(lastX, lastY, x, y, colour);
    }
    lastX = x;
    lastY = y;
    hasLast = true;
  }
}

static void drawWifiIconRaw(int x, int y, int s, uint16_t colour) {
  int cx = x + s / 2;
  int cy = y + s - 4;
  drawWifiArcRaw(cx, cy, s / 2 - 2, colour);
  drawWifiArcRaw(cx, cy, s / 3, colour);
  drawWifiArcRaw(cx, cy, s / 5, colour);
  gfx->fillCircle(cx, cy - 1, 2, colour);
}

static void drawBatteryIconRaw(int x, int y, int w, int h, uint8_t percent, uint16_t colour) {
  gfx->drawRoundRect(x, y, w, h, 3, dimAccent(colour));
  gfx->fillRect(x + w, y + h / 3, 3, h / 3, dimAccent(colour));
  int fillW = constrain((int)((w - 6) * percent / 100), 0, w - 6);
  gfx->fillRect(x + 3, y + 3, fillW, h - 6, colour);
}

static uint16_t batteryColour(const AppState &state) {
  if (!state.batteryPresent) return UI_MUTED;
  if (state.batteryCritical) return UI_RED;
  if (state.batteryLow) return UI_YELLOW;
  return UI_GREEN;
}

static void drawSpeakerIconRaw(int x, int y, int s, uint16_t colour) {
  gfx->fillRect(x, y + s / 3, s / 4, s / 3, colour);
  gfx->fillTriangle(x + s / 4, y + s / 3, x + s / 2, y + s / 6, x + s / 2, y + (s * 5) / 6, colour);
  gfx->drawCircle(x + s / 2, y + s / 2, s / 3, colour);
  gfx->drawCircle(x + s / 2, y + s / 2, s / 2, dimAccent(colour));
  gfx->fillRect(x + s / 2 - 2, y, s / 2 + 4, s, UI_CARD);
}

static void drawLightningBolt(int x, int y, int s, uint16_t colour) {
  x = sx(x);
  int x0 = x + s / 2;
  gfx->fillTriangle(x0, y, x0 - s / 3, y + s / 2, x0 + s / 10, y + s / 2, colour);
  gfx->fillTriangle(x0 + s / 10, y + s / 2, x0 - s / 6, y + s, x0 + s / 3, y + s / 3, colour);
}

static void drawRangeTarget(int x, int y, int r, uint16_t colour) {
  x = sx(x);
  gfx->drawCircle(x, y, r, colour);
  gfx->drawCircle(x, y, r / 2, colour);
  gfx->fillCircle(x, y, 3, colour);
}

static void drawSeverityBar(int x, int y, int w, int h, uint16_t active) {
  const uint16_t cols[6] = {UI_BLUE_DEEP, UI_GREEN, UI_YELLOW, UI_ORANGE, UI_RED, UI_RED_DEEP};
  int segW = w / 6;
  for (uint8_t i = 0; i < 6; i++) {
    uint16_t c = cols[i] == active ? softAccent(cols[i]) : blend565(cols[i], UI_BG, 65);
    gfx->fillRect(sx(x + i * segW), y, segW - 4, h, c);
  }
}

static void drawLabelValue(int x, int y, const char *label, const char *value, uint16_t valueColour, uint8_t valueSize = TEXT_VALUE) {
  textAt(x, y, label, UI_MUTED, TEXT_LABEL);
  textAt(x, y + 24, value, valueColour, valueSize);
}

static float currentTrendValue(const AppState &state, TrendTab tab) {
  switch (tab) {
    case TAB_RATE: return state.ratePerMin;
    case TAB_TEMP: return state.bme280Present ? state.tempC : NAN;
    case TAB_HUM: return state.bme280Present ? state.humidity : NAN;
    case TAB_PRESS: return state.bme280Present ? state.pressure : NAN;
    default: return 0.0f;
  }
}

static const char *trendUnit(TrendTab tab) {
  switch (tab) {
    case TAB_RATE: return "/m";
    case TAB_TEMP: return "C";
    case TAB_HUM: return "%";
    case TAB_PRESS: return "hPa";
    default: return "";
  }
}

void setupUi(Arduino_GFX *display) {
  gfx = display;
  lastRenderMs = 0;
  lastRenderedPage = PAGE_COUNT;
  pageChangeMs = millis();
}

void drawCard(int x, int y, int w, int h, const char *title, const char *value, uint16_t accent, bool primary) {
  uint16_t fill = primary ? UI_CARD_ALT : UI_CARD;
  uint16_t border = primary ? softAccent(accent) : dimAccent(accent);
  int px = sx(x);
  gfx->fillRoundRect(px, y, w, h, 8, fill);
  gfx->drawRoundRect(px, y, w, h, 8, border);
  if (primary) {
    gfx->fillRect(px, y + 8, 4, h - 16, softAccent(accent));
  }
  rawTextAt(px + 16, y + 14, title, UI_MUTED, TEXT_LABEL);
  rawTextAt(px + 16, y + 38, value, UI_WHITE, TEXT_VALUE);
}

static void drawCompactCard(int x, int y, int w, int h, const char *title, const char *value, uint16_t accent) {
  int px = sx(x);
  gfx->fillRoundRect(px, y, w, h, 8, UI_CARD);
  gfx->drawRoundRect(px, y, w, h, 8, dimAccent(accent));
  rawTextAt(px + 16, y + 14, title, UI_MUTED, TEXT_LABEL);
  rawTextAt(px + 16, y + 36, value, UI_WHITE, TEXT_VALUE);
}

static void drawModeCard(int x, int y, int w, int h, const AppState &state) {
  int px = sx(x);
  uint16_t accent = UI_CYAN;
  gfx->fillRoundRect(px, y, w, h, 8, UI_CARD);
  gfx->drawRoundRect(px, y, w, h, 8, dimAccent(accent));
  rawTextAt(px + 16, y + 14, "MODE", UI_MUTED, TEXT_LABEL);
  rawTextAt(px + 16, y + 42, modeName(state.mode), UI_WHITE, TEXT_VALUE);
}

static void drawBtCard(int x, int y, int w, int h, const char *title, bool primary) {
  int px = sx(x);
  uint16_t accent = UI_GREEN;
  gfx->fillRoundRect(px, y, w, h, 8, primary ? UI_CARD_ALT : UI_CARD);
  gfx->drawRoundRect(px, y, w, h, 8, primary ? softAccent(accent) : dimAccent(accent));
  rawTextAt(px + 16, y + 14, title, UI_MUTED, TEXT_LABEL);
  drawBluetoothIconRaw(px + 54, y + 34, 32, softAccent(accent));
  rawTextAt(px + 96, y + 42, "BT", UI_WHITE, TEXT_VALUE);
}

static void drawBatteryCard(int x, int y, int w, int h, const AppState &state, bool primary) {
  char buf[18];
  int px = sx(x);
  uint16_t accent = batteryColour(state);
  gfx->fillRoundRect(px, y, w, h, 8, primary ? UI_CARD_ALT : UI_CARD);
  gfx->drawRoundRect(px, y, w, h, 8, primary ? softAccent(accent) : dimAccent(accent));
  rawTextAt(px + 16, y + 14, "BATTERY", UI_MUTED, TEXT_LABEL);
  drawBatteryIconRaw(px + 18, y + 43, 48, 22, state.batteryPresent ? state.batteryPercent : 0, softAccent(accent));
  if (state.batteryPresent) {
    snprintf(buf, sizeof(buf), "%u%%", state.batteryPercent);
  } else {
    snprintf(buf, sizeof(buf), "--%%");
  }
  rawTextAt(px + 82, y + 42, buf, UI_WHITE, TEXT_VALUE);
}

static void drawSystemBatteryCard(int x, int y, int w, int h, const AppState &state) {
  char buf[32];
  int px = sx(x);
  uint16_t accent = batteryColour(state);
  gfx->fillRoundRect(px, y, w, h, 8, UI_CARD_ALT);
  gfx->drawRoundRect(px, y, w, h, 8, softAccent(accent));
  rawTextAt(px + 16, y + 14, "BATTERY", UI_MUTED, TEXT_LABEL);
  drawBatteryIconRaw(px + 18, y + 43, 54, 22, state.batteryPresent ? state.batteryPercent : 0, softAccent(accent));
  if (state.batteryPresent) {
    snprintf(buf, sizeof(buf), "%u%%  %.2f V", state.batteryPercent, state.batteryVoltage);
  } else {
    snprintf(buf, sizeof(buf), "--%%  --.-- V");
  }
  rawTextAt(px + 92, y + 42, buf, UI_WHITE, TEXT_VALUE);
  rawTextAt(px + w - 118, y + 18, state.batteryStatusText, softAccent(accent), TEXT_LABEL);
  rawTextAt(px + 92, y + 62, state.batteryCharging ? "CHARGING" : "APPROX VBAT", UI_MUTED, TEXT_LABEL);
}

static void drawDeviceCard(int x, int y, int w, int h, const AppState &state) {
  char buf[28];
  int px = sx(x);
  uint16_t accent = UI_MAGENTA;
  gfx->fillRoundRect(px, y, w, h, 8, UI_CARD);
  gfx->drawRoundRect(px, y, w, h, 8, dimAccent(accent));
  rawTextAt(px + 16, y + 14, "CPU FREQ", UI_MUTED, TEXT_LABEL);
  snprintf(buf, sizeof(buf), "%luMHz", state.cpuFreqMHz);
  rawTextAt(px + 16, y + 38, buf, UI_WHITE, TEXT_VALUE);
  snprintf(buf, sizeof(buf), "RAM %u%%", state.heapUsedPercent);
  rawTextAt(px + 16, y + 58, buf, softAccent(accent), TEXT_LABEL);
}

static void drawWifiStatusCard(int x, int y, int w, int h, const AppState &state, bool primary) {
  char buf[34];
  int px = sx(x);
  uint16_t accent = UI_CYAN;
  gfx->fillRoundRect(px, y, w, h, 8, primary ? UI_CARD_ALT : UI_CARD);
  gfx->drawRoundRect(px, y, w, h, 8, primary ? softAccent(accent) : dimAccent(accent));
  rawTextAt(px + 16, y + 14, "HOME NETWORK", UI_MUTED, TEXT_LABEL);
  drawWifiIconRaw(px + 20, y + 36, 34, softAccent(accent));
  snprintf(buf, sizeof(buf), "%s", state.ipAddress);
  rawTextAt(px + 72, y + 42, buf, UI_WHITE, TEXT_VALUE);
}

void drawHeader(const AppState &state, AppPage page) {
  char buf[12];
  uint16_t battColour = batteryColour(state);
  gfx->fillRect(0, 0, screenW(), HEADER_H, UI_BLACK);
  gfx->drawFastHLine(0, HEADER_H - 1, screenW(), blend565(UI_LINE, UI_BLACK, 140));
  rawTextAt(16, 15, PAGE_NAMES[page], UI_WHITE, 3);
  rawTextAt(screenW() - 260, 19, modeName(state.mode), UI_CYAN, TEXT_VALUE);
  if (state.connectivity == CONN_WIFI) {
    drawWifiIconRaw(screenW() - 174, 14, 30, UI_CYAN);
  } else {
    drawBluetoothIconRaw(screenW() - 166, 15, 28, UI_GREEN);
  }
  drawBatteryIconRaw(screenW() - 104, 17, 34, 16, state.batteryPresent ? state.batteryPercent : 0, battColour);
  if (state.batteryPresent) {
    snprintf(buf, sizeof(buf), "%u%%", state.batteryPercent);
  } else {
    snprintf(buf, sizeof(buf), "--");
  }
  rawTextAt(screenW() - 68, 39, buf, battColour, TEXT_LABEL);
  if (state.batteryCritical) {
    rawTextAt(screenW() - 105, 6, "!", UI_RED, TEXT_LABEL);
  }
}

void drawFlashSymbol(int x, int y, uint16_t colour, bool lightning, uint32_t eventMs) {
  uint32_t now = millis();
  const uint16_t durationMs = lightning ? 260 : 440;

  uint8_t amount = 90;
  uint32_t age = eventMs == 0 ? durationMs : now - eventMs;
  if (eventMs != 0 && age < durationMs) {
    amount = map(age, 0, durationMs - 1, 230, 90);
  }

  uint16_t c = blend565(colour, UI_BG, amount);
  if (lightning) {
    drawLightningBolt(x, y, 34, c);
  } else {
    int pulse = age < durationMs ? map(age, 0, durationMs - 1, 5, 0) : 0;
    drawRangeTarget(x + 18, y + 18, 17 + pulse, c);
  }
}

void drawLivePage(const AppState &state) {
  char buf[40];
  uint16_t rateColour = severityColourForRate(state.ratePerMin);
  uint16_t rangeColour = severityColourForDistance(state.distanceKm, state.lightningDataValid);
  uint16_t worstColour = severityRankForRate(state.ratePerMin) >= severityRankForDistance(state.lightningDataValid ? state.distanceKm : 0.0f) ? rateColour : rangeColour;

  if (state.lightningDataValid) {
    snprintf(buf, sizeof(buf), "%.1f km", state.distanceKm);
  } else {
    snprintf(buf, sizeof(buf), "-- km");
  }
  int heroX = M, heroY = CONTENT_Y, heroW = 328, heroH = 166;
  gfx->fillRoundRect(sx(heroX), heroY, heroW, heroH, 8, UI_CARD_ALT);
  gfx->drawRoundRect(sx(heroX), heroY, heroW, heroH, 8, softAccent(rangeColour));
  drawLabelValue(heroX + 18, heroY + 18, "DISTANCE", buf, UI_WHITE, TEXT_HERO);
  drawLabelValue(heroX + 18, heroY + 114, "TREND", state.lightningDataValid ? rangeTrendName(state) : "WAITING", softAccent(rangeColour), TEXT_VALUE);
  drawFlashSymbol(heroX + 272, heroY + 114, rangeColour, false, state.lastLightningEventMs);

  snprintf(buf, sizeof(buf), "%u/m", state.ratePerMin);
  drawCard(364, CONTENT_Y, 212, 77, "RATE", buf, rateColour, true);
  drawFlashSymbol(522, CONTENT_Y + 18, rateColour, true, state.lastLightningEventMs);

  snprintf(buf, sizeof(buf), "%u session", state.sessionCount);
  drawCard(364, CONTENT_Y + 89, 212, 77, "COUNT", buf, UI_CYAN, false);

  gfx->fillRoundRect(sx(M), 266, 552, 96, 8, UI_CARD);
  gfx->drawRoundRect(sx(M), 266, 552, 96, 8, dimAccent(UI_MAGENTA));
  textAt(M + 16, 280, "THUNDER", UI_MUTED, TEXT_LABEL);
  drawSpeakerIconRaw(sx(M + 18), 308, 26, softAccent(UI_MAGENTA));
  if (state.thunderConfirmed) {
    textAt(M + 56, 306, "THUNDER", UI_WHITE, TEXT_VALUE);
    textAt(M + 56, 330, "DETECTED", UI_WHITE, TEXT_VALUE);
    snprintf(buf, sizeof(buf), "%.1f km", state.audioDistanceKm);
    drawLabelValue(M + 340, 280, "AUDIO DIST", buf, softAccent(severityColourForDistance(state.audioDistanceKm, true)), TEXT_VALUE);
    snprintf(buf, sizeof(buf), "%.1fs", state.thunderDelaySec);
    textAt(M + 212, 306, buf, UI_MAGENTA, TEXT_VALUE);
  } else if (state.thunderListening) {
    uint16_t remaining = (state.thunderWindowUntilMs - millis()) / 1000UL;
    textAt(M + 56, 306, "LISTENING", UI_WHITE, TEXT_VALUE);
    snprintf(buf, sizeof(buf), "%us window", remaining);
    textAt(M + 212, 306, buf, UI_MAGENTA, TEXT_VALUE);
    snprintf(buf, sizeof(buf), "mic +%.0f", state.micDelta);
    textAt(M + 340, 314, buf, UI_MUTED, TEXT_LABEL);
  } else {
    textAt(M + 56, 306, "NOT DETECTED", UI_MUTED, TEXT_VALUE);
  }

  textAt(M, 382, "SEVERITY", UI_MUTED, TEXT_LABEL);
  drawSeverityBar(118, 381, 458, 16, worstColour);
}

void drawRadarPage(const AppState &state) {
  int cx = screenW() / 2 + contentOffsetX;
  int cy = 238;
  int maxR = 148;
  uint16_t ring = blend565(UI_CYAN, UI_BG, 105);
  for (uint8_t i = 1; i <= 4; i++) {
    int r = (maxR * i) / 4;
    gfx->drawCircle(cx, cy, r - 1, ring);
    gfx->drawCircle(cx, cy, r, ring);
    gfx->drawCircle(cx, cy, r + 1, ring);
    if (i == 4) {
      uint16_t outer = blend565(UI_CYAN, UI_BG, 70);
      gfx->drawCircle(cx, cy, r - 2, outer);
      gfx->drawCircle(cx, cy, r + 2, outer);
    }
    char ringLabel[8];
    snprintf(ringLabel, sizeof(ringLabel), "%ukm", i * 10);
    rawTextAt(cx + 8, cy - r - 5, ringLabel, blend565(UI_WHITE, UI_BG, 125), TEXT_LABEL);
  }
  gfx->fillCircle(cx, cy, 5, blend565(UI_WHITE, UI_BG, 150));
  rawTextAt(cx - 9, cy + 12, "YOU", UI_MUTED, TEXT_LABEL);

  for (uint8_t i = 0; i < state.strikeCount; i++) {
    const StrikeMarker &s = state.strikes[i];
    float radians = s.angleDeg * 0.0174533f;
    int r = constrain((int)((s.distanceKm / 40.0f) * maxR), 16, maxR);
    int x = cx + (int)(cosf(radians) * r);
    int y = cy + (int)(sinf(radians) * r);
    uint8_t fade = constrain(230 - (int)(s.ageSeconds / 5), 45, 220);
    uint16_t c = blend565(s.colour, UI_BG, fade);
    int oldOffset = contentOffsetX;
    contentOffsetX = 0;
    drawLightningBolt(x - 9, y - 12, 24, c);
    contentOffsetX = oldOffset;
    char label[8];
    snprintf(label, sizeof(label), "%um", s.minutesAgo);
    rawTextAt(x + 10, y + 6, label, c, TEXT_LABEL);
  }

  char buf[38];
  if (state.lightningDataValid) {
    snprintf(buf, sizeof(buf), "%.1f km", state.distanceKm);
  } else {
    snprintf(buf, sizeof(buf), "-- km");
  }
  uint16_t rateColour = severityColourForRate(state.ratePerMin);
  uint16_t rangeColour = severityColourForDistance(state.distanceKm, state.lightningDataValid);
  drawCard(M, CONTENT_Y, 180, 76, "RANGE", buf, rangeColour, true);
  drawFlashSymbol(162, CONTENT_Y + 22, rangeColour, false, state.lastLightningEventMs);
  snprintf(buf, sizeof(buf), "%u/m", state.ratePerMin);
  drawCard(396, CONTENT_Y, 180, 76, "RATE", buf, rateColour, false);
  drawFlashSymbol(530, CONTENT_Y + 22, rateColour, true, state.lastLightningEventMs);
  if (state.thunderConfirmed) {
    snprintf(buf, sizeof(buf), "%.1f km", state.audioDistanceKm);
  } else {
    snprintf(buf, sizeof(buf), "-- km");
  }
  drawCard(M, 318, 180, 76, "LAST CLAP", buf, UI_MAGENTA, false);
  textAt(214, 398, "Range only - not directional", UI_MUTED, TEXT_LABEL);
}

void drawStormPage(const AppState &state) {
  char buf[32];
  if (state.lightningDataValid) {
    snprintf(buf, sizeof(buf), "%.1f km", state.nearestKm);
  } else {
    snprintf(buf, sizeof(buf), "-- km");
  }
  drawCard(M, CONTENT_Y, 270, 84, "NEAREST STRIKE", buf, severityColourForDistance(state.nearestKm, state.lightningDataValid), true);
  if (state.lightningDataValid) {
    snprintf(buf, sizeof(buf), "%.1f km", state.lastKm);
  } else {
    snprintf(buf, sizeof(buf), "-- km");
  }
  drawCard(M + 282, CONTENT_Y, 270, 84, "LAST STRIKE", buf, severityColourForDistance(state.lastKm, state.lightningDataValid), false);
  drawCard(M, CONTENT_Y + 96, 270, 84, "TREND", state.lightningDataValid ? rangeTrendName(state) : "WAITING", UI_ORANGE, true);
  snprintf(buf, sizeof(buf), "%u/m", state.peakRate);
  drawCard(M + 282, CONTENT_Y + 96, 270, 84, "PEAK RATE", buf, severityColourForRate(state.peakRate), false);
  drawCard(M, CONTENT_Y + 192, 552, 84, "STORM STATE", state.lightningDataValid ? "ACTIVE" : "WAITING", severityColourForRate(state.ratePerMin), false);
}

void drawAtmosPage(const AppState &state) {
  char buf[32];
  if (state.bme280Present) {
    snprintf(buf, sizeof(buf), "%.1f hPa", state.pressure);
  } else {
    snprintf(buf, sizeof(buf), "-- hPa");
  }
  drawCard(M, CONTENT_Y, 552, 84, "PRESSURE", buf, UI_CYAN, true);
  if (state.bme280Present) {
    snprintf(buf, sizeof(buf), "%.1f C", state.tempC);
  } else {
    snprintf(buf, sizeof(buf), "-- C");
  }
  drawCard(M, CONTENT_Y + 96, 270, 96, "TEMPERATURE", buf, UI_ORANGE, true);
  if (state.bme280Present) {
    snprintf(buf, sizeof(buf), "%u%%", state.humidity);
  } else {
    snprintf(buf, sizeof(buf), "--%%");
  }
  drawCard(M + 282, CONTENT_Y + 96, 270, 96, "HUMIDITY", buf, UI_GREEN, false);
  textAt(M + 6, 326, state.bme280Present ? "BME280 LIVE I2C - v1.0 confidence" : "BME280 not detected - no environment data", UI_MUTED, TEXT_LABEL);
}

void drawTrendsPage(const AppState &state, TrendTab tab) {
  int tabW = 138;
  for (uint8_t i = 0; i < TAB_COUNT; i++) {
    int x = M + i * (tabW + 8);
    uint16_t c = i == tab ? UI_CYAN : dimAccent(UI_LINE);
    gfx->fillRoundRect(sx(x), 70, tabW, 46, 8, i == tab ? UI_CARD_ALT : UI_CARD);
    gfx->drawRoundRect(sx(x), 70, tabW, 46, 8, c);
    textAt(x + 34, 85, TAB_NAMES[i], i == tab ? UI_WHITE : UI_MUTED, TEXT_VALUE);
  }

  char valueText[28];
  float selectedValue = currentTrendValue(state, tab);
  if (!isfinite(selectedValue)) {
    snprintf(valueText, sizeof(valueText), "--%s", trendUnit(tab));
  } else if (tab == TAB_RATE || tab == TAB_HUM) {
    snprintf(valueText, sizeof(valueText), "%.0f%s", currentTrendValue(state, tab), trendUnit(tab));
  } else if (tab == TAB_PRESS) {
    snprintf(valueText, sizeof(valueText), "%.0f%s", currentTrendValue(state, tab), trendUnit(tab));
  } else {
    snprintf(valueText, sizeof(valueText), "%.1f%s", currentTrendValue(state, tab), trendUnit(tab));
  }

  uint16_t lineColour = tab == TAB_RATE ? UI_RED : (tab == TAB_PRESS ? UI_CYAN : (tab == TAB_TEMP ? UI_ORANGE : UI_GREEN));
  lineColour = softAccent(lineColour);
  drawLabelValue(M + 18, 124, TAB_NAMES[tab], valueText, lineColour, TEXT_VALUE);

  int gx = M + 18, gy = 166, gw = 516, gh = 212;
  uint16_t grid = blend565(UI_LINE, UI_BG, 42);
  gfx->drawRect(sx(gx), gy, gw, gh, grid);
  for (uint8_t i = 1; i < 4; i++) {
    gfx->drawFastHLine(sx(gx + 1), gy + (gh * i) / 4, gw - 2, grid);
  }

  bool hasGraphData = false;
  float minV = 0.0f;
  float maxV = 0.0f;
  for (uint16_t i = 0; i < GRAPH_POINTS; i++) {
    if (!isfinite(state.history[tab][i])) continue;
    if (!hasGraphData) {
      minV = state.history[tab][i];
      maxV = state.history[tab][i];
      hasGraphData = true;
    } else {
      minV = min(minV, state.history[tab][i]);
      maxV = max(maxV, state.history[tab][i]);
    }
  }
  if (tab == TAB_RATE) {
    minV = 0.0f;
    maxV = max(1.0f, ceilf(maxV + 1.0f));
  }
  if (!hasGraphData) {
    textAt(gx + 190, gy + 96, "NO DATA", UI_MUTED, TEXT_VALUE);
    return;
  }
  if (tab == TAB_TEMP) {
    minV = floorf(minV - 1.0f);
    maxV = ceilf(maxV + 1.0f);
  } else if (tab == TAB_HUM) {
    minV = max(0.0f, floorf(minV - 5.0f));
    maxV = min(100.0f, ceilf(maxV + 5.0f));
  } else if (tab == TAB_PRESS) {
    minV = floorf(minV - 2.0f);
    maxV = ceilf(maxV + 2.0f);
  }
  if (maxV - minV < 0.5f) maxV = minV + 0.5f;

  uint16_t glow = blend565(lineColour, UI_BG, 80);
  int xShift = 0;

  float lastX = gx + 8 - xShift;
  float lastY = gy + gh - 8 - ((state.history[tab][0] - minV) / (maxV - minV)) * (gh - 16);
  bool hasLastPoint = isfinite(state.history[tab][0]);
  for (uint16_t i = 1; i < GRAPH_POINTS; i++) {
    if (!isfinite(state.history[tab][i])) continue;
    float targetX = gx + 8 + ((float)(gw - 16) * i) / (GRAPH_POINTS - 1) - xShift;
    float targetY = gy + gh - 8 - ((state.history[tab][i] - minV) / (maxV - minV)) * (gh - 16);
    if (!hasLastPoint) {
      lastX = targetX;
      lastY = targetY;
      hasLastPoint = true;
      continue;
    }
    for (uint8_t step = 1; step <= 3; step++) {
      float t = step / 3.0f;
      float x = lastX + (targetX - lastX) * t;
      float y = lastY + (targetY - lastY) * t;
      int px0 = sx((int)lastX);
      int py0 = (int)lastY;
      int px1 = sx((int)x);
      int py1 = (int)y;
      gfx->drawLine(px0, py0 - 1, px1, py1 - 1, glow);
      gfx->drawLine(px0, py0 + 1, px1, py1 + 1, glow);
      gfx->drawLine(px0, py0, px1, py1, lineColour);
      lastX = x;
      lastY = y;
    }
  }

  char scaleText[22];
  if (tab == TAB_TEMP) {
    snprintf(scaleText, sizeof(scaleText), "%.1fC", maxV);
    textAt(gx + gw - 58, gy + 8, scaleText, UI_MUTED, TEXT_LABEL);
    snprintf(scaleText, sizeof(scaleText), "%.1fC", minV);
    textAt(gx + gw - 58, gy + gh - 18, scaleText, UI_MUTED, TEXT_LABEL);
  } else if (tab == TAB_HUM) {
    snprintf(scaleText, sizeof(scaleText), "%.0f%%", maxV);
    textAt(gx + gw - 44, gy + 8, scaleText, UI_MUTED, TEXT_LABEL);
    snprintf(scaleText, sizeof(scaleText), "%.0f%%", minV);
    textAt(gx + gw - 44, gy + gh - 18, scaleText, UI_MUTED, TEXT_LABEL);
  } else if (tab == TAB_PRESS) {
    snprintf(scaleText, sizeof(scaleText), "%.0fhPa", maxV);
    textAt(gx + gw - 72, gy + 8, scaleText, UI_MUTED, TEXT_LABEL);
    snprintf(scaleText, sizeof(scaleText), "%.0fhPa", minV);
    textAt(gx + gw - 72, gy + gh - 18, scaleText, UI_MUTED, TEXT_LABEL);
  }
}

void drawSystemPage(const AppState &state) {
  char buf[44];
  drawSystemBatteryCard(M, CONTENT_Y, 384, 76, state);
  drawDeviceCard(M + 396, CONTENT_Y, 156, 76, state);
  uint32_t uptimeSeconds = millis() / 1000UL;
  uint32_t hours = uptimeSeconds / 3600UL;
  uint8_t minutes = (uptimeSeconds / 60UL) % 60UL;
  uint8_t seconds = uptimeSeconds % 60UL;
  snprintf(buf, sizeof(buf), "%02lu:%02u:%02u", hours, minutes, seconds);
  drawCard(M, CONTENT_Y + 88, 270, 82, "UPTIME", buf, UI_CYAN, true);
  drawModeCard(M + 282, CONTENT_Y + 88, 270, 82, state);
  char sensorText[64];
  snprintf(sensorText, sizeof(sensorText), "%s / %s %s / MIC +%.0f",
           state.bme280Present ? "BME280 OK" : "BME280 NOT FOUND",
           state.lightningPresent ? "AS3935 OK" : "AS3935 NOT FOUND",
           state.as3935ProfileText,
           state.micDelta);
  drawCard(M, CONTENT_Y + 182, 552, 82, "SENSORS", sensorText, (state.bme280Present && state.lightningPresent) ? UI_GREEN : UI_ORANGE, false);
  snprintf(buf, sizeof(buf), "%s %u SAT", state.gpsActive ? "ACTIVE" : "INACTIVE", state.gpsSatellites);
  drawCompactCard(M, CONTENT_Y + 274, 270, 52, "GPS", buf, state.gpsActive ? UI_GREEN : UI_ORANGE);
  drawCompactCard(M + 282, CONTENT_Y + 274, 270, 52, "GPS TIME", state.gpsTime, state.gpsFixValid ? UI_GREEN : UI_CYAN);
}

static void drawLevelDots(int x, int y, uint8_t level, uint16_t accent) {
  for (uint8_t i = 1; i <= 10; i++) {
    uint16_t colour = i <= level ? softAccent(accent) : blend565(UI_LINE, UI_BG, 80);
    gfx->fillRoundRect(sx(x + (i - 1) * 39), y, 28, 10, 4, colour);
  }
}

static void drawMinusIconRaw(int x, int y, uint16_t colour) {
  gfx->drawFastHLine(x, y, 22, colour);
  gfx->drawFastHLine(x, y + 1, 22, colour);
}

static void drawPlusIconRaw(int x, int y, uint16_t colour) {
  drawMinusIconRaw(x, y, colour);
  gfx->drawFastVLine(x + 10, y - 10, 22, colour);
  gfx->drawFastVLine(x + 11, y - 10, 22, colour);
}

static void drawLevelControl(int x, int y, int w, const char *title, uint8_t level, uint16_t accent, bool enabled) {
  char buf[16];
  uint16_t fill = enabled ? UI_CARD_ALT : UI_CARD;
  uint16_t border = enabled ? softAccent(accent) : dimAccent(UI_MUTED);
  int px = sx(x);
  gfx->fillRoundRect(px, y, w, 104, 8, fill);
  gfx->drawRoundRect(px, y, w, 104, 8, border);
  rawTextAt(px + 18, y + 12, title, UI_MUTED, TEXT_LABEL);
  snprintf(buf, sizeof(buf), "%u/10", level);
  rawTextAt(px + 18, y + 34, buf, UI_WHITE, TEXT_HERO);

  uint16_t buttonBorder = enabled ? dimAccent(accent) : dimAccent(UI_MUTED);
  gfx->fillRoundRect(px + 338, y + 18, 54, 48, 8, UI_CARD);
  gfx->drawRoundRect(px + 338, y + 18, 54, 48, 8, buttonBorder);
  drawMinusIconRaw(px + 354, y + 42, enabled ? UI_WHITE : UI_MUTED);
  gfx->fillRoundRect(px + 414, y + 18, 54, 48, 8, enabled ? UI_CARD_ALT : UI_CARD);
  gfx->drawRoundRect(px + 414, y + 18, 54, 48, 8, buttonBorder);
  drawPlusIconRaw(px + 430, y + 42, enabled ? UI_WHITE : UI_MUTED);
  drawLevelDots(x + 18, y + 84, level, enabled ? accent : UI_MUTED);
}

void drawDisplayPage(const AppState &state) {
  drawLevelControl(M, CONTENT_Y, 552, "BRIGHTNESS", state.displayBrightnessLevel, UI_CYAN, true);
  drawLevelControl(M, CONTENT_Y + 114, 552, "CONTRAST", 5, UI_MAGENTA, false);
  drawLevelControl(M, CONTENT_Y + 228, 552, "GAMMA", 5, UI_YELLOW, false);
}

void drawNetworkPage(const AppState &state, bool resetConfirmVisible) {
  char buf[64];
  if (state.mode == MODE_HOME) {
    snprintf(buf, sizeof(buf), "%s  %s", state.wifiStatus, state.ipAddress);
    drawCard(M, CONTENT_Y, 552, 78, "MODE: HOME / WIFI", buf, state.wifiConnected ? UI_GREEN : UI_YELLOW, true);
    snprintf(buf, sizeof(buf), "%s", state.wifiSsid);
    drawCard(M, CONTENT_Y + 88, 270, 72, "SSID", buf, UI_CYAN, false);
    snprintf(buf, sizeof(buf), "HTTP %s", state.webStatus);
    drawCard(M + 282, CONTENT_Y + 88, 270, 72, "WEB SERVER", buf, state.webServerEnabled ? UI_GREEN : UI_MUTED, false);
    if (state.wifiProvisioning) {
      snprintf(buf, sizeof(buf), "%s %s", state.captivePortalSsid, state.captivePortalIp);
      drawCard(M, CONTENT_Y + 170, 552, 62, "WI-FI SETUP", buf, UI_YELLOW, false);
    } else {
      snprintf(buf, sizeof(buf), "AS3935 %s", state.as3935ProfileText);
      drawCard(M, CONTENT_Y + 170, 552, 62, "LIGHTNING PROFILE", buf, UI_CYAN, false);
    }
  } else {
    snprintf(buf, sizeof(buf), "%s  clients %u", state.bleStatus, state.bleClientCount);
    drawCard(M, CONTENT_Y, 552, 78, "MODE: ROAMING / BLE", buf, state.bleEnabled ? UI_GREEN : UI_YELLOW, true);
    drawCard(M, CONTENT_Y + 88, 270, 72, "WIFI", "OFF", UI_MUTED, false);
    drawCard(M + 282, CONTENT_Y + 88, 270, 72, "BLE NAME", state.bleDeviceName, UI_GREEN, false);
    snprintf(buf, sizeof(buf), "AS3935 %s", state.as3935ProfileText);
    drawCard(M, CONTENT_Y + 170, 552, 62, "LIGHTNING PROFILE", buf, UI_CYAN, false);
  }

  gfx->fillRoundRect(sx(24), 328, 252, 54, 8, UI_CARD);
  gfx->drawRoundRect(sx(24), 328, 252, 54, 8, dimAccent(UI_RED));
  textAt(64, 346, state.mode == MODE_HOME ? "RESET WIFI" : "RESET HOME WIFI", blend565(UI_RED, UI_WHITE, 120), TEXT_VALUE);

  gfx->fillRoundRect(sx(324), 328, 252, 54, 8, UI_CARD_ALT);
  gfx->drawRoundRect(sx(324), 328, 252, 54, 8, dimAccent(UI_CYAN));
  textAt(358, 346, state.mode == MODE_HOME ? "GO ROAMING" : "GO HOME", UI_WHITE, TEXT_VALUE);

  if (resetConfirmVisible) {
    gfx->fillRoundRect(72, 116, 456, 210, 8, rgb565(8, 12, 18));
    gfx->drawRoundRect(72, 116, 456, 210, 8, dimAccent(UI_YELLOW));
    rawTextAt(108, 148, "CONFIRM RESET WIFI?", UI_YELLOW, 3);
    rawTextAt(108, 198, "Clear saved HOME Wi-Fi credentials.", UI_WHITE, TEXT_VALUE);
    rawTextAt(108, 222, "Provisioning portal will start.", UI_MUTED, TEXT_LABEL);
    gfx->fillRoundRect(108, 252, 156, 50, 8, UI_CARD_ALT);
    gfx->drawRoundRect(108, 252, 156, 50, 8, UI_LINE);
    rawTextAt(150, 268, "CANCEL", UI_WHITE, TEXT_VALUE);
    gfx->fillRoundRect(336, 252, 156, 50, 8, UI_CARD);
    gfx->drawRoundRect(336, 252, 156, 50, 8, dimAccent(UI_RED));
    rawTextAt(366, 268, "CONFIRM", UI_WHITE, TEXT_VALUE);
  }
}

void renderUi(AppState &state, AppPage page, TrendTab tab, bool resetConfirmVisible, bool force) {
  if (!gfx) return;
  uint32_t now = millis();
  uint32_t renderIntervalMs = page == PAGE_TRENDS ? 10000UL : 80UL;
  if (!force && now - lastRenderMs < renderIntervalMs) return;

  if (page != lastRenderedPage) {
    pageChangeMs = millis();
    lastRenderedPage = page;
  }

  uint32_t animAge = millis() - pageChangeMs;
  contentOffsetX = animAge < 180 ? map(animAge, 0, 179, 16, 0) : 0;

  if (now - lastTrendShiftMs >= 10000UL) {
    lastTrendShiftMs = now;
  }

  gfx->fillScreen(UI_BG);
  drawHeader(state, page);

  switch (page) {
    case PAGE_LIVE: drawLivePage(state); break;
    case PAGE_RADAR: drawRadarPage(state); break;
    case PAGE_STORM: drawStormPage(state); break;
    case PAGE_TRENDS: drawTrendsPage(state, tab); break;
    case PAGE_ATMOS: drawAtmosPage(state); break;
    case PAGE_DISPLAY: drawDisplayPage(state); break;
    case PAGE_NETWORK: drawNetworkPage(state, resetConfirmVisible); break;
    case PAGE_SYSTEM: drawSystemPage(state); break;
    default: break;
  }

  uint8_t edgeFade = animAge < 180 ? map(animAge, 0, 179, 120, 0) : 0;
  if (edgeFade > 0) {
    gfx->fillRect(0, HEADER_H, 4, screenH() - HEADER_H - FOOTER_H, blend565(UI_CYAN, UI_BG, edgeFade));
  }

  gfx->drawFastHLine(0, screenH() - FOOTER_H, screenW(), blend565(UI_LINE, UI_BG, 95));
  rawTextAt(16, screenH() - 20, "Swipe left/right for pages", UI_MUTED, TEXT_LABEL);
  rawTextAt(screenW() - 238, screenH() - 20, "v1.0 confidence / live sensors", UI_MUTED, TEXT_LABEL);
  gfx->flush();
  lastRenderMs = millis();
}

bool pointInRect(uint16_t px, uint16_t py, int x, int y, int w, int h) {
  return px >= x && px < x + w && py >= y && py < y + h;
}
