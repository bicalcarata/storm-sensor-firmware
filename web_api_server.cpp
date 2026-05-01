#include "web_api_server.h"

#include <string.h>
#include "app_helpers.h"

static void appendJsonString(String &json, const char *key, const char *value, bool comma = true) {
  json += "\"";
  json += key;
  json += "\":\"";
  for (const char *p = value; *p; p++) {
    if (*p == '"' || *p == '\\') json += '\\';
    json += *p;
  }
  json += "\"";
  if (comma) json += ",";
}

void WebApiServer::begin(AppState &state) {
  configureRoutes();
  strncpy(state.webStatus, "OFF", sizeof(state.webStatus));
  state.webServerEnabled = false;
}

void WebApiServer::configureRoutes() {
  server.on("/api/status", HTTP_GET, [this]() {
    extern AppState appState;
    server.send(200, "application/json", jsonStatus(appState));
  });
  server.on("/api/lightning", HTTP_GET, [this]() {
    extern AppState appState;
    server.send(200, "application/json", jsonLightning(appState));
  });
  server.on("/api/environment", HTTP_GET, [this]() {
    extern AppState appState;
    server.send(200, "application/json", jsonEnvironment(appState));
  });
  server.on("/api/gps", HTTP_GET, [this]() {
    extern AppState appState;
    server.send(200, "application/json", jsonGps(appState));
  });
  server.on("/", HTTP_GET, [this]() {
    server.send(200, "application/json", "{\"endpoints\":[\"/api/status\",\"/api/lightning\",\"/api/environment\",\"/api/gps\"]}");
  });
}

String WebApiServer::jsonStatus(const AppState &state) const {
  String json = "{";
  appendJsonString(json, "network_mode", modeName(state.mode));
  appendJsonString(json, "as3935_profile", state.as3935ProfileText);
  appendJsonString(json, "wifi_status", state.wifiStatus);
  appendJsonString(json, "ssid", state.wifiSsid);
  appendJsonString(json, "ip", state.ipAddress);
  json += "\"battery_percent\":";
  json += state.batteryPresent ? String(state.batteryPercent) : "null";
  json += ",\"battery_voltage\":";
  json += state.batteryPresent ? String(state.batteryVoltage, 2) : "null";
  json += ",\"uptime_s\":";
  json += millis() / 1000UL;
  json += ",\"gps_active\":";
  json += state.gpsActive ? "true" : "false";
  json += ",\"gps_satellites\":";
  json += state.gpsSatellites;
  json += ",\"web_server\":\"";
  json += active ? "ON" : "OFF";
  json += "\"}";
  return json;
}

String WebApiServer::jsonLightning(const AppState &state) const {
  String json = "{";
  json += "\"rate_per_min\":";
  json += state.ratePerMin;
  json += ",\"distance_km\":";
  json += state.lightningDataValid ? String(state.distanceKm, 1) : "null";
  json += ",\"range_band\":\"";
  json += rangeTrendName(state);
  json += "\",\"strike_count\":";
  json += state.sessionCount;
  json += ",\"latest_strike_age_s\":";
  json += state.lastStrikeMs > 0 ? String((millis() - state.lastStrikeMs) / 1000UL) : "null";
  json += ",\"as3935_profile\":\"";
  json += state.as3935ProfileText;
  json += "\"}";
  return json;
}

String WebApiServer::jsonEnvironment(const AppState &state) const {
  String json = "{";
  json += "\"temperature_c\":";
  json += state.bme280Present ? String(state.tempC, 1) : "null";
  json += ",\"humidity_percent\":";
  json += state.bme280Present ? String(state.humidity) : "null";
  json += ",\"pressure_hpa\":";
  json += state.bme280Present ? String(state.pressure, 1) : "null";
  json += ",\"gps_date\":\"";
  json += state.gpsDate;
  json += "\",\"gps_time\":\"";
  json += state.gpsTime;
  json += "\"}";
  return json;
}

String WebApiServer::jsonGps(const AppState &state) const {
  String json = "{";
  json += "\"active\":";
  json += state.gpsActive ? "true" : "false";
  json += ",\"fix_valid\":";
  json += state.gpsFixValid ? "true" : "false";
  json += ",\"satellites\":";
  json += state.gpsSatellites;
  json += ",\"latitude\":";
  json += state.gpsFixValid ? String(state.gpsLatitude, 6) : "null";
  json += ",\"longitude\":";
  json += state.gpsFixValid ? String(state.gpsLongitude, 6) : "null";
  json += ",\"altitude_m\":";
  json += state.gpsFixValid ? String(state.gpsAltitudeM, 1) : "null";
  json += ",\"speed_kph\":";
  json += state.gpsFixValid ? String(state.gpsSpeedKph, 1) : "null";
  json += ",\"date\":\"";
  json += state.gpsDate;
  json += "\",\"time\":\"";
  json += state.gpsTime;
  json += "\"}";
  return json;
}

void WebApiServer::start(AppState &state) {
  if (active) return;
  server.begin();
  active = true;
  state.webServerEnabled = true;
  strncpy(state.webStatus, "ON", sizeof(state.webStatus));
  Serial.println("HTTP API server started");
}

void WebApiServer::stop(AppState &state) {
  if (!active) {
    state.webServerEnabled = false;
    strncpy(state.webStatus, "OFF", sizeof(state.webStatus));
    return;
  }
  server.stop();
  active = false;
  state.webServerEnabled = false;
  strncpy(state.webStatus, "OFF", sizeof(state.webStatus));
  Serial.println("HTTP API server stopped");
}

void WebApiServer::update(AppState &state) {
  if (!active) return;
  server.handleClient();
}

bool WebApiServer::isActive() const {
  return active;
}
