#pragma once

#include <Arduino.h>
#include "app_state.h"

// Waveshare documents BAT_ADC on GPIO17 for ESP32-S3-Touch-AMOLED-2.41.
// Confirm this against the exact board revision/schematic before reusing this
// firmware on a different Waveshare AMOLED variant.
#ifndef BATTERY_ADC_PIN
#define BATTERY_ADC_PIN 17
#endif

// Schematic BAT_ADC divider appears to use 200K over 100K, so the ADC node is
// approximately one third of VBAT. This is still approximate and ADC calibration
// varies between ESP32-S3 chips.
#ifndef BATTERY_VOLTAGE_DIVIDER
#define BATTERY_VOLTAGE_DIVIDER 3.0f
#endif

#ifndef BATTERY_DEBUG_LOG
#define BATTERY_DEBUG_LOG 0
#endif

void beginBatteryStatus(AppState &state);
void updateBatteryStatus(AppState &state);
