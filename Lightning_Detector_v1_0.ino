#include <Arduino_GFX_Library.h>
#include <Wire.h>
#include <ctype.h>
#include <math.h>

#include "app_state.h"
#include "app_helpers.h"
#include "as3935_profile_manager.h"
#include "battery_status.h"
#include "ble_service.h"
#include "captive_portal_handler.h"
#include "colours.h"
#include "gps_manager.h"
#include "led_manager.h"
#include "mode_manager.h"
#include "ui.h"
#include "web_api_server.h"
#include "wifi_manager.h"

#define FW_BUILD_LABEL "v1.0"

static const uint16_t BOOT_MIN_HOLD_MS = 3000;

// Waveshare ESP32-S3 Touch AMOLED 2.41 display/QSPI pins.
#define AMOLED_CS 9
#define AMOLED_SCK 10
#define AMOLED_D0 11
#define AMOLED_D1 12
#define AMOLED_D2 13
#define AMOLED_D3 14
#define AMOLED_RST 21

// Shared I2C bus for TCA9554 and FT6336 touch.
#define I2C_SDA 47
#define I2C_SCL 48

// TCA9554 I/O expander used by Waveshare to gate AMOLED power.
#define TCA9554_ADDR 0x20
#define TCA9554_REG_OUTPUT 0x01
#define TCA9554_REG_CONFIG 0x03
#define EXIO1_AMOLED_EN 1
#define PIN_BAT_PWR 16
#define PIN_TP_RST 3

// FT6336 touch controller.
#define FT6336_ADDR 0x38
#define FT6336_REG_POINTS 0x02
#define FT6336_REG_P1_XH 0x03

// AS3935 lightning sensor.
#define PIN_SENSOR_IRQ 39
#define AS3935_REG_AFE_GAIN 0x00
#define AS3935_REG_INT 0x03
#define AS3935_REG_DISTANCE 0x07
#define AS3935_CMD_PRESET_DEFAULT 0x3C
#define AS3935_CMD_CALIB_RCO 0x3D
#define AS3935_INT_NOISE 0x01
#define AS3935_INT_DISTURBER 0x04
#define AS3935_INT_LIGHTNING 0x08

// BME280 environmental sensor.
#define BME280_REG_ID 0xD0
#define BME280_REG_RESET 0xE0
#define BME280_REG_CTRL_HUM 0xF2
#define BME280_REG_STATUS 0xF3
#define BME280_REG_CTRL_MEAS 0xF4
#define BME280_REG_CONFIG 0xF5
#define BME280_REG_DATA 0xF7

// Analog microphone module. GPIO4 is reserved for the onboard SD card clock.
// Move the mic signal wire to this ADC-capable GPIO, or change this define.
#define PIN_MIC_ADC 1
#define MIC_ADC_MAX 4095

Arduino_DataBus *bus = new Arduino_ESP32QSPI(
  AMOLED_CS, AMOLED_SCK, AMOLED_D0, AMOLED_D1, AMOLED_D2, AMOLED_D3
);

Arduino_RM690B0 *panel = new Arduino_RM690B0(
  bus,
  AMOLED_RST,
  0,
  450,
  600,
  16,
  0,
  16,
  0
);

Arduino_Canvas *gfx = new Arduino_Canvas(450, 600, panel, 0, 0, 0);

struct Bme280Calibration {
  uint16_t digT1;
  int16_t digT2;
  int16_t digT3;
  uint16_t digP1;
  int16_t digP2;
  int16_t digP3;
  int16_t digP4;
  int16_t digP5;
  int16_t digP6;
  int16_t digP7;
  int16_t digP8;
  int16_t digP9;
  uint8_t digH1;
  int16_t digH2;
  uint8_t digH3;
  int16_t digH4;
  int16_t digH5;
  int8_t digH6;
  int32_t tFine;
};

Bme280Calibration bmeCal;

AppState appState;
AppPage currentPage = PAGE_LIVE;
TrendTab currentTrendTab = TAB_RATE;
ModeManager modeManager;
DeviceWifiManager wifiManager;
CaptivePortalHandler captivePortal;
WebApiServer webApiServer;
BleLightningService bleService;
As3935ProfileManager as3935ProfileManager;
GpsManager gpsManager;

bool displayOK = false;
bool touchOK = false;
bool resetConfirmVisible = false;
bool bme280OK = false;
uint8_t bme280Addr = 0;
bool i2cBusFault = false;
bool lightningIrqPending = false;
bool touchDown = false;
uint16_t touchStartX = 0;
uint16_t touchStartY = 0;
uint16_t touchLastX = 0;
uint16_t touchLastY = 0;
uint32_t touchStartMs = 0;
uint32_t lastGestureMs = 0;
uint32_t lightningIrqSeenMs = 0;

const uint16_t EDGE_ZONE_PX = 64;
const uint16_t SWIPE_MIN_DELTA_PX = 70;
const uint16_t SWIPE_MAX_OFF_AXIS_PX = 80;
const uint16_t TAP_MAX_MOVE_PX = 26;
const uint16_t TOUCH_MIN_HOLD_MS = 35;
const uint16_t GESTURE_COOLDOWN_MS = 220;
const uint32_t BME280_READ_INTERVAL_MS = 5000;
const uint32_t MIC_SAMPLE_INTERVAL_MS = 20;
const uint32_t MIC_SERIAL_INTERVAL_MS = 500;
const float MIC_BASELINE_ALPHA_IDLE = 0.010f;
const float MIC_BASELINE_ALPHA_LISTENING = 0.002f;
const uint16_t MIC_DSP_SAMPLE_RATE_HZ = 4000;
const uint8_t MIC_DSP_SAMPLE_COUNT = 80;
const float THUNDER_LOW_BASELINE_ALPHA = 0.025f;
const float THUNDER_LOW_ENERGY_MULTIPLIER = 3.0f;
const float THUNDER_LOW_ENERGY_STRONG_MULTIPLIER = 6.0f;
const float THUNDER_RATIO_THRESHOLD = 1.8f;
const float THUNDER_RATIO_STRONG_THRESHOLD = 3.0f;
const uint16_t THUNDER_MIN_RUMBLE_MS = 500;
const uint16_t THUNDER_STRONG_RUMBLE_MS = 1200;
const uint16_t THUNDER_IMPULSE_REJECT_MS = 200;
const uint32_t THUNDER_WINDOW_MS = 150000UL;
const uint32_t THUNDER_COOLDOWN_MS = 12000UL;
const uint32_t STORM_INACTIVITY_RESET_MS = 30UL * 60UL * 1000UL;
const uint8_t STRIKE_BASE_SCORE = 60;
const uint8_t STRIKE_ACCEPT_SCORE = 75;
const uint8_t STRIKE_PROMPT_MIN_SCORE = 45;
const uint32_t STRIKE_PROMPT_TIMEOUT_MS = 20000UL;
const uint8_t MAX_PROMPTS_PER_10_MIN = 3;
const int8_t SESSION_BIAS_MIN = -20;
const int8_t SESSION_BIAS_MAX = 20;
const bool TRAINING_MODE_ENABLED = true;
const bool AUTO_LEARN_ENABLED = true;
const bool ALLOW_TRAINING_RESET = true;
const uint8_t MAX_NOISE_EVENTS = 32;
const uint8_t MAX_DISTURBER_EVENTS = 32;
const uint8_t ENV_SAMPLE_COUNT = 145;
const uint32_t ENV_SAMPLE_INTERVAL_MS = 5UL * 60UL * 1000UL;
uint32_t lastMicSampleMs = 0;
uint32_t lastMicSerialMs = 0;
uint32_t lastValidLightningMs = 0;
bool stormActive = false;

struct EnvSample {
  uint32_t ms;
  float pressure;
  float tempC;
  float humidity;
};

struct MicBandFrame {
  float lowEnergy;
  float highEnergy;
  float lowHighRatio;
  float peakDelta;
};

struct StrikeCandidate {
  bool active;
  bool accepted;
  float distanceKm;
  bool hasValidDistance;
  uint32_t eventMs;
  uint32_t promptUntilMs;
  bool promptOpen;
  bool closeWindow;
  uint8_t thunderDetections;
  uint8_t sharpImpulseCount;
  int8_t acousticConfidenceDelta;
  int16_t score;
  StrikeConfidenceDecision decision;
};

EnvSample envSamples[ENV_SAMPLE_COUNT];
uint8_t envSampleCount = 0;
uint8_t envSampleNext = 0;
uint32_t lastEnvSampleMs = 0;
uint32_t disturberEventMs[MAX_DISTURBER_EVENTS] = {0};
uint32_t noiseEventMs[MAX_NOISE_EVENTS] = {0};
uint32_t promptEventMs[MAX_PROMPTS_PER_10_MIN] = {0};
StrikeCandidate pendingStrike = {};
uint32_t thunderWindowOpenMs = 0;
float thunderLowBaseline = 1.0f;
uint32_t thunderRumbleStartMs = 0;
uint32_t thunderImpulseStartMs = 0;
bool thunderImpulseActive = false;

uint8_t displayBrightnessRegisterValue(uint8_t level) {
  level = constrain(level, 1, 10);
  return ((uint16_t)level * 255U + 5U) / 10U;
}

void applyDisplayBrightness() {
  if (!displayOK) return;
  panel->setBrightness(displayBrightnessRegisterValue(appState.displayBrightnessLevel));
}

void drawBootFallback() {
  if (!displayOK) return;
  gfx->fillScreen(UI_BLACK);
  gfx->setTextColor(UI_WHITE);
  gfx->setTextSize(3);
  gfx->setCursor(132, 168);
  gfx->print("STORM SENSOR SUITE");
  gfx->setTextColor(UI_CYAN);
  gfx->setTextSize(2);
  gfx->setCursor(154, 214);
  gfx->print("MONITOR  DETECT  ANALYSE");
  gfx->setTextColor(UI_WHITE);
  gfx->setCursor(232, 262);
  gfx->print("SYSTEM BOOT");
  gfx->flush();
}

void showBootScreen() {
  if (!displayOK) return;

  drawBootFallback();
  pulseBootLedsOnce();
}

void recoverI2CBus() {
  pinMode(I2C_SDA, INPUT_PULLUP);
  pinMode(I2C_SCL, OUTPUT);
  digitalWrite(I2C_SCL, HIGH);
  delayMicroseconds(5);
  for (uint8_t i = 0; i < 9 && digitalRead(I2C_SDA) == LOW; i++) {
    digitalWrite(I2C_SCL, LOW);
    delayMicroseconds(5);
    digitalWrite(I2C_SCL, HIGH);
    delayMicroseconds(5);
  }
  pinMode(I2C_SCL, INPUT_PULLUP);
}

void beginSharedI2C() {
  Wire.end();
  recoverI2CBus();
  Wire.begin(I2C_SDA, I2C_SCL);
  Wire.setClock(100000);
  Wire.setTimeOut(50);
  delay(20);
}

bool i2cDevicePresent(uint8_t addr) {
  if (digitalRead(I2C_SDA) == LOW || digitalRead(I2C_SCL) == LOW) {
    return false;
  }
  Wire.beginTransmission(addr);
  return Wire.endTransmission() == 0;
}

const char *knownI2CDeviceName(uint8_t addr) {
  switch (addr) {
    case 0x01: return "AS3935 option";
    case 0x02: return "AS3935 option";
    case 0x03: return "AS3935 option";
    case 0x20: return "TCA9554 AMOLED power";
    case 0x38: return "FT6336 touch";
    case 0x51: return "board peripheral";
    case 0x6B: return "board peripheral";
    case 0x76: return "BME280";
    case 0x77: return "BME280 alt";
    default: return "";
  }
}

void logI2CScan(const char *label) {
  Serial.printf("I2C scan %s:", label);
  bool any = false;
  uint8_t foundCount = 0;
  for (uint8_t addr = 1; addr < 0x78; addr++) {
    if (i2cDevicePresent(addr)) {
      Serial.printf(" 0x%02X", addr);
      any = true;
      foundCount++;
    }
  }
  if (!any) {
    Serial.print(" none");
  }
  Serial.println();

  if (digitalRead(I2C_SDA) == LOW || digitalRead(I2C_SCL) == LOW || foundCount > 16) {
    i2cBusFault = true;
    Serial.printf("ERROR: I2C bus fault suspected. SDA=%u SCL=%u device_count=%u\n",
                  digitalRead(I2C_SDA), digitalRead(I2C_SCL), foundCount);
    Serial.println("ERROR: If every address appears present, check for SDA held low, shorts, wrong sensor mode, or pullup/power issues.");
  } else {
    i2cBusFault = false;
  }
  Serial.flush();
}

void logFullI2CScan(const char *label) {
  uint8_t foundCount = 0;

  Serial.printf("FULL I2C scan summary: %s", label);
  for (uint8_t addr = 1; addr < 0x78; addr++) {
    if (digitalRead(I2C_SDA) == HIGH && digitalRead(I2C_SCL) == HIGH) {
      Wire.beginTransmission(addr);
      if (Wire.endTransmission() == 0) {
        Serial.printf(" 0x%02X", addr);
        foundCount++;
      }
    }
  }
  if (foundCount == 0) Serial.print(" none");
  Serial.println();

  if (digitalRead(I2C_SDA) == LOW || digitalRead(I2C_SCL) == LOW || foundCount > 16) {
    Serial.println("I2C bus fault suspected");
  }
  Serial.flush();
}

bool tcaReadReg(uint8_t reg, uint8_t &value) {
  Wire.beginTransmission(TCA9554_ADDR);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom(TCA9554_ADDR, (uint8_t)1) != 1) return false;
  value = Wire.read();
  return true;
}

bool tcaWriteReg(uint8_t reg, uint8_t value) {
  Wire.beginTransmission(TCA9554_ADDR);
  Wire.write(reg);
  Wire.write(value);
  return Wire.endTransmission() == 0;
}

bool setExioBit(uint8_t bit, bool level) {
  uint8_t config;
  uint8_t output;
  if (!tcaReadReg(TCA9554_REG_CONFIG, config)) return false;
  if (!tcaReadReg(TCA9554_REG_OUTPUT, output)) return false;
  config &= ~(1 << bit);
  if (level) {
    output |= (1 << bit);
  } else {
    output &= ~(1 << bit);
  }
  return tcaWriteReg(TCA9554_REG_OUTPUT, output) && tcaWriteReg(TCA9554_REG_CONFIG, config);
}

bool touchReadRegs(uint8_t reg, uint8_t *buf, uint8_t len) {
  Wire.beginTransmission(FT6336_ADDR);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom(FT6336_ADDR, len) != len) return false;
  for (uint8_t i = 0; i < len; i++) {
    buf[i] = Wire.read();
  }
  return true;
}

bool i2cReadReg8(uint8_t addr, uint8_t reg, uint8_t &value) {
  Wire.beginTransmission(addr);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom(addr, (uint8_t)1) != 1) return false;
  value = Wire.read();
  return true;
}

bool i2cWriteReg8(uint8_t addr, uint8_t reg, uint8_t value) {
  Wire.beginTransmission(addr);
  Wire.write(reg);
  Wire.write(value);
  return Wire.endTransmission() == 0;
}

bool i2cReadBlock(uint8_t addr, uint8_t reg, uint8_t *buf, uint8_t len) {
  Wire.beginTransmission(addr);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom(addr, len) != len) return false;
  for (uint8_t i = 0; i < len; i++) {
    buf[i] = Wire.read();
  }
  return true;
}

uint16_t u16le(const uint8_t *b) {
  return ((uint16_t)b[1] << 8) | b[0];
}

int16_t s16le(const uint8_t *b) {
  return (int16_t)u16le(b);
}

bool readTouchRaw(uint16_t &x, uint16_t &y) {
  uint8_t points = 0;
  if (!touchReadRegs(FT6336_REG_POINTS, &points, 1)) return false;
  points &= 0x0F;
  if (points == 0 || points > 5) return false;

  uint8_t xy[4] = {0};
  if (!touchReadRegs(FT6336_REG_P1_XH, xy, 4)) return false;
  x = ((uint16_t)(xy[0] & 0x0F) << 8) | xy[1];
  y = ((uint16_t)(xy[2] & 0x0F) << 8) | xy[3];
  return true;
}

bool readTouchLandscape(uint16_t &x, uint16_t &y) {
  uint16_t rawX;
  uint16_t rawY;
  if (!readTouchRaw(rawX, rawY)) return false;

  uint16_t mappedX = 599 - rawY;
  uint16_t mappedY = rawX;
  if (mappedX >= gfx->width()) mappedX = gfx->width() - 1;
  if (mappedY >= gfx->height()) mappedY = gfx->height() - 1;
  x = mappedX;
  y = mappedY;
  return true;
}

void setupDisplay() {
  pinMode(PIN_BAT_PWR, OUTPUT);
  digitalWrite(PIN_BAT_PWR, HIGH);
  Serial.println("BAT_PWR enabled");

  pinMode(AMOLED_RST, OUTPUT);
  digitalWrite(AMOLED_RST, HIGH);
  delay(20);

  beginSharedI2C();
  delay(50);

  if (!setExioBit(EXIO1_AMOLED_EN, true)) {
    Serial.println("WARNING: Failed to enable AMOLED via TCA9554");
  } else {
    Serial.println("AMOLED power enabled");
  }
  delay(120);

  digitalWrite(AMOLED_RST, LOW);
  delay(20);
  digitalWrite(AMOLED_RST, HIGH);
  delay(180);

  logI2CScan("after display power");
  logFullI2CScan("after display power");

  displayOK = false;
  for (uint8_t attempt = 1; attempt <= 2 && !displayOK; attempt++) {
    displayOK = gfx->begin();
    if (!displayOK) {
      Serial.printf("WARNING: gfx->begin() failed on attempt %u\n", attempt);
      setExioBit(EXIO1_AMOLED_EN, false);
      delay(80);
      setExioBit(EXIO1_AMOLED_EN, true);
      delay(180);
      digitalWrite(AMOLED_RST, LOW);
      delay(20);
      digitalWrite(AMOLED_RST, HIGH);
      delay(180);
    }
  }
  if (!displayOK) {
    Serial.println("ERROR: gfx->begin() failed");
    Serial.println("Loop will continue without display drawing.");
    return;
  }

  gfx->setRotation(1);
  applyDisplayBrightness();
  gfx->fillScreen(UI_BLACK);
  setupUi(gfx);
  Serial.printf("Display init OK: %dx%d\n", gfx->width(), gfx->height());
}

void setupTouch() {
  touchOK = false;

  if (i2cBusFault) {
    Serial.println("WARNING: Skipping FT6336 touch init because I2C bus is faulted.");
    return;
  }

  for (uint8_t attempt = 1; attempt <= 3; attempt++) {
    pinMode(PIN_TP_RST, OUTPUT);
    digitalWrite(PIN_TP_RST, LOW);
    delay(12);
    digitalWrite(PIN_TP_RST, HIGH);
    delay(90);

    uint8_t touchPointStatus = 0;
    if (i2cDevicePresent(FT6336_ADDR) &&
        touchReadRegs(FT6336_REG_POINTS, &touchPointStatus, 1)) {
      touchOK = true;
      Serial.printf("Touch init OK at 0x%02X on attempt %u\n", FT6336_ADDR, attempt);
      return;
    }

    Serial.printf("WARNING: FT6336 touch probe failed on attempt %u\n", attempt);
    beginSharedI2C();
  }

  Serial.println("WARNING: FT6336 touch not responding; UI will still run.");
}

bool readBme280Now() {
  if (!bme280OK) return false;

  uint8_t data[8] = {0};
  if (!i2cReadBlock(bme280Addr, BME280_REG_DATA, data, sizeof(data))) {
    Serial.println("WARNING: BME280 read failed");
    return false;
  }

  int32_t adcP = ((uint32_t)data[0] << 12) | ((uint32_t)data[1] << 4) | (data[2] >> 4);
  int32_t adcT = ((uint32_t)data[3] << 12) | ((uint32_t)data[4] << 4) | (data[5] >> 4);
  int32_t adcH = ((uint32_t)data[6] << 8) | data[7];

  int32_t var1 = ((((adcT >> 3) - ((int32_t)bmeCal.digT1 << 1))) * ((int32_t)bmeCal.digT2)) >> 11;
  int32_t var2 = (((((adcT >> 4) - ((int32_t)bmeCal.digT1)) * ((adcT >> 4) - ((int32_t)bmeCal.digT1))) >> 12) * ((int32_t)bmeCal.digT3)) >> 14;
  bmeCal.tFine = var1 + var2;
  float tempC = (bmeCal.tFine * 5 + 128) / 256.0f / 100.0f;

  int64_t pVar1 = ((int64_t)bmeCal.tFine) - 128000;
  int64_t pVar2 = pVar1 * pVar1 * (int64_t)bmeCal.digP6;
  pVar2 = pVar2 + ((pVar1 * (int64_t)bmeCal.digP5) << 17);
  pVar2 = pVar2 + (((int64_t)bmeCal.digP4) << 35);
  pVar1 = ((pVar1 * pVar1 * (int64_t)bmeCal.digP3) >> 8) + ((pVar1 * (int64_t)bmeCal.digP2) << 12);
  pVar1 = (((((int64_t)1) << 47) + pVar1)) * ((int64_t)bmeCal.digP1) >> 33;
  float pressureHpa = NAN;
  if (pVar1 != 0) {
    int64_t p = 1048576 - adcP;
    p = (((p << 31) - pVar2) * 3125) / pVar1;
    pVar1 = (((int64_t)bmeCal.digP9) * (p >> 13) * (p >> 13)) >> 25;
    pVar2 = (((int64_t)bmeCal.digP8) * p) >> 19;
    p = ((p + pVar1 + pVar2) >> 8) + (((int64_t)bmeCal.digP7) << 4);
    pressureHpa = (float)p / 25600.0f;
  }

  int32_t vX1 = bmeCal.tFine - 76800;
  vX1 = (((((adcH << 14) - (((int32_t)bmeCal.digH4) << 20) - (((int32_t)bmeCal.digH5) * vX1)) + 16384) >> 15) *
          (((((((vX1 * ((int32_t)bmeCal.digH6)) >> 10) * (((vX1 * ((int32_t)bmeCal.digH3)) >> 11) + 32768)) >> 10) + 2097152) *
             ((int32_t)bmeCal.digH2) + 8192) >> 14));
  vX1 = vX1 - (((((vX1 >> 15) * (vX1 >> 15)) >> 7) * ((int32_t)bmeCal.digH1)) >> 4);
  vX1 = constrain(vX1, 0, 419430400);
  float humidity = (vX1 >> 12) / 1024.0f;

  if (!isfinite(tempC) || !isfinite(humidity) || !isfinite(pressureHpa)) {
    Serial.println("WARNING: BME280 returned invalid reading");
    return false;
  }

  appState.tempC = tempC;
  appState.humidity = constrain((int)roundf(humidity), 0, 100);
  appState.pressure = pressureHpa;
  appState.bme280Present = true;
  appState.lastBmeReadMs = millis();
  recordEnvironmentSample(appState.lastBmeReadMs);
  appendEnvironmentHistory(appState);
  Serial.printf("BME280 live update: %.1f C, %u%%, %.1f hPa\n", appState.tempC, appState.humidity, appState.pressure);
  return true;
}

void setupBme280() {
  if (i2cBusFault) {
    appState.bme280Present = false;
    Serial.println("WARNING: Skipping BME280 init because I2C bus is faulted.");
    return;
  }

  uint8_t id = 0;
  if (i2cReadReg8(0x76, BME280_REG_ID, id) && id == 0x60) {
    bme280Addr = 0x76;
  } else if (i2cReadReg8(0x77, BME280_REG_ID, id) && id == 0x60) {
    bme280Addr = 0x77;
  }

  if (bme280Addr == 0) {
    appState.bme280Present = false;
    Serial.println("WARNING: BME280 not found at 0x76 or 0x77; environment UI will show no data.");
    return;
  }

  i2cWriteReg8(bme280Addr, BME280_REG_RESET, 0xB6);
  delay(4);

  uint8_t cal1[26] = {0};
  uint8_t cal2[7] = {0};
  if (!i2cReadBlock(bme280Addr, 0x88, cal1, sizeof(cal1)) ||
      !i2cReadBlock(bme280Addr, 0xE1, cal2, sizeof(cal2))) {
    Serial.println("WARNING: BME280 calibration read failed");
    return;
  }

  bmeCal.digT1 = u16le(&cal1[0]);
  bmeCal.digT2 = s16le(&cal1[2]);
  bmeCal.digT3 = s16le(&cal1[4]);
  bmeCal.digP1 = u16le(&cal1[6]);
  bmeCal.digP2 = s16le(&cal1[8]);
  bmeCal.digP3 = s16le(&cal1[10]);
  bmeCal.digP4 = s16le(&cal1[12]);
  bmeCal.digP5 = s16le(&cal1[14]);
  bmeCal.digP6 = s16le(&cal1[16]);
  bmeCal.digP7 = s16le(&cal1[18]);
  bmeCal.digP8 = s16le(&cal1[20]);
  bmeCal.digP9 = s16le(&cal1[22]);
  bmeCal.digH1 = cal1[25];
  bmeCal.digH2 = s16le(&cal2[0]);
  bmeCal.digH3 = cal2[2];
  bmeCal.digH4 = ((int16_t)cal2[3] << 4) | (cal2[4] & 0x0F);
  bmeCal.digH5 = ((int16_t)cal2[5] << 4) | (cal2[4] >> 4);
  bmeCal.digH6 = (int8_t)cal2[6];

  i2cWriteReg8(bme280Addr, BME280_REG_CTRL_HUM, 0x01);
  i2cWriteReg8(bme280Addr, BME280_REG_CONFIG, 0xA0);
  i2cWriteReg8(bme280Addr, BME280_REG_CTRL_MEAS, 0x57);

  bme280OK = true;
  Serial.println("BME280 init OK");
  if (readBme280Now()) {
    seedEnvironmentHistory(appState);
    Serial.printf("BME280: %.1f C, %u%%, %.1f hPa\n", appState.tempC, appState.humidity, appState.pressure);
  }
}

bool as3935Read(uint8_t reg, uint8_t &value) {
  if (!appState.lightningPresent) return false;
  return i2cReadReg8(appState.lightningI2cAddr, reg, value);
}

bool as3935Write(uint8_t reg, uint8_t value) {
  if (!appState.lightningPresent) return false;
  return i2cWriteReg8(appState.lightningI2cAddr, reg, value);
}

void setupLightningSensor() {
  pinMode(PIN_SENSOR_IRQ, INPUT);

  if (i2cBusFault) {
    appState.lightningPresent = false;
    Serial.println("WARNING: Skipping AS3935 init because I2C bus is faulted.");
    return;
  }

  const uint8_t addresses[] = {0x03, 0x02, 0x01};
  uint8_t probe = 0;
  for (uint8_t i = 0; i < sizeof(addresses); i++) {
    if (i2cReadReg8(addresses[i], AS3935_REG_AFE_GAIN, probe)) {
      appState.lightningPresent = true;
      appState.lightningI2cAddr = addresses[i];
      break;
    }
  }

  if (!appState.lightningPresent) {
    Serial.println("WARNING: AS3935 lightning sensor not found at 0x01/0x02/0x03.");
    return;
  }

  Serial.printf("AS3935 found at 0x%02X, IRQ GPIO%d\n", appState.lightningI2cAddr, PIN_SENSOR_IRQ);
  as3935Write(AS3935_CMD_PRESET_DEFAULT, 0x96);
  delay(2);
  as3935Write(AS3935_CMD_CALIB_RCO, 0x96);
  delay(2);

  uint8_t reg0 = 0;
  if (as3935Read(AS3935_REG_AFE_GAIN, reg0)) {
    // Power up; the mode-specific AFE gain is applied by As3935ProfileManager.
    reg0 &= ~0x01;
    as3935Write(AS3935_REG_AFE_GAIN, reg0);
  }

  uint8_t interruptReg = 0;
  as3935Read(AS3935_REG_INT, interruptReg);
  as3935ProfileManager.applyForMode(appState, appState.mode);
}

void setupMicrophone() {
  analogReadResolution(12);
  analogSetPinAttenuation(PIN_MIC_ADC, ADC_11db);
  pinMode(PIN_MIC_ADC, INPUT);
  delay(20);

  uint32_t sum = 0;
  const uint8_t samples = 32;
  for (uint8_t i = 0; i < samples; i++) {
    sum += analogRead(PIN_MIC_ADC);
    delay(2);
  }

  appState.micRaw = sum / samples;
  appState.micBaseline = appState.micRaw;
  appState.micDelta = 0.0f;
  appState.thunderCandidate = false;
  appState.thunderConfirmed = false;
  appState.thunderListening = false;
  appState.thunderDelaySec = 0.0f;
  appState.audioDistanceKm = 0.0f;
  appState.thunderCandidateSinceMs = 0;
  appState.thunderCooldownUntilMs = 0;
  thunderLowBaseline = 1.0f;
  thunderRumbleStartMs = 0;
  thunderImpulseStartMs = 0;
  thunderImpulseActive = false;
  lastMicSampleMs = millis();

  Serial.printf("Microphone ADC ready on GPIO%d: baseline=%u, thunder band=%u-%u Hz\n",
                PIN_MIC_ADC, appState.micRaw, 40, 180);
}

bool isValidAs3935DistanceBucket(uint8_t rawDistance) {
  switch (rawDistance) {
    case 1:
    case 5:
    case 6:
    case 8:
    case 10:
    case 12:
    case 14:
    case 17:
    case 20:
    case 24:
    case 27:
    case 31:
    case 34:
    case 37:
    case 40:
      return true;
    default:
      return false;
  }
}

int16_t clampScore(int16_t score) {
  return constrain(score, 0, 100);
}

const char *confidenceDecisionName(StrikeConfidenceDecision decision) {
  switch (decision) {
    case STRIKE_DECISION_ACCEPT: return "ACCEPT";
    case STRIKE_DECISION_PROMPT: return "PROMPT";
    default: return "SUPPRESS";
  }
}

StrikeConfidenceDecision decisionForScore(int16_t score) {
  if (score >= STRIKE_ACCEPT_SCORE) return STRIKE_DECISION_ACCEPT;
  if (score >= STRIKE_PROMPT_MIN_SCORE) return STRIKE_DECISION_PROMPT;
  return STRIKE_DECISION_SUPPRESS;
}

void rememberTimedEvent(uint32_t *events, uint8_t maxEvents, uint32_t now) {
  for (int8_t i = maxEvents - 1; i > 0; i--) {
    events[i] = events[i - 1];
  }
  events[0] = now;
}

uint8_t countTimedEvents(const uint32_t *events, uint8_t maxEvents, uint32_t now, uint32_t windowMs) {
  uint8_t count = 0;
  for (uint8_t i = 0; i < maxEvents; i++) {
    if (events[i] != 0 && (uint32_t)(now - events[i]) <= windowMs) count++;
  }
  return count;
}

bool canOpenPrompt(uint32_t now) {
  return countTimedEvents(promptEventMs, MAX_PROMPTS_PER_10_MIN, now, 10UL * 60UL * 1000UL) < MAX_PROMPTS_PER_10_MIN;
}

void applySessionBiasDelta(int8_t delta) {
  if (!AUTO_LEARN_ENABLED) return;
  appState.strikeSessionBias = constrain((int)appState.strikeSessionBias + delta, SESSION_BIAS_MIN, SESSION_BIAS_MAX);
}

void resetTrainingBias() {
  if (!ALLOW_TRAINING_RESET) return;
  appState.strikeSessionBias = 0;
  Serial.println("Strike confidence session bias reset");
}

void recordEnvironmentSample(uint32_t now) {
  if (!appState.bme280Present || !isfinite(appState.pressure) || !isfinite(appState.tempC)) return;
  if (envSampleCount > 0 && (uint32_t)(now - lastEnvSampleMs) < ENV_SAMPLE_INTERVAL_MS) return;

  envSamples[envSampleNext].ms = now;
  envSamples[envSampleNext].pressure = appState.pressure;
  envSamples[envSampleNext].tempC = appState.tempC;
  envSamples[envSampleNext].humidity = appState.humidity;
  envSampleNext = (envSampleNext + 1) % ENV_SAMPLE_COUNT;
  if (envSampleCount < ENV_SAMPLE_COUNT) envSampleCount++;
  lastEnvSampleMs = now;
}

bool envSampleAtLeastAgo(uint32_t now, uint32_t ageMs, EnvSample &sample) {
  bool found = false;
  uint32_t bestAge = 0;
  for (uint8_t i = 0; i < envSampleCount; i++) {
    uint32_t age = now - envSamples[i].ms;
    if (age >= ageMs && (!found || age < bestAge)) {
      sample = envSamples[i];
      bestAge = age;
      found = true;
    }
  }
  return found;
}

float pressureDeltaForAge(uint32_t now, uint32_t ageMs) {
  EnvSample sample;
  if (!envSampleAtLeastAgo(now, ageMs, sample) || !isfinite(appState.pressure)) return NAN;
  return appState.pressure - sample.pressure;
}

float tempDeltaForAge(uint32_t now, uint32_t ageMs) {
  EnvSample sample;
  if (!envSampleAtLeastAgo(now, ageMs, sample) || !isfinite(appState.tempC)) return NAN;
  return appState.tempC - sample.tempC;
}

float humidityDeltaForAge(uint32_t now, uint32_t ageMs) {
  EnvSample sample;
  if (!envSampleAtLeastAgo(now, ageMs, sample)) return NAN;
  return (float)appState.humidity - sample.humidity;
}

bool pressureStable(float pressureDrop1h) {
  return isfinite(pressureDrop1h) && fabsf(pressureDrop1h) < 0.5f;
}

bool tempStable(float tempDrop1h) {
  return isfinite(tempDrop1h) && fabsf(tempDrop1h) < 0.5f;
}

bool humidityStable(float humidityRise1h) {
  return isfinite(humidityRise1h) && fabsf(humidityRise1h) < 3.0f;
}

uint8_t countAcceptedStrikes(uint32_t now, uint32_t windowMs) {
  return countTimedEvents(appState.strikeEventMs, MAX_RATE_EVENTS, now, windowMs);
}

bool strikeRateIncreasing(uint32_t now) {
  uint8_t recent10 = 0;
  uint8_t previous10 = 0;
  for (uint8_t i = 0; i < MAX_RATE_EVENTS; i++) {
    if (appState.strikeEventMs[i] == 0) continue;
    uint32_t age = now - appState.strikeEventMs[i];
    if (age <= 10UL * 60UL * 1000UL) recent10++;
    else if (age <= 20UL * 60UL * 1000UL) previous10++;
  }
  return recent10 >= 2 && recent10 > previous10;
}

bool distanceIncoming(float candidateKm, bool hasValidDistance) {
  if (!hasValidDistance || !appState.lightningDataValid) return false;
  return candidateKm + 0.5f < appState.distanceKm;
}

bool farMidNearPattern(float candidateKm, bool hasValidDistance) {
  if (!hasValidDistance || appState.strikeCount < 2) return false;
  float newest = appState.strikes[0].distanceKm;
  float older = appState.strikes[1].distanceKm;
  return older >= 20.0f && newest >= 8.0f && newest < older && candidateKm <= 10.0f && candidateKm < newest;
}

bool stableCoherentDistance(float candidateKm, bool hasValidDistance) {
  if (!hasValidDistance || appState.strikeCount < 2) return false;
  return fabsf(candidateKm - appState.strikes[0].distanceKm) <= 4.0f &&
         fabsf(candidateKm - appState.strikes[1].distanceKm) <= 4.0f;
}

bool environmentStrong(float pressureDrop1h, float humidityRise1h, float tempDrop1h) {
  return isfinite(pressureDrop1h) && isfinite(humidityRise1h) && isfinite(tempDrop1h) &&
         pressureDrop1h <= -2.0f && humidityRise1h >= 10.0f && tempDrop1h <= -1.0f;
}

int16_t scoreStrikeCandidate(float distanceKm,
                             bool hasValidDistance,
                             uint32_t now,
                             uint8_t thunderDetections,
                             uint8_t sharpImpulseCount,
                             int8_t acousticConfidenceDelta) {
  int16_t score = STRIKE_BASE_SCORE + appState.strikeSessionBias;

  float pressureDrop1h = pressureDeltaForAge(now, 60UL * 60UL * 1000UL);
  float pressureDrop3h = pressureDeltaForAge(now, 3UL * 60UL * 60UL * 1000UL);
  float pressureDrop12h = pressureDeltaForAge(now, 12UL * 60UL * 60UL * 1000UL);
  float tempDrop1h = tempDeltaForAge(now, 60UL * 60UL * 1000UL);
  float humidityRise1h = humidityDeltaForAge(now, 60UL * 60UL * 1000UL);

  if (isfinite(pressureDrop1h)) {
    if (pressureDrop1h <= -3.0f) score += 10;
    else if (pressureDrop1h <= -2.0f) score += 6;
    else if (pressureDrop1h <= -1.0f) score += 3;
  }
  if (isfinite(pressureDrop3h)) {
    if (pressureDrop3h <= -6.0f) score += 18;
    else if (pressureDrop3h <= -4.0f) score += 12;
    else if (pressureDrop3h <= -3.0f) score += 8;
  }
  if (isfinite(pressureDrop12h)) {
    if (pressureDrop12h <= -10.0f) score += 15;
    else if (pressureDrop12h <= -8.0f) score += 10;
  }
  if (isfinite(appState.pressure)) {
    if (appState.pressure < 1000.0f) score += 10;
    else if (appState.pressure < 1008.0f) score += 6;
    else if (appState.pressure > 1022.0f) score -= 4;
  }
  if (isfinite(pressureDrop1h) && isfinite(pressureDrop3h)) {
    float previous2h = pressureDrop3h - pressureDrop1h;
    if (pressureDrop1h <= -2.0f && previous2h <= -1.0f && fabsf(pressureDrop1h) >= fabsf(previous2h) * 1.4f) score += 8;
    else if (pressureDrop1h <= -1.0f && previous2h <= -0.5f && fabsf(pressureDrop1h) > fabsf(previous2h)) score += 4;
  }

  if (isfinite(tempDrop1h)) {
    if (tempDrop1h <= -3.0f) score += 8;
    else if (tempDrop1h <= -2.0f) score += 5;
    else if (tempDrop1h <= -1.0f) score += 2;
  }
  if (tempStable(tempDrop1h) && pressureStable(pressureDrop1h) && humidityStable(humidityRise1h)) score -= 6;

  if (isfinite(humidityRise1h)) {
    if (humidityRise1h >= 15.0f) score += 8;
    else if (humidityRise1h >= 10.0f) score += 5;
    else if (humidityRise1h >= 5.0f) score += 2;
    if (appState.humidity >= 80 && humidityRise1h > 0.0f) score += 5;
    if (appState.humidity < 50 && humidityStable(humidityRise1h)) score -= 4;
  }

  bool pressureFalling = isfinite(pressureDrop1h) && pressureDrop1h <= -1.0f;
  bool humidityRising = isfinite(humidityRise1h) && humidityRise1h >= 5.0f;
  bool tempFalling = isfinite(tempDrop1h) && tempDrop1h <= -1.0f;
  if (pressureFalling && humidityRising && tempFalling) score += 12;
  else if (pressureFalling && humidityRising) score += 7;
  else if (pressureFalling && tempFalling) score += 5;
  if (environmentStrong(pressureDrop1h, humidityRise1h, tempDrop1h)) score += 15;

  uint8_t strikes10 = countAcceptedStrikes(now, 10UL * 60UL * 1000UL);
  uint8_t strikes30 = countAcceptedStrikes(now, 30UL * 60UL * 1000UL);
  if (strikes10 >= 3) score += 8;
  if (strikes30 >= 5) score += 10;
  if (strikeRateIncreasing(now)) score += 8;
  if (distanceIncoming(distanceKm, hasValidDistance)) score += 10;
  if (farMidNearPattern(distanceKm, hasValidDistance)) score += 12;
  else if (stableCoherentDistance(distanceKm, hasValidDistance)) score += 5;

  bool noPriorStrikes = strikes30 == 0;
  if (hasValidDistance && noPriorStrikes) {
    if (distanceKm <= 5.0f) score -= 25;
    else if (distanceKm <= 10.0f) score -= 15;
    if (distanceKm <= 10.0f && environmentStrong(pressureDrop1h, humidityRise1h, tempDrop1h)) score += 10;
  }

  uint8_t disturbers10 = countTimedEvents(disturberEventMs, MAX_DISTURBER_EVENTS, now, 10UL * 60UL * 1000UL);
  uint8_t noise10 = countTimedEvents(noiseEventMs, MAX_NOISE_EVENTS, now, 10UL * 60UL * 1000UL);
  if (disturbers10 >= 10) score -= 20;
  else if (disturbers10 >= 5) score -= 12;
  else if (disturbers10 >= 2) score -= 5;
  if (noise10 >= 5) score -= 18;
  else if (noise10 >= 2) score -= 8;
  if (disturbers10 >= 5 && noise10 >= 2) score -= 15;

  score += acousticConfidenceDelta;
  if (thunderDetections > 1 && acousticConfidenceDelta > 0) score += 3;
  if (sharpImpulseCount > 0 && acousticConfidenceDelta >= 0 && thunderDetections == 0) score -= 3;

  return clampScore(score);
}

void clearPendingStrike() {
  pendingStrike.active = false;
  pendingStrike.accepted = false;
  pendingStrike.promptOpen = false;
  appState.strikePromptPending = false;
}

void acceptPendingStrike(const char *source) {
  if (!pendingStrike.active) return;
  if (pendingStrike.accepted) return;
  uint8_t acceptedScore = pendingStrike.score;
  float distanceKm = pendingStrike.distanceKm;
  bool hasValidDistance = pendingStrike.hasValidDistance;
  pendingStrike.accepted = true;
  pendingStrike.promptOpen = false;
  appState.strikePromptPending = false;
  appState.strikeConfidenceScore = acceptedScore;
  appState.strikeConfidenceDecision = STRIKE_DECISION_ACCEPT;
  addLightningStrike(distanceKm, hasValidDistance);
  Serial.printf("Strike confidence ACCEPT via %s: score=%u bias=%d\n",
                source, acceptedScore, appState.strikeSessionBias);
}

void suppressPendingStrike(const char *source) {
  if (!pendingStrike.active) return;
  if (pendingStrike.accepted) {
    appState.strikeConfidenceScore = pendingStrike.score;
    appState.strikeConfidenceDecision = STRIKE_DECISION_ACCEPT;
    Serial.printf("Strike confidence retained accepted event via %s: score=%u bias=%d\n",
                  source, pendingStrike.score, appState.strikeSessionBias);
    return;
  }
  uint8_t suppressedScore = pendingStrike.score;
  clearPendingStrike();
  appState.strikeConfidenceScore = suppressedScore;
  appState.strikeConfidenceDecision = STRIKE_DECISION_SUPPRESS;
  Serial.printf("Strike confidence SUPPRESS via %s: score=%u bias=%d\n",
                source, suppressedScore, appState.strikeSessionBias);
}

void rescorePendingStrike(const char *source) {
  if (!pendingStrike.active) return;
  uint32_t now = millis();
  pendingStrike.score = scoreStrikeCandidate(pendingStrike.distanceKm,
                                             pendingStrike.hasValidDistance,
                                             now,
                                             pendingStrike.thunderDetections,
                                             pendingStrike.sharpImpulseCount,
                                             pendingStrike.acousticConfidenceDelta);
  pendingStrike.decision = decisionForScore(pendingStrike.score);
  appState.strikeConfidenceScore = pendingStrike.score;
  appState.strikeConfidenceDecision = pendingStrike.accepted ? STRIKE_DECISION_ACCEPT : pendingStrike.decision;

  if (pendingStrike.accepted) {
    Serial.printf("Strike confidence acoustic update after %s: score=%u acoustic=%+d\n",
                  source, pendingStrike.score, pendingStrike.acousticConfidenceDelta);
    return;
  }

  if (pendingStrike.decision == STRIKE_DECISION_ACCEPT) {
    acceptPendingStrike(source);
  } else if (pendingStrike.decision == STRIKE_DECISION_PROMPT &&
             appState.strikeTrainingModeEnabled &&
             !pendingStrike.promptOpen &&
             canOpenPrompt(now)) {
    pendingStrike.promptOpen = true;
    pendingStrike.promptUntilMs = now + STRIKE_PROMPT_TIMEOUT_MS;
    appState.strikePromptPending = true;
    rememberTimedEvent(promptEventMs, MAX_PROMPTS_PER_10_MIN, now);
    Serial.printf("Strike confidence PROMPT: score=%u. Reply Y/N/U within %lus.\n",
                  pendingStrike.score, STRIKE_PROMPT_TIMEOUT_MS / 1000UL);
  } else {
    Serial.printf("Strike confidence %s after %s: score=%u\n",
                  confidenceDecisionName(pendingStrike.decision), source, pendingStrike.score);
  }
}

void thunderWindowForDistance(float distanceKm, bool hasValidDistance, uint32_t &openDelayMs, uint32_t &closeDelayMs) {
  if (!hasValidDistance) {
    openDelayMs = 2000UL;
    closeDelayMs = 45000UL;
  } else if (distanceKm <= 5.0f) {
    openDelayMs = 1000UL;
    closeDelayMs = 20000UL;
  } else if (distanceKm <= 13.0f) {
    openDelayMs = 10000UL;
    closeDelayMs = 50000UL;
  } else if (distanceKm <= 23.0f) {
    openDelayMs = 30000UL;
    closeDelayMs = 90000UL;
  } else {
    openDelayMs = 60000UL;
    closeDelayMs = 150000UL;
  }
}

float thunderDistanceWeight(float distanceKm, bool hasValidDistance) {
  if (!hasValidDistance) return 0.7f;
  if (distanceKm <= 13.0f) return 1.0f;
  if (distanceKm <= 23.0f) return 0.6f;
  return 0.25f;
}

void openThunderWindowForCandidate(uint32_t now, float distanceKm, bool hasValidDistance) {
  uint32_t openDelayMs = 0;
  uint32_t closeDelayMs = 0;
  thunderWindowForDistance(distanceKm, hasValidDistance, openDelayMs, closeDelayMs);
  thunderWindowOpenMs = now + openDelayMs;
  appState.thunderWindowUntilMs = now + closeDelayMs;
  appState.lastLightningEventMs = now;
  appState.thunderCandidate = false;
  appState.thunderConfirmed = false;
  appState.thunderDelaySec = 0.0f;
  appState.audioDistanceKm = 0.0f;
  appState.thunderCandidateSinceMs = 0;
  thunderRumbleStartMs = 0;
  thunderImpulseStartMs = 0;
  thunderImpulseActive = false;
}

void createStrikeCandidate(float distanceKm, bool hasValidDistance) {
  uint32_t now = millis();
  pendingStrike.active = true;
  pendingStrike.accepted = false;
  pendingStrike.distanceKm = distanceKm;
  pendingStrike.hasValidDistance = hasValidDistance;
  pendingStrike.eventMs = now;
  pendingStrike.promptUntilMs = 0;
  pendingStrike.promptOpen = false;
  pendingStrike.closeWindow = hasValidDistance && distanceKm <= 10.0f;
  pendingStrike.thunderDetections = 0;
  pendingStrike.sharpImpulseCount = 0;
  pendingStrike.acousticConfidenceDelta = 0;
  openThunderWindowForCandidate(now, distanceKm, hasValidDistance);
  pendingStrike.score = scoreStrikeCandidate(distanceKm, hasValidDistance, now, 0, 0, 0);
  pendingStrike.decision = decisionForScore(pendingStrike.score);
  appState.strikeConfidenceScore = pendingStrike.score;
  appState.strikeConfidenceDecision = pendingStrike.decision;
  appState.strikePromptPending = false;

  Serial.printf("AS3935 lightning candidate: distance=%s score=%u decision=%s bias=%d\n",
                hasValidDistance ? String(distanceKm, 1).c_str() : "out-of-range",
                pendingStrike.score,
                confidenceDecisionName(pendingStrike.decision),
                appState.strikeSessionBias);

  if (pendingStrike.decision == STRIKE_DECISION_ACCEPT) {
    acceptPendingStrike("score");
  } else if (pendingStrike.decision == STRIKE_DECISION_PROMPT &&
             appState.strikeTrainingModeEnabled &&
             canOpenPrompt(now)) {
    pendingStrike.promptOpen = true;
    pendingStrike.promptUntilMs = now + STRIKE_PROMPT_TIMEOUT_MS;
    appState.strikePromptPending = true;
    rememberTimedEvent(promptEventMs, MAX_PROMPTS_PER_10_MIN, now);
    Serial.printf("Strike confidence PROMPT: Reply Y/N/U within %lus. Timeout defaults to no strike.\n",
                  STRIKE_PROMPT_TIMEOUT_MS / 1000UL);
  } else if (pendingStrike.decision == STRIKE_DECISION_SUPPRESS) {
    Serial.println("Strike confidence SUPPRESS: candidate retained only for thunder-window rescoring");
  } else {
    Serial.println("Strike confidence PROMPT suppressed: prompt rate limit reached");
  }
}

void addLightningStrike(float distanceKm, bool hasValidDistance) {
  uint32_t now = millis();
  lastValidLightningMs = now;
  stormActive = true;
  appState.lightningDataValid = hasValidDistance;
  if (hasValidDistance) {
    appState.lastKm = appState.distanceKm > 0.0f ? appState.distanceKm : distanceKm;
    appState.distanceKm = distanceKm;
    appState.nearestKm = appState.nearestKm <= 0.0f ? distanceKm : min(appState.nearestKm, distanceKm);
  } else {
    appState.distanceKm = 0.0f;
  }
  appState.lastStrikeMs = now;
  appState.lastLightningEventMs = now;
  if (appState.thunderWindowUntilMs == 0 || appState.thunderWindowUntilMs < now) {
    appState.thunderWindowUntilMs = now + THUNDER_WINDOW_MS;
  }
  appState.thunderCandidate = false;
  appState.thunderConfirmed = false;
  appState.thunderDelaySec = 0.0f;
  appState.audioDistanceKm = 0.0f;
  appState.thunderCandidateSinceMs = 0;

  if (hasValidDistance) {
    for (int8_t i = MAX_STRIKE_MARKERS - 1; i > 0; i--) {
      appState.strikes[i] = appState.strikes[i - 1];
    }
    appState.strikes[0].distanceKm = distanceKm;
    appState.strikes[0].minutesAgo = 0;
    appState.strikes[0].ageSeconds = 0;
    appState.strikes[0].colour = severityColourForDistance(distanceKm, true);
    appState.strikes[0].angleDeg = random(0, 360);
    appState.strikes[0].eventMs = now;
    if (appState.strikeCount < MAX_STRIKE_MARKERS) appState.strikeCount++;
  }

  for (int8_t i = MAX_RATE_EVENTS - 1; i > 0; i--) {
    appState.strikeEventMs[i] = appState.strikeEventMs[i - 1];
  }
  appState.strikeEventMs[0] = now;

  uint8_t recent = 0;
  for (uint8_t i = 0; i < MAX_RATE_EVENTS; i++) {
    if (appState.strikeEventMs[i] != 0 && now - appState.strikeEventMs[i] <= 60000UL) recent++;
  }
  appState.ratePerMin = recent;
  appState.sessionCount++;
  appState.peakRate = max(appState.peakRate, appState.ratePerMin);
  updateLedMetrics(appState.ratePerMin, appState.distanceKm, appState.lightningDataValid);
  triggerLightningFlash();

  if (hasValidDistance) {
    Serial.printf("AS3935 lightning: %.1f km, rate %u/m, count %u\n", distanceKm, appState.ratePerMin, appState.sessionCount);
  } else {
    Serial.printf("AS3935 lightning: out of range, rate %u/m, count %u\n", appState.ratePerMin, appState.sessionCount);
  }
}

void resetLiveStormState() {
  clearPendingStrike();
  appState.distanceKm = 0.0f;
  appState.nearestKm = 0.0f;
  appState.lastKm = 0.0f;
  appState.ratePerMin = 0;
  appState.peakRate = 0;
  appState.lightningDataValid = false;
  appState.lastStrikeMs = 0;
  appState.lastLightningEventMs = 0;
  appState.thunderWindowUntilMs = 0;
  appState.thunderCandidate = false;
  appState.thunderConfirmed = false;
  appState.thunderListening = false;
  appState.thunderDelaySec = 0.0f;
  appState.audioDistanceKm = 0.0f;
  appState.thunderCandidateSinceMs = 0;
  appState.thunderCooldownUntilMs = 0;
  appState.strikeSessionBias = 0;
  appState.strikeConfidenceScore = 0;
  appState.strikeConfidenceDecision = STRIKE_DECISION_SUPPRESS;
  appState.strikeCount = 0;
  for (uint8_t i = 0; i < MAX_RATE_EVENTS; i++) {
    appState.strikeEventMs[i] = 0;
  }
  for (uint8_t i = 0; i < MAX_STRIKE_MARKERS; i++) {
    appState.strikes[i].distanceKm = 0.0f;
    appState.strikes[i].minutesAgo = 0;
    appState.strikes[i].ageSeconds = 0;
    appState.strikes[i].colour = severityColourForDistance(0.0f, false);
    appState.strikes[i].angleDeg = random(0, 360);
    appState.strikes[i].eventMs = 0;
  }
  stormActive = false;
  updateLedMetrics(appState.ratePerMin, appState.distanceKm, appState.lightningDataValid);
  Serial.println("Storm inactivity reset: live storm state cleared");
}

void updateStormInactivityReset() {
  if (stormActive && (uint32_t)(millis() - lastValidLightningMs) >= STORM_INACTIVITY_RESET_MS) {
    resetLiveStormState();
  }
}

void updateLightningSensor() {
  if (!appState.lightningPresent) return;

  uint32_t now = millis();
  if (!lightningIrqPending && digitalRead(PIN_SENSOR_IRQ) == HIGH) {
    lightningIrqPending = true;
    lightningIrqSeenMs = now;
  }
  if (!lightningIrqPending || now - lightningIrqSeenMs < 2) return;
  lightningIrqPending = false;

  uint8_t interruptReg = 0;
  if (!as3935Read(AS3935_REG_INT, interruptReg)) return;
  uint8_t reason = interruptReg & 0x0F;
  appState.lightningLastInterrupt = reason;

  if (reason == AS3935_INT_LIGHTNING) {
    uint8_t distanceReg = 0;
    if (!as3935Read(AS3935_REG_DISTANCE, distanceReg)) return;
    uint8_t rawDistance = distanceReg & 0x3F;
    bool hasValidDistance = isValidAs3935DistanceBucket(rawDistance);
    float distanceKm = hasValidDistance ? (float)rawDistance : 0.0f;
    createStrikeCandidate(distanceKm, hasValidDistance);
  } else if (reason == AS3935_INT_NOISE) {
    rememberTimedEvent(noiseEventMs, MAX_NOISE_EVENTS, now);
    Serial.printf("AS3935 noise interrupt logged: 10min=%u\n",
                  countTimedEvents(noiseEventMs, MAX_NOISE_EVENTS, now, 10UL * 60UL * 1000UL));
  } else if (reason == AS3935_INT_DISTURBER) {
    rememberTimedEvent(disturberEventMs, MAX_DISTURBER_EVENTS, now);
    Serial.printf("AS3935 disturber interrupt logged: 10min=%u\n",
                  countTimedEvents(disturberEventMs, MAX_DISTURBER_EVENTS, now, 10UL * 60UL * 1000UL));
  }
}

void updateBme280() {
  if (!bme280OK) return;
  if (millis() - appState.lastBmeReadMs < BME280_READ_INTERVAL_MS) return;
  readBme280Now();
}

void updateStrikePrompt() {
  if (!pendingStrike.active) return;
  uint32_t now = millis();

  if (pendingStrike.accepted) {
    if (now > appState.thunderWindowUntilMs) {
      clearPendingStrike();
    }
    return;
  }

  if (pendingStrike.promptOpen && (int32_t)(now - pendingStrike.promptUntilMs) >= 0) {
    applySessionBiasDelta(-4);
    suppressPendingStrike("prompt-timeout");
    Serial.println("Strike confidence learning: TIMEOUT -> session bias -4");
    return;
  }

  if (!pendingStrike.promptOpen &&
      now > appState.thunderWindowUntilMs &&
      pendingStrike.decision != STRIKE_DECISION_ACCEPT) {
    suppressPendingStrike("window-expired");
  }
}

void handleStrikeTrainingSerial() {
  while (Serial.available() > 0) {
    char c = Serial.read();
    if (c == '\r' || c == '\n' || c == ' ') continue;
    c = (char)toupper(c);

    if (c == 'R') {
      resetTrainingBias();
      continue;
    }

    if (!pendingStrike.active || !pendingStrike.promptOpen) {
      continue;
    }

    if (c == 'Y') {
      applySessionBiasDelta(5);
      pendingStrike.score = pendingStrike.score < STRIKE_ACCEPT_SCORE ? STRIKE_ACCEPT_SCORE : pendingStrike.score;
      acceptPendingStrike("user-yes");
      Serial.println("Strike confidence learning: YES -> session bias +5");
    } else if (c == 'N') {
      applySessionBiasDelta(-8);
      suppressPendingStrike("user-no");
      Serial.println("Strike confidence learning: NO -> session bias -8");
    } else if (c == 'U') {
      suppressPendingStrike("user-unsure");
      Serial.println("Strike confidence learning: UNSURE -> no bias change");
    }
  }
}

float lpAlpha(float cutoffHz) {
  const float dt = 1.0f / MIC_DSP_SAMPLE_RATE_HZ;
  const float rc = 1.0f / (2.0f * PI * cutoffHz);
  return dt / (rc + dt);
}

float hpAlpha(float cutoffHz) {
  const float dt = 1.0f / MIC_DSP_SAMPLE_RATE_HZ;
  const float rc = 1.0f / (2.0f * PI * cutoffHz);
  return rc / (rc + dt);
}

MicBandFrame sampleMicrophoneBands(uint16_t rawSample) {
  static float lowHpY = 0.0f;
  static float lowHpPrevX = 0.0f;
  static float lowLpY = 0.0f;
  static float highHpY = 0.0f;
  static float highHpPrevX = 0.0f;

  const uint32_t samplePeriodUs = 1000000UL / MIC_DSP_SAMPLE_RATE_HZ;
  const float lowHpA = hpAlpha(40.0f);
  const float lowLpA = lpAlpha(180.0f);
  const float highHpA = hpAlpha(300.0f);
  float lowEnergy = 0.0f;
  float highEnergy = 0.0f;
  float peakDelta = 0.0f;
  uint32_t nextSampleUs = micros();

  for (uint8_t i = 0; i < MIC_DSP_SAMPLE_COUNT; i++) {
    uint16_t raw = i == 0 ? rawSample : constrain(analogRead(PIN_MIC_ADC), 0, MIC_ADC_MAX);
    if (i == MIC_DSP_SAMPLE_COUNT - 1) appState.micRaw = raw;

    float x = (float)raw - appState.micBaseline;
    peakDelta = max(peakDelta, fabsf(x));

    lowHpY = lowHpA * (lowHpY + x - lowHpPrevX);
    lowHpPrevX = x;
    lowLpY += lowLpA * (lowHpY - lowLpY);

    highHpY = highHpA * (highHpY + x - highHpPrevX);
    highHpPrevX = x;

    lowEnergy += lowLpY * lowLpY;
    highEnergy += highHpY * highHpY;

    nextSampleUs += samplePeriodUs;
    while ((int32_t)(micros() - nextSampleUs) < 0) {
      delayMicroseconds(20);
    }
  }

  MicBandFrame frame;
  frame.lowEnergy = lowEnergy / MIC_DSP_SAMPLE_COUNT;
  frame.highEnergy = highEnergy / MIC_DSP_SAMPLE_COUNT;
  frame.lowHighRatio = frame.lowEnergy / max(frame.highEnergy, 1.0f);
  frame.peakDelta = peakDelta;
  return frame;
}

void applyAcousticDelta(int8_t delta, const char *source) {
  if (!pendingStrike.active) return;
  if (delta > pendingStrike.acousticConfidenceDelta) {
    pendingStrike.acousticConfidenceDelta = delta;
    rescorePendingStrike(source);
  } else if (delta < 0 && pendingStrike.acousticConfidenceDelta <= 0 && delta < pendingStrike.acousticConfidenceDelta) {
    pendingStrike.acousticConfidenceDelta = delta;
    rescorePendingStrike(source);
  }
}

void updateMicrophone() {
  uint32_t now = millis();
  if (now - lastMicSampleMs < MIC_SAMPLE_INTERVAL_MS) return;
  lastMicSampleMs = now;

  uint16_t raw = constrain(analogRead(PIN_MIC_ADC), 0, MIC_ADC_MAX);
  appState.micRaw = raw;
  appState.thunderListening = pendingStrike.active && now < appState.thunderWindowUntilMs;

  if (appState.micBaseline <= 0.0f) {
    appState.micBaseline = raw;
  }

  MicBandFrame frame = sampleMicrophoneBands(raw);
  appState.micDelta = frame.peakDelta;

  if (!appState.thunderListening) {
    appState.micBaseline = appState.micBaseline * (1.0f - MIC_BASELINE_ALPHA_IDLE) + appState.micRaw * MIC_BASELINE_ALPHA_IDLE;
    thunderLowBaseline = thunderLowBaseline * (1.0f - THUNDER_LOW_BASELINE_ALPHA) + frame.lowEnergy * THUNDER_LOW_BASELINE_ALPHA;
    thunderRumbleStartMs = 0;
    thunderImpulseStartMs = 0;
    thunderImpulseActive = false;
    appState.thunderCandidate = false;
    appState.thunderCandidateSinceMs = 0;
    if (now - lastMicSerialMs >= MIC_SERIAL_INTERVAL_MS) {
      lastMicSerialMs = now;
      Serial.printf("MIC GPIO%d raw=%4u baseline=%6.1f delta=%5.1f low=%.0f high=%.0f ratio=%.2f state=idle\n",
                    PIN_MIC_ADC,
                    appState.micRaw,
                    appState.micBaseline,
                    appState.micDelta,
                    frame.lowEnergy,
                    frame.highEnergy,
                    frame.lowHighRatio);
    }
    return;
  }

  if (now < thunderWindowOpenMs) {
    appState.micBaseline = appState.micBaseline * (1.0f - MIC_BASELINE_ALPHA_LISTENING) + appState.micRaw * MIC_BASELINE_ALPHA_LISTENING;
    thunderLowBaseline = thunderLowBaseline * (1.0f - THUNDER_LOW_BASELINE_ALPHA) + frame.lowEnergy * THUNDER_LOW_BASELINE_ALPHA;
    bool preWindowImpulse = frame.highEnergy > max(thunderLowBaseline * 5.0f, frame.lowEnergy * 1.5f);
    if (preWindowImpulse && pendingStrike.active && pendingStrike.sharpImpulseCount == 0) {
      pendingStrike.sharpImpulseCount = 1;
      applyAcousticDelta(-10, "pre-window-impulse");
    }
    return;
  }

  float delaySinceLightning = (now - appState.lastLightningEventMs) / 1000.0f;
  float baselineLow = max(thunderLowBaseline, 1.0f);
  bool lowEnergyValid = frame.lowEnergy > baselineLow * THUNDER_LOW_ENERGY_MULTIPLIER;
  bool lowDominant = frame.lowHighRatio > THUNDER_RATIO_THRESHOLD;
  bool highDominant = frame.highEnergy > max(baselineLow * 4.0f, frame.lowEnergy * 1.4f);
  bool rumbleLike = lowEnergyValid && lowDominant;

  if (now >= appState.thunderCooldownUntilMs && rumbleLike) {
    if (thunderRumbleStartMs == 0) {
      thunderRumbleStartMs = now;
      appState.thunderCandidate = true;
      appState.thunderCandidateSinceMs = now;
      Serial.printf("Thunder rumble candidate: low=%.0f high=%.0f ratio=%.2f delay=%.1fs\n",
                    frame.lowEnergy, frame.highEnergy, frame.lowHighRatio, delaySinceLightning);
    }
    uint32_t rumbleMs = now - thunderRumbleStartMs;
    if (rumbleMs >= THUNDER_MIN_RUMBLE_MS) {
      float distanceWeight = thunderDistanceWeight(pendingStrike.distanceKm, pendingStrike.hasValidDistance);
      bool strong = rumbleMs >= THUNDER_STRONG_RUMBLE_MS &&
                    frame.lowEnergy > baselineLow * THUNDER_LOW_ENERGY_STRONG_MULTIPLIER &&
                    frame.lowHighRatio > THUNDER_RATIO_STRONG_THRESHOLD;
      int8_t acousticDelta = strong ? 15 : (frame.lowEnergy > baselineLow * 4.0f ? 10 : 5);
      acousticDelta = (int8_t)roundf(acousticDelta * distanceWeight);
      if (acousticDelta < 1) acousticDelta = 1;
      appState.thunderConfirmed = true;
      appState.thunderDelaySec = delaySinceLightning;
      appState.audioDistanceKm = appState.thunderDelaySec / 3.0f;
      appState.thunderCooldownUntilMs = now + THUNDER_COOLDOWN_MS;
      appState.thunderCandidate = false;
      appState.thunderCandidateSinceMs = 0;
      if (pendingStrike.active) {
        pendingStrike.thunderDetections = min((int)pendingStrike.thunderDetections + 1, 255);
        applyAcousticDelta(acousticDelta, strong ? "strong-thunder-rumble" : "thunder-rumble");
      }
      Serial.printf("Thunder confirmed: delay=%.1fs audio distance=%.1f km acoustic=%+d low=%.0f high=%.0f ratio=%.2f\n",
                    appState.thunderDelaySec, appState.audioDistanceKm, acousticDelta,
                    frame.lowEnergy, frame.highEnergy, frame.lowHighRatio);
    }
  } else {
    if (thunderRumbleStartMs != 0 && now - thunderRumbleStartMs < THUNDER_IMPULSE_REJECT_MS) {
      pendingStrike.sharpImpulseCount = min((int)pendingStrike.sharpImpulseCount + 1, 255);
      applyAcousticDelta(-10, "short-acoustic-impulse");
    } else if (highDominant && now >= appState.thunderCooldownUntilMs) {
      if (!thunderImpulseActive) {
        thunderImpulseActive = true;
        thunderImpulseStartMs = now;
      } else if (now - thunderImpulseStartMs >= THUNDER_IMPULSE_REJECT_MS) {
        pendingStrike.sharpImpulseCount = min((int)pendingStrike.sharpImpulseCount + 1, 255);
        applyAcousticDelta(-5, "high-frequency-noise");
        thunderImpulseActive = false;
      }
    } else {
      thunderImpulseActive = false;
    }
    if (!appState.thunderConfirmed) {
      thunderLowBaseline = thunderLowBaseline * (1.0f - THUNDER_LOW_BASELINE_ALPHA) + frame.lowEnergy * THUNDER_LOW_BASELINE_ALPHA;
    }
    thunderRumbleStartMs = 0;
    appState.thunderCandidate = false;
    appState.thunderCandidateSinceMs = 0;
  }

  if (now - lastMicSerialMs >= MIC_SERIAL_INTERVAL_MS) {
    lastMicSerialMs = now;
    const char *state = appState.thunderConfirmed ? "confirmed" : (appState.thunderCandidate ? "candidate" : "listening");
    Serial.printf("MIC GPIO%d raw=%4u baseline=%6.1f delta=%5.1f low=%.0f high=%.0f ratio=%.2f acoustic=%+d state=%s\n",
                  PIN_MIC_ADC,
                  appState.micRaw,
                  appState.micBaseline,
                  appState.micDelta,
                  frame.lowEnergy,
                  frame.highEnergy,
                  frame.lowHighRatio,
                  pendingStrike.active ? pendingStrike.acousticConfidenceDelta : 0,
                  state);
  }
}

void updateDeviceMetrics() {
  appState.freeHeap = ESP.getFreeHeap();
  appState.totalHeap = ESP.getHeapSize();
  appState.heapUsedPercent = appState.totalHeap > 0
                                ? constrain((int)(100 - ((uint64_t)appState.freeHeap * 100 / appState.totalHeap)), 0, 100)
                                : 0;
  appState.freePsram = ESP.getFreePsram();
  appState.totalPsram = ESP.getPsramSize();
  appState.psramUsedPercent = appState.totalPsram > 0
                                 ? constrain((int)(100 - ((uint64_t)appState.freePsram * 100 / appState.totalPsram)), 0, 100)
                                 : 0;
  appState.cpuFreqMHz = ESP.getCpuFreqMHz();
}

void applyDeviceMode(DeviceMode mode, bool persist) {
  modeManager.setMode(appState, mode, persist);
  as3935ProfileManager.applyForMode(appState, mode);

  if (mode == MODE_HOME) {
    bleService.stop(appState);
    webApiServer.stop(appState);
    wifiManager.applyMode(appState, mode);
  } else {
    captivePortal.stop(appState);
    webApiServer.stop(appState);
    wifiManager.applyMode(appState, mode);
    bleService.start(appState);
  }

  Serial.printf("Mode applied: %s, AS3935 profile %s\n", modeName(appState.mode), appState.as3935ProfileText);
}

void updateNetworkServices() {
  wifiManager.update(appState);

  char portalSsid[33];
  char portalPassword[65];
  if (captivePortal.takeNewCredentials(portalSsid, sizeof(portalSsid), portalPassword, sizeof(portalPassword))) {
    wifiManager.noteProvisionedCredentials(appState, portalSsid, portalPassword);
  }

  if (appState.mode == MODE_HOME) {
    if (wifiManager.shouldProvision(appState)) {
      webApiServer.stop(appState);
      captivePortal.start(appState);
    } else if (appState.wifiConnected) {
      captivePortal.stop(appState);
      webApiServer.start(appState);
    } else {
      webApiServer.stop(appState);
    }
  } else {
    captivePortal.stop(appState);
    webApiServer.stop(appState);
  }

  captivePortal.update(appState);
  webApiServer.update(appState);
  bleService.update(appState);
}

void resetHomeWifiCredentials() {
  wifiManager.clearSavedCredentials(appState);
  if (appState.mode == MODE_HOME) {
    webApiServer.stop(appState);
    captivePortal.start(appState);
  } else {
    strncpy(appState.captivePortalProgress, "home wifi reset", sizeof(appState.captivePortalProgress));
  }
  Serial.println("Saved HOME Wi-Fi credentials cleared from NETWORK page");
}

void toggleHomeRoamingMode() {
  DeviceMode nextMode = appState.mode == MODE_HOME ? MODE_ROAMING : MODE_HOME;
  applyDeviceMode(nextMode, true);
}

void adjustDisplayBrightness(int8_t delta) {
  uint8_t next = constrain((int)appState.displayBrightnessLevel + delta, 1, 10);
  if (next == appState.displayBrightnessLevel) return;
  appState.displayBrightnessLevel = next;
  applyDisplayBrightness();
  Serial.printf("Display brightness set to %u/10 (%u%%)\n", appState.displayBrightnessLevel, appState.displayBrightnessLevel * 10);
}

uint16_t uiXFromTouch(uint16_t x) {
  return gfx->width() - 1 - x;
}

uint16_t uiYFromTouch(uint16_t y) {
  return gfx->height() - 1 - y;
}

void nextPage() {
  currentPage = (AppPage)((currentPage + 1) % PAGE_COUNT);
  resetConfirmVisible = false;
  renderUi(appState, currentPage, currentTrendTab, resetConfirmVisible, true);
}

void prevPage() {
  currentPage = (AppPage)((currentPage + PAGE_COUNT - 1) % PAGE_COUNT);
  resetConfirmVisible = false;
  renderUi(appState, currentPage, currentTrendTab, resetConfirmVisible, true);
}

void handleTap(uint16_t x, uint16_t y) {
  const uint16_t w = gfx->width();

  if (resetConfirmVisible) {
    uint16_t uiX = uiXFromTouch(x);
    uint16_t uiY = uiYFromTouch(y);
    Serial.printf("NETWORK reset confirm tap rawX=%u uiX=%u rawY=%u uiY=%u\n", x, uiX, y, uiY);
    if (pointInRect(uiX, uiY, 336, 252, 156, 50)) {
      resetHomeWifiCredentials();
      Serial.println("NETWORK reset WiFi confirmed");
    } else if (pointInRect(uiX, uiY, 108, 252, 156, 50)) {
      Serial.println("NETWORK reset WiFi confirmation dismissed");
    } else {
      Serial.println("NETWORK reset WiFi confirmation dismissed by outside tap");
    }
    resetConfirmVisible = false;
    renderUi(appState, currentPage, currentTrendTab, resetConfirmVisible, true);
    return;
  }

  if (currentPage == PAGE_TRENDS) {
    uint16_t tabX = w - 1 - x;
    uint8_t tabIndex;
    if (tabX < 170) {
      tabIndex = TAB_RATE;
    } else if (tabX < 315) {
      tabIndex = TAB_TEMP;
    } else if (tabX < 460) {
      tabIndex = TAB_HUM;
    } else {
      tabIndex = TAB_PRESS;
    }
    currentTrendTab = (TrendTab)tabIndex;
    Serial.printf("TRENDS tab tap x=%u mappedX=%u y=%u -> %s\n", x, tabX, y, TAB_NAMES[tabIndex]);
    renderUi(appState, currentPage, currentTrendTab, resetConfirmVisible, true);
    return;
  }

  if (currentPage == PAGE_LIVE) {
    if (pointInRect(x, y, 24, 76, 328, 166)) {
      currentPage = PAGE_RADAR;
    } else if (pointInRect(x, y, 364, 76, 212, 77)) {
      currentPage = PAGE_TRENDS;
      currentTrendTab = TAB_RATE;
    } else if (pointInRect(x, y, 24, 266, 552, 76)) {
      currentPage = PAGE_TRENDS;
    }
  } else if (currentPage == PAGE_ATMOS) {
    if (pointInRect(x, y, 24, 76, 552, 84)) {
      currentPage = PAGE_TRENDS;
      currentTrendTab = TAB_PRESS;
    } else if (pointInRect(x, y, 24, 172, 270, 96)) {
      currentPage = PAGE_TRENDS;
      currentTrendTab = TAB_TEMP;
    } else if (pointInRect(x, y, 306, 172, 270, 96)) {
      currentPage = PAGE_TRENDS;
      currentTrendTab = TAB_HUM;
    }
  } else if (currentPage == PAGE_DISPLAY) {
    uint16_t uiX = uiXFromTouch(x);
    uint16_t uiY = uiYFromTouch(y);
    bool brightnessMinus = pointInRect(x, y, 362, 94, 54, 48) || pointInRect(uiX, uiY, 362, 94, 54, 48);
    bool brightnessPlus = pointInRect(x, y, 438, 94, 54, 48) || pointInRect(uiX, uiY, 438, 94, 54, 48);
    if (brightnessMinus) {
      adjustDisplayBrightness(-1);
    } else if (brightnessPlus) {
      adjustDisplayBrightness(1);
    }
  } else if (currentPage == PAGE_NETWORK) {
    uint16_t uiX = uiXFromTouch(x);
    uint16_t uiY = uiYFromTouch(y);
    Serial.printf("NETWORK tap rawX=%u uiX=%u rawY=%u uiY=%u\n", x, uiX, y, uiY);
    if (pointInRect(uiX, uiY, 24, 328, 252, 54)) {
      resetConfirmVisible = true;
      Serial.println("NETWORK reset WiFi confirmation shown");
    } else if (pointInRect(uiX, uiY, 324, 328, 252, 54)) {
      toggleHomeRoamingMode();
    } else {
      Serial.println("NETWORK tap missed controls");
    }
  }

  renderUi(appState, currentPage, currentTrendTab, resetConfirmVisible, true);
}

void handleTouch() {
  if (!touchOK || !displayOK || (millis() - lastGestureMs) < GESTURE_COOLDOWN_MS) return;

  uint16_t x = 0;
  uint16_t y = 0;
  bool touched = readTouchLandscape(x, y);

  if (touched) {
    if (!touchDown) {
      touchDown = true;
      touchStartX = x;
      touchStartY = y;
      touchStartMs = millis();
    }
    touchLastX = x;
    touchLastY = y;
    return;
  }

  if (!touchDown) return;

  int16_t dx = (int16_t)touchLastX - (int16_t)touchStartX;
  int16_t dy = (int16_t)touchLastY - (int16_t)touchStartY;
  uint32_t holdMs = millis() - touchStartMs;
  touchDown = false;

  if (holdMs < TOUCH_MIN_HOLD_MS) return;

  if (abs(dx) >= SWIPE_MIN_DELTA_PX && abs(dy) <= SWIPE_MAX_OFF_AXIS_PX && abs(dx) > abs(dy)) {
    if (dx > 0) {
      nextPage();
    } else {
      prevPage();
    }
    lastGestureMs = millis();
    return;
  }

  if (abs(dx) <= TAP_MAX_MOVE_PX && abs(dy) <= TAP_MAX_MOVE_PX) {
    handleTap(touchStartX, touchStartY);
    lastGestureMs = millis();
  }
}

void setup() {
  Serial.begin(115200);
  uint32_t serialWaitStart = millis();
  while (!Serial && (millis() - serialWaitStart) < 250) {
    delay(10);
  }

  Serial.println();
  initAppState(appState);
  appState.strikeTrainingModeEnabled = TRAINING_MODE_ENABLED;
  Serial.print("Lightning_Detector_v1_0 boot [text boot screen] ");
  Serial.println(FW_BUILD_LABEL);
  Serial.println("Touchscreen HOME/ROAMING mode, Wi-Fi provisioning, BLE, web API, GPS, AS3935 profiles, and GPIO40 addressable LEDs enabled.");

  initLeds();
  setBootLedsDeepBlue();
  setupDisplay();
  showBootScreen();
  uint32_t bootScreenStartMs = millis();
  while (millis() - bootScreenStartMs < BOOT_MIN_HOLD_MS) {
    updateLeds();
    delay(10);
  }

  modeManager.begin(appState);
  as3935ProfileManager.begin(appState, as3935Read, as3935Write);
  wifiManager.begin(appState);
  captivePortal.begin(appState);
  webApiServer.begin(appState);
  bleService.begin(appState);
  updateLedMetrics(appState.ratePerMin, appState.distanceKm, appState.lightningDataValid);
  gpsManager.begin(appState);
  setupMicrophone();
  beginBatteryStatus(appState);
  setupTouch();
  setupBme280();
  setupLightningSensor();
  applyDeviceMode(appState.mode, false);
  logI2CScan("after sensor setup");
  logFullI2CScan("after sensor setup");
  updateBatteryStatus(appState);
  updateDeviceMetrics();

  releaseBootLeds();
  updateLedMetrics(appState.ratePerMin, appState.distanceKm, appState.lightningDataValid);
  updateLeds();

  if (displayOK) {
    currentPage = PAGE_LIVE;
    renderUi(appState, currentPage, currentTrendTab, resetConfirmVisible, true);
  }
}

void loop() {
  gpsManager.update(appState);
  updateLightningSensor();
  handleStrikeTrainingSerial();
  updateStrikePrompt();
  updateStormInactivityReset();
  updateDerivedState(appState);
  updateLedMetrics(appState.ratePerMin, appState.distanceKm, appState.lightningDataValid);
  updateLeds();
  updateBatteryStatus(appState);
  updateMicrophone();
  updateBme280();
  updateDeviceMetrics();
  updateNetworkServices();
  handleTouch();
  if (displayOK) {
    renderUi(appState, currentPage, currentTrendTab, resetConfirmVisible, false);
  }
  yield();
}
