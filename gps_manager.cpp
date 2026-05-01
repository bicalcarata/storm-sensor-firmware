#include "gps_manager.h"

#include <HardwareSerial.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#define GPS_RX_PIN 44
#define GPS_TX_PIN 43
#define GPS_BAUD 9600

static const uint32_t GPS_ACTIVE_TIMEOUT_MS = 5000UL;
static HardwareSerial gpsSerial(1);

void GpsManager::begin(AppState &state) {
  gpsSerial.begin(GPS_BAUD, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);
  state.gpsHardwareStarted = true;
  state.gpsActive = false;
  state.gpsFixValid = false;
  state.gpsSatellites = 0;
  state.gpsLastCharMs = 0;
  state.gpsLastFixMs = 0;
  Serial.printf("GPS UART ready: RX GPIO%d, TX GPIO%d, %u baud\n", GPS_RX_PIN, GPS_TX_PIN, GPS_BAUD);
}

bool GpsManager::checksumValid(const char *sentence) const {
  if (!sentence || sentence[0] != '$') return false;
  const char *star = strchr(sentence, '*');
  if (!star || star - sentence < 2 || strlen(star) < 3) return false;

  uint8_t checksum = 0;
  for (const char *p = sentence + 1; p < star; p++) {
    checksum ^= (uint8_t)*p;
  }
  uint8_t expected = (uint8_t)strtoul(star + 1, nullptr, 16);
  return checksum == expected;
}

double GpsManager::parseCoordinate(const char *value, const char *hemisphere) {
  if (!value || !*value || !hemisphere || !*hemisphere) return NAN;
  double raw = atof(value);
  int degrees = (int)(raw / 100.0);
  double minutes = raw - (degrees * 100.0);
  double decimal = degrees + minutes / 60.0;
  if (hemisphere[0] == 'S' || hemisphere[0] == 'W') decimal = -decimal;
  return decimal;
}

void GpsManager::parseGga(AppState &state, char *sentence) {
  char *fields[16] = {0};
  uint8_t count = 0;
  for (char *token = strtok(sentence, ","); token && count < 16; token = strtok(nullptr, ",")) {
    fields[count++] = token;
  }
  if (count < 10) return;

  int fixQuality = atoi(fields[6]);
  state.gpsSatellites = constrain(atoi(fields[7]), 0, 99);
  state.gpsFixValid = fixQuality > 0;
  state.gpsAltitudeM = fields[9] && *fields[9] ? atof(fields[9]) : NAN;
  if (state.gpsFixValid) {
    double lat = parseCoordinate(fields[2], fields[3]);
    double lon = parseCoordinate(fields[4], fields[5]);
    if (isfinite(lat) && isfinite(lon)) {
      state.gpsLatitude = lat;
      state.gpsLongitude = lon;
      state.gpsLastFixMs = millis();
    }
  }
}

void GpsManager::parseRmc(AppState &state, char *sentence) {
  char *fields[14] = {0};
  uint8_t count = 0;
  for (char *token = strtok(sentence, ","); token && count < 14; token = strtok(nullptr, ",")) {
    fields[count++] = token;
  }
  if (count < 10) return;

  if (fields[1] && strlen(fields[1]) >= 6) {
    snprintf(state.gpsTime, sizeof(state.gpsTime), "%c%c:%c%c:%c%c",
             fields[1][0], fields[1][1], fields[1][2], fields[1][3], fields[1][4], fields[1][5]);
  }
  if (fields[9] && strlen(fields[9]) == 6) {
    snprintf(state.gpsDate, sizeof(state.gpsDate), "%c%c/%c%c/20%c%c",
             fields[9][0], fields[9][1], fields[9][2], fields[9][3], fields[9][4], fields[9][5]);
  }
  state.gpsSpeedKph = fields[7] && *fields[7] ? atof(fields[7]) * 1.852f : NAN;

  if (fields[2] && fields[2][0] == 'A') {
    double lat = parseCoordinate(fields[3], fields[4]);
    double lon = parseCoordinate(fields[5], fields[6]);
    if (isfinite(lat) && isfinite(lon)) {
      state.gpsLatitude = lat;
      state.gpsLongitude = lon;
      state.gpsFixValid = true;
      state.gpsLastFixMs = millis();
    }
  }
}

void GpsManager::handleSentence(AppState &state, const char *sentence) {
  if (!checksumValid(sentence)) return;

  char copy[128];
  strncpy(copy, sentence + 1, sizeof(copy));
  copy[sizeof(copy) - 1] = '\0';
  char *star = strchr(copy, '*');
  if (star) *star = '\0';

  if (strncmp(copy, "GPGGA", 5) == 0 || strncmp(copy, "GNGGA", 5) == 0) {
    parseGga(state, copy);
  } else if (strncmp(copy, "GPRMC", 5) == 0 || strncmp(copy, "GNRMC", 5) == 0) {
    parseRmc(state, copy);
  }
}

void GpsManager::update(AppState &state) {
  while (gpsSerial.available() > 0) {
    char c = (char)gpsSerial.read();
    state.gpsLastCharMs = millis();

    if (c == '$') {
      sentenceIndex = 0;
      sentenceBuffer[sentenceIndex++] = c;
    } else if (c == '\n') {
      if (sentenceIndex > 0) {
        sentenceBuffer[sentenceIndex] = '\0';
        handleSentence(state, sentenceBuffer);
      }
      sentenceIndex = 0;
    } else if (c != '\r' && sentenceIndex < sizeof(sentenceBuffer) - 1) {
      sentenceBuffer[sentenceIndex++] = c;
    }
  }

  uint32_t now = millis();
  state.gpsActive = state.gpsLastCharMs > 0 && now - state.gpsLastCharMs <= GPS_ACTIVE_TIMEOUT_MS;
  if (!state.gpsActive) {
    state.gpsFixValid = false;
    state.gpsSatellites = 0;
  }
}
