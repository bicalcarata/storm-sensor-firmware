#pragma once

#include <Arduino.h>
#include "app_state.h"

class BleLightningService {
public:
  void begin(AppState &state);
  void start(AppState &state);
  void stop(AppState &state);
  void update(AppState &state);
  bool isActive() const;
  void setClientCount(AppState &state, uint8_t count);

private:
  bool active = false;
  uint32_t lastNotifyMs = 0;
  uint8_t lastRate = 255;
  uint16_t lastStrikeCount = 0xFFFF;
  uint8_t lastBattery = 255;
  char lastProfile[12] = {0};
};
