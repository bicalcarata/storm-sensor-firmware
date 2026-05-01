#pragma once

#include <Arduino.h>
#include <DNSServer.h>
#include <WebServer.h>
#include "app_state.h"

class CaptivePortalHandler {
public:
  void begin(AppState &state);
  void start(AppState &state);
  void stop(AppState &state);
  void update(AppState &state);
  bool isActive() const;
  bool takeNewCredentials(char *ssid, size_t ssidLen, char *password, size_t passwordLen);

private:
  void configureRoutes();
  String pageHtml(const AppState &state) const;

  DNSServer dns;
  WebServer server{80};
  bool active = false;
  char pendingSsid[33] = {0};
  char pendingPassword[65] = {0};
  bool credentialsPending = false;
};
