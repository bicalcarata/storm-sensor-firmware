#pragma once

#include "app_state.h"

void initAppState(AppState &state);
void updateDerivedState(AppState &state);
void seedEnvironmentHistory(AppState &state);
void appendEnvironmentHistory(AppState &state);
void appendTrendHistory(AppState &state);
const char *modeName(DeviceMode mode);
const char *connectivityName(Connectivity connectivity);
const char *as3935ProfileName(As3935Profile profile);
const char *rangeTrendName(const AppState &state);
