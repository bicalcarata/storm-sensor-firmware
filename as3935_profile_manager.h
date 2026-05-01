#pragma once

#include <Arduino.h>
#include "app_state.h"

typedef bool (*As3935ReadFn)(uint8_t reg, uint8_t &value);
typedef bool (*As3935WriteFn)(uint8_t reg, uint8_t value);

class As3935ProfileManager {
public:
  void begin(AppState &state, As3935ReadFn readFn, As3935WriteFn writeFn);
  bool applyForMode(AppState &state, DeviceMode mode);
  As3935Profile profile() const;

private:
  As3935ReadFn readReg = nullptr;
  As3935WriteFn writeReg = nullptr;
  As3935Profile activeProfile = AS3935_PROFILE_ROAMING;
  bool hasApplied = false;
};
