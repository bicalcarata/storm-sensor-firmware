#include "captive_portal_handler.h"

#include <WiFi.h>
#include <string.h>

static const byte DNS_PORT = 53;
static const char *PORTAL_AP_PREFIX = "Lightning-Setup-";

void CaptivePortalHandler::begin(AppState &state) {
  snprintf(state.captivePortalSsid, sizeof(state.captivePortalSsid), "%s%06X", PORTAL_AP_PREFIX, (uint32_t)(ESP.getEfuseMac() & 0xFFFFFF));
  strncpy(state.captivePortalIp, "0.0.0.0", sizeof(state.captivePortalIp));
  strncpy(state.captivePortalProgress, "idle", sizeof(state.captivePortalProgress));
  configureRoutes();
}

void CaptivePortalHandler::configureRoutes() {
  server.on("/", HTTP_GET, [this]() {
    extern AppState appState;
    server.send(200, "text/html", pageHtml(appState));
  });
  server.on("/save", HTTP_POST, [this]() {
    String ssid = server.arg("ssid");
    String password = server.arg("password");
    ssid.trim();
    if (ssid.length() == 0 || ssid.length() >= sizeof(pendingSsid)) {
      server.send(400, "text/plain", "SSID missing or too long");
      return;
    }
    strncpy(pendingSsid, ssid.c_str(), sizeof(pendingSsid));
    strncpy(pendingPassword, password.c_str(), sizeof(pendingPassword));
    pendingSsid[sizeof(pendingSsid) - 1] = '\0';
    pendingPassword[sizeof(pendingPassword) - 1] = '\0';
    credentialsPending = true;
    server.send(200, "text/html", "<h1>Wi-Fi setup</h1><p>Credentials saved. Connecting...</p>");
  });
  server.onNotFound([this]() {
    server.sendHeader("Location", "/", true);
    server.send(302, "text/plain", "");
  });
}

String CaptivePortalHandler::pageHtml(const AppState &state) const {
  String html;
  html.reserve(900);
  html += "<!doctype html><html><head><meta name='viewport' content='width=device-width,initial-scale=1'>";
  html += "<title>Wi-Fi setup</title></head><body><h1>Wi-Fi setup</h1>";
  html += "<p>AP SSID: ";
  html += state.captivePortalSsid;
  html += "</p><p>Setup IP: ";
  html += state.captivePortalIp;
  html += "</p><p>Progress: ";
  html += state.captivePortalProgress;
  html += "</p><form method='post' action='/save'>";
  html += "<p><input name='ssid' placeholder='SSID'></p>";
  html += "<p><input name='password' type='password' placeholder='Password'></p>";
  html += "<p><button type='submit'>Connect</button></p></form>";
  if (state.wifiConnected) {
    html += "<p>Connected IP: ";
    html += state.ipAddress;
    html += "</p>";
  }
  html += "</body></html>";
  return html;
}

void CaptivePortalHandler::start(AppState &state) {
  if (active) return;
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(state.captivePortalSsid);
  IPAddress ip = WiFi.softAPIP();
  strncpy(state.captivePortalIp, ip.toString().c_str(), sizeof(state.captivePortalIp));
  state.captivePortalIp[sizeof(state.captivePortalIp) - 1] = '\0';
  dns.start(DNS_PORT, "*", ip);
  server.begin();
  active = true;
  state.wifiProvisioning = true;
  strncpy(state.captivePortalProgress, "waiting for credentials", sizeof(state.captivePortalProgress));
  strncpy(state.wifiStatus, "SETUP AP", sizeof(state.wifiStatus));
  Serial.printf("Captive portal started: SSID %s IP %s\n", state.captivePortalSsid, state.captivePortalIp);
}

void CaptivePortalHandler::stop(AppState &state) {
  if (!active) return;
  server.stop();
  dns.stop();
  WiFi.softAPdisconnect(true);
  active = false;
  state.wifiProvisioning = false;
  strncpy(state.captivePortalProgress, "idle", sizeof(state.captivePortalProgress));
}

void CaptivePortalHandler::update(AppState &state) {
  if (!active) return;
  dns.processNextRequest();
  server.handleClient();
  if (credentialsPending) {
    strncpy(state.captivePortalProgress, "connecting", sizeof(state.captivePortalProgress));
  } else if (state.wifiConnected) {
    strncpy(state.captivePortalProgress, "connected", sizeof(state.captivePortalProgress));
  }
}

bool CaptivePortalHandler::isActive() const {
  return active;
}

bool CaptivePortalHandler::takeNewCredentials(char *ssid, size_t ssidLen, char *password, size_t passwordLen) {
  if (!credentialsPending) return false;
  strncpy(ssid, pendingSsid, ssidLen);
  strncpy(password, pendingPassword, passwordLen);
  ssid[ssidLen - 1] = '\0';
  password[passwordLen - 1] = '\0';
  credentialsPending = false;
  pendingSsid[0] = '\0';
  pendingPassword[0] = '\0';
  return true;
}
