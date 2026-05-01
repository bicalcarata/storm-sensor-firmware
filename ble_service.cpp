#include "ble_service.h"

#include <string.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include "app_helpers.h"

static const char *BLE_SERVICE_UUID = "7f9d0001-7d2a-4d1d-9b8f-ad0000000001";
static const char *BLE_RATE_UUID = "7f9d0002-7d2a-4d1d-9b8f-ad0000000001";
static const char *BLE_DISTANCE_UUID = "7f9d0003-7d2a-4d1d-9b8f-ad0000000001";
static const char *BLE_COUNT_UUID = "7f9d0004-7d2a-4d1d-9b8f-ad0000000001";
static const char *BLE_ENV_UUID = "7f9d0005-7d2a-4d1d-9b8f-ad0000000001";
static const char *BLE_BATT_UUID = "7f9d0006-7d2a-4d1d-9b8f-ad0000000001";
static const char *BLE_STATUS_UUID = "7f9d0007-7d2a-4d1d-9b8f-ad0000000001";
static const char *BLE_PROFILE_UUID = "7f9d0008-7d2a-4d1d-9b8f-ad0000000001";

static BLEServer *bleServer = nullptr;
static BLECharacteristic *rateChar = nullptr;
static BLECharacteristic *distanceChar = nullptr;
static BLECharacteristic *countChar = nullptr;
static BLECharacteristic *envChar = nullptr;
static BLECharacteristic *batteryChar = nullptr;
static BLECharacteristic *statusChar = nullptr;
static BLECharacteristic *profileChar = nullptr;
static BleLightningService *activeService = nullptr;

class LightningBleCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer *) override {
    extern AppState appState;
    if (activeService) activeService->setClientCount(appState, appState.bleClientCount + 1);
  }
  void onDisconnect(BLEServer *) override {
    extern AppState appState;
    uint8_t next = appState.bleClientCount > 0 ? appState.bleClientCount - 1 : 0;
    if (activeService) activeService->setClientCount(appState, next);
    BLEDevice::startAdvertising();
  }
};

static BLECharacteristic *addReadNotifyCharacteristic(BLEService *service, const char *uuid) {
  return service->createCharacteristic(uuid, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY);
}

void BleLightningService::begin(AppState &state) {
  strncpy(state.bleDeviceName, "Lightning Detector", sizeof(state.bleDeviceName));
  state.bleDeviceName[sizeof(state.bleDeviceName) - 1] = '\0';
  strncpy(state.bleStatus, "OFF", sizeof(state.bleStatus));
  state.bleEnabled = false;
  activeService = this;
}

void BleLightningService::start(AppState &state) {
  if (!bleServer) {
    BLEDevice::init(state.bleDeviceName);
    bleServer = BLEDevice::createServer();
    bleServer->setCallbacks(new LightningBleCallbacks());
    BLEService *service = bleServer->createService(BLE_SERVICE_UUID);
    rateChar = addReadNotifyCharacteristic(service, BLE_RATE_UUID);
    distanceChar = addReadNotifyCharacteristic(service, BLE_DISTANCE_UUID);
    countChar = addReadNotifyCharacteristic(service, BLE_COUNT_UUID);
    envChar = addReadNotifyCharacteristic(service, BLE_ENV_UUID);
    batteryChar = addReadNotifyCharacteristic(service, BLE_BATT_UUID);
    statusChar = addReadNotifyCharacteristic(service, BLE_STATUS_UUID);
    profileChar = addReadNotifyCharacteristic(service, BLE_PROFILE_UUID);
    service->start();
    BLEAdvertising *advertising = BLEDevice::getAdvertising();
    advertising->addServiceUUID(BLE_SERVICE_UUID);
    advertising->setScanResponse(true);
  }
  BLEDevice::startAdvertising();
  active = true;
  state.bleEnabled = true;
  strncpy(state.bleStatus, "ADVERTISING", sizeof(state.bleStatus));
  Serial.println("BLE lightning service advertising");
}

void BleLightningService::stop(AppState &state) {
  if (active) {
    BLEDevice::stopAdvertising();
  }
  active = false;
  state.bleEnabled = false;
  state.bleClientCount = 0;
  strncpy(state.bleStatus, "OFF", sizeof(state.bleStatus));
}

void BleLightningService::setClientCount(AppState &state, uint8_t count) {
  state.bleClientCount = count;
  strncpy(state.bleStatus, count > 0 ? "CONNECTED" : "ADVERTISING", sizeof(state.bleStatus));
}

void BleLightningService::update(AppState &state) {
  if (!active || !rateChar) return;
  uint32_t now = millis();
  bool changed = state.ratePerMin != lastRate ||
                 state.sessionCount != lastStrikeCount ||
                 state.batteryPercent != lastBattery ||
                 strncmp(state.as3935ProfileText, lastProfile, sizeof(lastProfile)) != 0;
  if (!changed && now - lastNotifyMs < 5000UL) return;

  char buf[96];
  snprintf(buf, sizeof(buf), "%u", state.ratePerMin);
  rateChar->setValue(buf);
  rateChar->notify();
  if (state.lightningDataValid) {
    snprintf(buf, sizeof(buf), "%.1f km", state.distanceKm);
  } else {
    snprintf(buf, sizeof(buf), "-- km %s", rangeTrendName(state));
  }
  distanceChar->setValue(buf);
  distanceChar->notify();
  snprintf(buf, sizeof(buf), "%u", state.sessionCount);
  countChar->setValue(buf);
  countChar->notify();
  snprintf(buf, sizeof(buf), "%.1fC %u%% %.1fhPa", state.tempC, state.humidity, state.pressure);
  envChar->setValue(buf);
  envChar->notify();
  snprintf(buf, sizeof(buf), "%u%%", state.batteryPercent);
  batteryChar->setValue(buf);
  batteryChar->notify();
  snprintf(buf, sizeof(buf), "%s noise:%s", state.lightningLastInterrupt == 0x01 ? "NOISE" : "OK", state.thunderCandidate ? "yes" : "no");
  statusChar->setValue(buf);
  statusChar->notify();
  profileChar->setValue(state.as3935ProfileText);
  profileChar->notify();

  lastRate = state.ratePerMin;
  lastStrikeCount = state.sessionCount;
  lastBattery = state.batteryPercent;
  strncpy(lastProfile, state.as3935ProfileText, sizeof(lastProfile));
  lastNotifyMs = now;
}

bool BleLightningService::isActive() const {
  return active;
}
