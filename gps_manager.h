#pragma once

#include <Arduino.h>
#include "app_state.h"

class GpsManager {
public:
  void begin(AppState &state);
  void update(AppState &state);

private:
  void handleSentence(AppState &state, const char *sentence);
  void parseGga(AppState &state, char *sentence);
  void parseRmc(AppState &state, char *sentence);
  double parseCoordinate(const char *value, const char *hemisphere);
  bool checksumValid(const char *sentence) const;

  char sentenceBuffer[128] = {0};
  uint8_t sentenceIndex = 0;
};
