#pragma once

#include <Arduino.h>
#include <WebServer.h>
#include "app_state.h"

class WebApiServer {
public:
  void begin(AppState &state);
  void start(AppState &state);
  void stop(AppState &state);
  void update(AppState &state);
  bool isActive() const;

private:
  void configureRoutes();
  String jsonStatus(const AppState &state) const;
  String jsonLightning(const AppState &state) const;
  String jsonEnvironment(const AppState &state) const;
  String jsonGps(const AppState &state) const;

  WebServer server{80};
  bool active = false;
};
