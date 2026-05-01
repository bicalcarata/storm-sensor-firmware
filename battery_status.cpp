#include "battery_status.h"

#include <math.h>

static const uint32_t BATTERY_SAMPLE_INTERVAL_MS = 7000;
static const float BATTERY_EMA_ALPHA = 0.22f;
static const float BATTERY_MIN_VALID_V = 2.0f;
static const float BATTERY_HIGH_WARN_V = 4.6f;
static const float BATTERY_MAX_VALID_V = 5.5f;

struct BatteryCurvePoint {
  float voltage;
  uint8_t percent;
};

static const BatteryCurvePoint BATTERY_CURVE[] = {
  {4.20f, 100},
  {4.10f, 90},
  {4.00f, 80},
  {3.90f, 65},
  {3.80f, 50},
  {3.70f, 35},
  {3.60f, 20},
  {3.50f, 10},
  {3.30f, 5},
  {3.20f, 0}
};

static uint32_t lastBatterySampleMs = 0;
static float smoothedBatteryVoltage = NAN;
static bool haveBatterySample = false;

static void setBatteryStatusText(AppState &state, const char *text) {
  strncpy(state.batteryStatusText, text, sizeof(state.batteryStatusText));
  state.batteryStatusText[sizeof(state.batteryStatusText) - 1] = '\0';
}

static void setBatteryUnknown(AppState &state, const char *reason) {
  state.batteryPresent = false;
  state.batteryVoltage = NAN;
  state.batteryPercent = 0;
  state.batteryLow = false;
  state.batteryCritical = false;
  state.batteryCharging = false;
  setBatteryStatusText(state, reason);
}

static uint8_t batteryPercentForVoltage(float voltage) {
  if (voltage >= BATTERY_CURVE[0].voltage) return BATTERY_CURVE[0].percent;

  const uint8_t count = sizeof(BATTERY_CURVE) / sizeof(BATTERY_CURVE[0]);
  for (uint8_t i = 1; i < count; i++) {
    const BatteryCurvePoint &upper = BATTERY_CURVE[i - 1];
    const BatteryCurvePoint &lower = BATTERY_CURVE[i];
    if (voltage >= lower.voltage) {
      float span = upper.voltage - lower.voltage;
      if (span <= 0.0f) return lower.percent;
      float t = (voltage - lower.voltage) / span;
      return constrain((int)roundf(lower.percent + t * (upper.percent - lower.percent)), 0, 100);
    }
  }

  return 0;
}

static void applyBatteryReading(AppState &state, float voltage) {
  if (!haveBatterySample || !isfinite(smoothedBatteryVoltage)) {
    smoothedBatteryVoltage = voltage;
    haveBatterySample = true;
  } else {
    smoothedBatteryVoltage = smoothedBatteryVoltage * (1.0f - BATTERY_EMA_ALPHA) + voltage * BATTERY_EMA_ALPHA;
  }

  state.batteryPresent = true;
  state.batteryVoltage = smoothedBatteryVoltage;
  state.batteryPercent = batteryPercentForVoltage(smoothedBatteryVoltage);
  state.batteryLow = state.batteryPercent <= 20;
  state.batteryCritical = state.batteryPercent <= 10;
  state.batteryCharging = false;

  if (smoothedBatteryVoltage > BATTERY_HIGH_WARN_V) {
    setBatteryStatusText(state, "HIGH");
  } else if (state.batteryCritical) {
    setBatteryStatusText(state, "CRITICAL");
  } else if (state.batteryLow) {
    setBatteryStatusText(state, "LOW");
  } else {
    setBatteryStatusText(state, "OK");
  }
}

void beginBatteryStatus(AppState &state) {
  lastBatterySampleMs = millis() - BATTERY_SAMPLE_INTERVAL_MS;
  smoothedBatteryVoltage = NAN;
  haveBatterySample = false;
  setBatteryUnknown(state, "UNKNOWN");

  analogReadResolution(12);
  analogSetPinAttenuation(BATTERY_ADC_PIN, ADC_11db);
  pinMode(BATTERY_ADC_PIN, INPUT);

#if BATTERY_DEBUG_LOG
  Serial.printf("Battery ADC enabled on GPIO%d, divider %.2f\n", BATTERY_ADC_PIN, BATTERY_VOLTAGE_DIVIDER);
#endif
}

void updateBatteryStatus(AppState &state) {
  uint32_t now = millis();
  if (now - lastBatterySampleMs < BATTERY_SAMPLE_INTERVAL_MS) return;
  lastBatterySampleMs = now;

  uint32_t millivolts = analogReadMilliVolts(BATTERY_ADC_PIN);
  if (millivolts == 0) {
    setBatteryUnknown(state, "NO ADC");
    return;
  }

  float voltage = ((float)millivolts / 1000.0f) * BATTERY_VOLTAGE_DIVIDER;
  if (!isfinite(voltage) || voltage < BATTERY_MIN_VALID_V || voltage > BATTERY_MAX_VALID_V) {
    setBatteryUnknown(state, "UNKNOWN");
    return;
  }

  applyBatteryReading(state, voltage);

#if BATTERY_DEBUG_LOG
  Serial.printf("Battery ADC GPIO%d: %lumV -> %.2fV smooth %.2fV %u%% %s\n",
                BATTERY_ADC_PIN,
                millivolts,
                voltage,
                state.batteryVoltage,
                state.batteryPercent,
                state.batteryStatusText);
#endif
}
