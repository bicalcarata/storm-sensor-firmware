#pragma once

#include <Arduino.h>
#include <Preferences.h>
#include "app_state.h"

class ModeManager {
public:
  void begin(AppState &state);
  DeviceMode mode() const;
  void setMode(AppState &state, DeviceMode mode, bool persist);
  void toggle(AppState &state);

private:
  Preferences prefs;
  DeviceMode currentMode = MODE_ROAMING;
};
