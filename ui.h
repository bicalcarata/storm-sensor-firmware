#pragma once

#include <Arduino_GFX_Library.h>
#include "app_state.h"

void setupUi(Arduino_GFX *display);
void renderUi(AppState &state, AppPage page, TrendTab tab, bool resetConfirmVisible, bool force);
void drawHeader(const AppState &state, AppPage page);
void drawLivePage(const AppState &state);
void drawRadarPage(const AppState &state);
void drawStormPage(const AppState &state);
void drawAtmosPage(const AppState &state);
void drawTrendsPage(const AppState &state, TrendTab tab);
void drawSystemPage(const AppState &state);
void drawDisplayPage(const AppState &state);
void drawNetworkPage(const AppState &state, bool resetConfirmVisible);
void drawCard(int x, int y, int w, int h, const char *title, const char *value, uint16_t accent, bool primary = false);
void drawFlashSymbol(int x, int y, uint16_t colour, bool lightning, uint32_t eventMs);
bool pointInRect(uint16_t px, uint16_t py, int x, int y, int w, int h);
