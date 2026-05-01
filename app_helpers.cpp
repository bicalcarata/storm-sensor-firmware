#include "app_helpers.h"

#include <math.h>

const char *PAGE_NAMES[PAGE_COUNT] = {
  "LIVE", "RADAR", "STORM", "TRENDS", "ATMOS", "DISPLAY", "NETWORK", "SYSTEM"
};

const char *TAB_NAMES[TAB_COUNT] = {
  "RATE", "TEMP", "HUM", "PRESS"
};

static uint32_t lastStateUpdateMs = 0;
static uint32_t lastHistoryShiftMs = 0;

static uint8_t rollingStrikeRatePerMinute(const AppState &state, uint32_t now) {
  uint8_t recent = 0;
  for (uint8_t i = 0; i < MAX_RATE_EVENTS; i++) {
    if (state.strikeEventMs[i] != 0 && (uint32_t)(now - state.strikeEventMs[i]) <= 60000UL) {
      recent++;
    }
  }
  return recent;
}

void appendTrendHistory(AppState &state) {
  uint16_t writeIndex = state.trendSampleCount;
  if (writeIndex >= GRAPH_POINTS) {
    for (uint8_t tab = 0; tab < TAB_COUNT; tab++) {
      for (uint16_t i = 0; i < GRAPH_POINTS - 1; i++) {
        state.history[tab][i] = state.history[tab][i + 1];
      }
    }
    writeIndex = GRAPH_POINTS - 1;
  }

  state.history[TAB_RATE][writeIndex] = state.ratePerMin;
  state.history[TAB_TEMP][writeIndex] = state.bme280Present ? state.tempC : NAN;
  state.history[TAB_HUM][writeIndex] = state.bme280Present ? state.humidity : NAN;
  state.history[TAB_PRESS][writeIndex] = state.bme280Present ? state.pressure : NAN;

  if (state.trendSampleCount < GRAPH_POINTS) {
    state.trendSampleCount++;
  }
}

void initAppState(AppState &state) {
  randomSeed((uint32_t)micros());
  state.distanceKm = 0.0f;
  state.nearestKm = 0.0f;
  state.lastKm = 0.0f;
  state.ratePerMin = 0;
  state.sessionCount = 0;
  state.peakRate = 0;
  state.lightningPresent = false;
  state.lightningDataValid = false;
  state.lightningI2cAddr = 0;
  state.lightningLastInterrupt = 0;
  state.strikeConfidenceScore = 0;
  state.strikeConfidenceDecision = STRIKE_DECISION_SUPPRESS;
  state.strikeSessionBias = 0;
  state.strikeTrainingModeEnabled = true;
  state.strikePromptPending = false;
  state.as3935Profile = AS3935_PROFILE_ROAMING;
  strncpy(state.as3935ProfileText, "ROAMING", sizeof(state.as3935ProfileText));
  state.as3935ProfileText[sizeof(state.as3935ProfileText) - 1] = '\0';
  state.lastStrikeMs = 0;
  for (uint8_t i = 0; i < MAX_RATE_EVENTS; i++) {
    state.strikeEventMs[i] = 0;
  }
  state.pressure = NAN;
  state.tempC = NAN;
  state.humidity = 0;
  state.bme280Present = false;
  state.lastBmeReadMs = 0;
  state.batteryPercent = 82;
  state.batteryVoltage = 3.88f;
  state.batteryPresent = false;
  strncpy(state.batteryStatusText, "UNKNOWN", sizeof(state.batteryStatusText));
  state.batteryStatusText[sizeof(state.batteryStatusText) - 1] = '\0';
  state.batteryLow = false;
  state.batteryCritical = false;
  state.batteryCharging = false;
  state.batteryCapacityMah = 1800;
  state.freeHeap = 0;
  state.totalHeap = 0;
  state.heapUsedPercent = 0;
  state.freePsram = 0;
  state.totalPsram = 0;
  state.psramUsedPercent = 0;
  state.cpuFreqMHz = 0;
  state.displayBrightnessLevel = 8;
  state.micRaw = 2048;
  state.micBaseline = 2048.0f;
  state.micDelta = 0.0f;
  state.thunderCandidate = false;
  state.thunderConfirmed = false;
  state.thunderListening = false;
  state.thunderDelaySec = 0.0f;
  state.audioDistanceKm = 0.0f;
  state.lastLightningEventMs = 0;
  state.thunderWindowUntilMs = 0;
  state.thunderCandidateSinceMs = 0;
  state.thunderCooldownUntilMs = 0;
  state.mode = MODE_ROAMING;
  state.connectivity = CONN_BLE;
  state.wifiEnabled = false;
  state.wifiConnected = false;
  state.wifiProvisioning = false;
  state.webServerEnabled = false;
  state.bleEnabled = true;
  state.bleClientCount = 0;
  state.wifiFailureCount = 0;
  strncpy(state.wifiStatus, "OFF", sizeof(state.wifiStatus));
  state.wifiStatus[sizeof(state.wifiStatus) - 1] = '\0';
  strncpy(state.wifiSsid, "--", sizeof(state.wifiSsid));
  state.wifiSsid[sizeof(state.wifiSsid) - 1] = '\0';
  strncpy(state.ipAddress, "0.0.0.0", sizeof(state.ipAddress));
  state.ipAddress[sizeof(state.ipAddress) - 1] = '\0';
  strncpy(state.webStatus, "OFF", sizeof(state.webStatus));
  state.webStatus[sizeof(state.webStatus) - 1] = '\0';
  strncpy(state.bleStatus, "ADVERTISING", sizeof(state.bleStatus));
  state.bleStatus[sizeof(state.bleStatus) - 1] = '\0';
  strncpy(state.bleDeviceName, "Lightning Detector", sizeof(state.bleDeviceName));
  state.bleDeviceName[sizeof(state.bleDeviceName) - 1] = '\0';
  strncpy(state.captivePortalSsid, "--", sizeof(state.captivePortalSsid));
  state.captivePortalSsid[sizeof(state.captivePortalSsid) - 1] = '\0';
  strncpy(state.captivePortalIp, "0.0.0.0", sizeof(state.captivePortalIp));
  state.captivePortalIp[sizeof(state.captivePortalIp) - 1] = '\0';
  strncpy(state.captivePortalProgress, "idle", sizeof(state.captivePortalProgress));
  state.captivePortalProgress[sizeof(state.captivePortalProgress) - 1] = '\0';
  state.gpsHardwareStarted = false;
  state.gpsActive = false;
  state.gpsFixValid = false;
  state.gpsSatellites = 0;
  state.gpsLatitude = NAN;
  state.gpsLongitude = NAN;
  state.gpsAltitudeM = NAN;
  state.gpsSpeedKph = NAN;
  state.gpsLastCharMs = 0;
  state.gpsLastFixMs = 0;
  strncpy(state.gpsDate, "--/--/----", sizeof(state.gpsDate));
  state.gpsDate[sizeof(state.gpsDate) - 1] = '\0';
  strncpy(state.gpsTime, "--:--:--", sizeof(state.gpsTime));
  state.gpsTime[sizeof(state.gpsTime) - 1] = '\0';

  state.strikeCount = 0;
  for (uint8_t i = 0; i < MAX_STRIKE_MARKERS; i++) {
    state.strikes[i].distanceKm = 0.0f;
    state.strikes[i].minutesAgo = 0;
    state.strikes[i].ageSeconds = 0;
    state.strikes[i].colour = severityColourForDistance(0.0f, false);
    state.strikes[i].angleDeg = random(0, 360);
    state.strikes[i].eventMs = 0;
  }

  state.trendSampleCount = 0;
  for (uint16_t i = 0; i < GRAPH_POINTS; i++) {
    state.history[TAB_RATE][i] = NAN;
    state.history[TAB_TEMP][i] = NAN;
    state.history[TAB_HUM][i] = NAN;
    state.history[TAB_PRESS][i] = NAN;
  }

  lastStateUpdateMs = millis();
  lastHistoryShiftMs = millis();
}

void updateDerivedState(AppState &state) {
  uint32_t now = millis();

  state.thunderListening = now < state.thunderWindowUntilMs;
  state.ratePerMin = rollingStrikeRatePerMinute(state, now);
  state.peakRate = max(state.peakRate, state.ratePerMin);
  if (!state.thunderListening) {
    state.thunderCandidate = false;
    state.thunderCandidateSinceMs = 0;
  }

  if (now - lastStateUpdateMs >= 1000) {
    for (uint8_t i = 0; i < state.strikeCount; i++) {
      state.strikes[i].ageSeconds = (now - state.strikes[i].eventMs) / 1000UL;
      state.strikes[i].minutesAgo = state.strikes[i].ageSeconds / 60;
      state.strikes[i].colour = severityColourForDistance(state.strikes[i].distanceKm, true);
      state.strikes[i].angleDeg = (state.strikes[i].angleDeg + random(-18, 19) + 360) % 360;
    }
    lastStateUpdateMs = now;
  }

  if (now - lastHistoryShiftMs >= 10000UL) {
    appendTrendHistory(state);
    lastHistoryShiftMs = now;
  }
}

void seedEnvironmentHistory(AppState &state) {
  appendTrendHistory(state);
}

void appendEnvironmentHistory(AppState &state) {
  (void)state;
}

const char *modeName(DeviceMode mode) {
  return mode == MODE_HOME ? "HOME" : "ROAMING";
}

const char *connectivityName(Connectivity connectivity) {
  return connectivity == CONN_WIFI ? "WIFI" : "BT";
}

const char *as3935ProfileName(As3935Profile profile) {
  return profile == AS3935_PROFILE_HOME ? "HOME" : "ROAMING";
}

const char *rangeTrendName(const AppState &state) {
  if (!state.lightningDataValid) return "WAITING";
  if (fabsf(state.distanceKm - state.lastKm) < 0.5f) return "STABLE";
  return state.distanceKm < state.lastKm ? "INCOMING" : "OUTGOING";
}
