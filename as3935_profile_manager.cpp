#include "as3935_profile_manager.h"

#include <string.h>
#include "app_helpers.h"

static const uint8_t AS3935_REG_AFE_GAIN_LOCAL = 0x00;
static const uint8_t AS3935_AFE_MASK = 0x3E;
static const uint8_t AS3935_AFE_INDOOR = 0x24;
static const uint8_t AS3935_AFE_OUTDOOR = 0x1C;
static const uint8_t AS3935_POWER_DOWN_BIT = 0x01;

void As3935ProfileManager::begin(AppState &state, As3935ReadFn readFn, As3935WriteFn writeFn) {
  readReg = readFn;
  writeReg = writeFn;
  state.as3935Profile = activeProfile;
  strncpy(state.as3935ProfileText, as3935ProfileName(activeProfile), sizeof(state.as3935ProfileText));
  state.as3935ProfileText[sizeof(state.as3935ProfileText) - 1] = '\0';
}

As3935Profile As3935ProfileManager::profile() const {
  return activeProfile;
}

bool As3935ProfileManager::applyForMode(AppState &state, DeviceMode mode) {
  As3935Profile target = mode == MODE_HOME ? AS3935_PROFILE_HOME : AS3935_PROFILE_ROAMING;
  state.as3935Profile = target;
  strncpy(state.as3935ProfileText, as3935ProfileName(target), sizeof(state.as3935ProfileText));
  state.as3935ProfileText[sizeof(state.as3935ProfileText) - 1] = '\0';

  if (!state.lightningPresent || !readReg || !writeReg) {
    activeProfile = target;
    hasApplied = false;
    return false;
  }
  if (hasApplied && activeProfile == target) {
    return true;
  }

  uint8_t reg0 = 0;
  if (!readReg(AS3935_REG_AFE_GAIN_LOCAL, reg0)) {
    return false;
  }

  uint8_t afe = target == AS3935_PROFILE_HOME ? AS3935_AFE_INDOOR : AS3935_AFE_OUTDOOR;
  reg0 = (reg0 & ~AS3935_AFE_MASK) | afe;
  reg0 &= ~AS3935_POWER_DOWN_BIT;

  if (!writeReg(AS3935_REG_AFE_GAIN_LOCAL, reg0)) {
    return false;
  }

  activeProfile = target;
  hasApplied = true;
  Serial.printf("AS3935 profile applied: %s (AFE 0x%02X)\n", as3935ProfileName(target), afe);
  return true;
}
