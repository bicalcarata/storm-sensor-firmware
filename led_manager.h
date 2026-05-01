#pragma once

#include <Arduino.h>
#include "colours.h"

void initLeds();
void setBootLedsDeepBlue();
void pulseBootLedsOnce();
void releaseBootLeds();
void updateLedMetrics(uint8_t ratePerMinute, float distanceKm, bool hasValidDistance);
void triggerLightningFlash();
void updateLeds();
SemanticRgb currentRateLedColour();
SemanticRgb currentRangeLedColour();
