#pragma once

#include <Arduino.h>
#include "colours.h"

enum AppPage {
  PAGE_LIVE = 0,
  PAGE_RADAR,
  PAGE_STORM,
  PAGE_TRENDS,
  PAGE_ATMOS,
  PAGE_DISPLAY,
  PAGE_NETWORK,
  PAGE_SYSTEM,
  PAGE_COUNT
};

enum TrendTab {
  TAB_RATE = 0,
  TAB_TEMP,
  TAB_HUM,
  TAB_PRESS,
  TAB_COUNT
};

enum DeviceMode {
  MODE_HOME = 0,
  MODE_ROAMING
};

enum Connectivity {
  CONN_BLE = 0,
  CONN_WIFI
};

enum As3935Profile {
  AS3935_PROFILE_HOME = 0,
  AS3935_PROFILE_ROAMING
};

enum StrikeConfidenceDecision {
  STRIKE_DECISION_SUPPRESS = 0,
  STRIKE_DECISION_PROMPT,
  STRIKE_DECISION_ACCEPT
};

struct StrikeMarker {
  float distanceKm;
  uint8_t minutesAgo;
  uint16_t ageSeconds;
  uint16_t colour;
  int16_t angleDeg;
  uint32_t eventMs;
};

const uint8_t MAX_STRIKE_MARKERS = 8;
const uint8_t MAX_RATE_EVENTS = 120;
const uint16_t GRAPH_POINTS = 360;

struct AppState {
  float distanceKm;
  float nearestKm;
  float lastKm;
  uint8_t ratePerMin;
  uint16_t sessionCount;
  uint8_t peakRate;
  bool lightningPresent;
  bool lightningDataValid;
  uint8_t lightningI2cAddr;
  uint8_t lightningLastInterrupt;
  uint8_t strikeConfidenceScore;
  StrikeConfidenceDecision strikeConfidenceDecision;
  int8_t strikeSessionBias;
  bool strikeTrainingModeEnabled;
  bool strikePromptPending;
  As3935Profile as3935Profile;
  char as3935ProfileText[12];
  uint32_t lastStrikeMs;
  uint32_t strikeEventMs[MAX_RATE_EVENTS];
  float pressure;
  float tempC;
  uint8_t humidity;
  bool bme280Present;
  uint32_t lastBmeReadMs;
  uint8_t batteryPercent;
  float batteryVoltage;
  bool batteryPresent;
  char batteryStatusText[12];
  bool batteryLow;
  bool batteryCritical;
  bool batteryCharging;
  uint16_t batteryCapacityMah;
  uint32_t freeHeap;
  uint32_t totalHeap;
  uint8_t heapUsedPercent;
  uint32_t freePsram;
  uint32_t totalPsram;
  uint8_t psramUsedPercent;
  uint32_t cpuFreqMHz;
  uint8_t displayBrightnessLevel;
  uint16_t micRaw;
  float micBaseline;
  float micDelta;
  bool thunderCandidate;
  bool thunderConfirmed;
  bool thunderListening;
  float thunderDelaySec;
  float audioDistanceKm;
  uint32_t lastLightningEventMs;
  uint32_t thunderWindowUntilMs;
  uint32_t thunderCandidateSinceMs;
  uint32_t thunderCooldownUntilMs;
  DeviceMode mode;
  Connectivity connectivity;
  bool wifiEnabled;
  bool wifiConnected;
  bool wifiProvisioning;
  bool webServerEnabled;
  bool bleEnabled;
  uint8_t bleClientCount;
  uint8_t wifiFailureCount;
  char wifiStatus[24];
  char wifiSsid[33];
  char ipAddress[16];
  char webStatus[16];
  char bleStatus[16];
  char bleDeviceName[32];
  char captivePortalSsid[33];
  char captivePortalIp[16];
  char captivePortalProgress[32];
  bool gpsHardwareStarted;
  bool gpsActive;
  bool gpsFixValid;
  uint8_t gpsSatellites;
  double gpsLatitude;
  double gpsLongitude;
  float gpsAltitudeM;
  float gpsSpeedKph;
  uint32_t gpsLastCharMs;
  uint32_t gpsLastFixMs;
  char gpsDate[11];
  char gpsTime[9];
  StrikeMarker strikes[MAX_STRIKE_MARKERS];
  uint8_t strikeCount;
  uint16_t trendSampleCount;
  float history[TAB_COUNT][GRAPH_POINTS];
};

extern const char *PAGE_NAMES[PAGE_COUNT];
extern const char *TAB_NAMES[TAB_COUNT];
