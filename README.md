# Lightning Detector v1.0

Firmware version: `v1.0`

This is a meant to be a fun project and not designed for actual scientific use or storm monitoring.

`Lightning_Detector_v1_0` is an ESP32-S3 Arduino firmware for a portable storm sensor suite built around a Waveshare ESP32-S3 Touch AMOLED 2.41 display. It combines lightning detection, environmental sensing, microphone-based thunder correlation, GPS status, addressable LEDs, BLE telemetry, optional Wi-Fi/Home mode, captive Wi-Fi provisioning, and a small HTTP API.

AS3935 lightning interrupts are treated as strike candidates, then scored using recent storm context, BME280 environmental trends, AS3935 noise/disturber interrupts, optional microphone thunder confirmation, and serial training feedback before being accepted into the live storm state.


## Features

| Feature | Description |
| --- | --- |
| Lightning detection | AS3935 I2C detection with HOME/ROAMING gain profiles, distance buckets, interrupt classification, and accepted-strike history. |
| Strike confidence scoring | Candidate strikes are accepted, prompted, or suppressed using storm context, environmental trends, noise/disturber history, thunder correlation, and session learning. |
| Environmental sensing | Direct BME280 register driver for temperature, humidity, pressure, and long-window trend data. |
| Thunder correlation | Analog microphone sampling checks for delayed thunder after a candidate strike and feeds the confidence score. |
| Touch dashboard | AMOLED UI with LIVE, RADAR, STORM, TRENDS, ATMOS, DISPLAY, NETWORK, and SYSTEM pages. |
| Field and home modes | ROAMING mode uses BLE and an outdoor AS3935 profile; HOME mode uses Wi-Fi, captive setup, HTTP API, and an indoor AS3935 profile. |
| Telemetry | BLE GATT characteristics and HTTP JSON endpoints expose lightning, environment, GPS, battery, and system status. |
| GPS and battery status | UART NMEA parser, fix/status display, ADC battery estimate, low/critical flags, and system metrics. |
| Status LEDs | Two NeoPixels show strike-rate and range severity with boot and strike flash behavior. |

## Project Layout

| File | Responsibility |
| --- | --- |
| `Lightning_Detector_v1_0.ino` | Main firmware orchestration, hardware pin definitions, I2C helpers, BME280 and AS3935 drivers, confidence scoring, microphone thunder logic, touch handling, setup, and main loop. |
| `app_state.h` | Shared application state model, pages, modes, trend tabs, strike markers, and confidence decision enums. |
| `app_helpers.cpp/.h` | State initialization, rolling strike rate, trend history, strike marker aging, labels, and range trend helper. |
| `ui.cpp/.h` | AMOLED dashboard rendering, page layouts, touch target helpers, trend graph drawing, and display controls. |
| `colours.h` | RGB565 UI palette and semantic severity color mapping for rate and distance. |
| `mode_manager.cpp/.h` | HOME/ROAMING mode persistence using ESP32 `Preferences`. |
| `as3935_profile_manager.cpp/.h` | Applies AS3935 indoor/outdoor AFE gain profiles based on HOME/ROAMING mode. |
| `wifi_manager.cpp/.h` | Saved Home Wi-Fi credentials, station connection, retry behavior, and provisioning decisions. |
| `captive_portal_handler.cpp/.h` | Setup AP, DNS captive portal, and HTML form for saving Wi-Fi credentials. |
| `web_api_server.cpp/.h` | HTTP JSON API for status, lightning, environment, and GPS data. |
| `ble_service.cpp/.h` | BLE GATT service for telemetry notifications in ROAMING mode. |
| `gps_manager.cpp/.h` | UART NMEA parsing for GGA/RMC GPS sentences with checksum validation. |
| `battery_status.cpp/.h` | Battery ADC sampling, smoothing, voltage-to-percent estimate, and status flags. |
| `led_manager.cpp/.h` | Two NeoPixel status LEDs for strike rate and range severity, including boot and strike flash behavior. |

## Target Hardware

The code is written for an ESP32-S3 board with a Waveshare 2.41 inch AMOLED panel and FT6336 touch controller.

### Display and Touch

| Function | Pin / Bus | Notes |
| --- | --- | --- |
| AMOLED CS | GPIO9 | QSPI display bus. |
| AMOLED SCK | GPIO10 | QSPI display bus. |
| AMOLED D0 | GPIO11 | QSPI display bus. |
| AMOLED D1 | GPIO12 | QSPI display bus. |
| AMOLED D2 | GPIO13 | QSPI display bus. |
| AMOLED D3 | GPIO14 | QSPI display bus. |
| AMOLED RST | GPIO21 | Display reset. |
| I2C SDA | GPIO47 | Shared TCA9554, FT6336, BME280, AS3935 bus. |
| I2C SCL | GPIO48 | Shared I2C bus, 100 kHz. |
| TCA9554 | I2C `0x20` | Used to enable AMOLED power through EXIO1. |
| FT6336 touch | I2C `0x38` | Touch controller. |
| Touch reset | GPIO3 | Reset line for FT6336. |
| BAT_PWR | GPIO16 | Enabled before display setup. |

The display object is `Arduino_RM690B0` through `Arduino_ESP32QSPI`, wrapped in a 450 x 600 `Arduino_Canvas`. The UI rotates the display to landscape with `gfx->setRotation(1)`.

### Sensors and Peripherals

| Function | Pin / Address | Notes |
| --- | --- | --- |
| AS3935 IRQ | GPIO39 | Polled as a digital interrupt source. |
| AS3935 I2C | `0x03`, `0x02`, or `0x01` | Probed in that order. |
| BME280 I2C | `0x76` or `0x77` | Probed by chip ID `0x60`. |
| Microphone ADC | GPIO1 | Analog mic input. GPIO4 is intentionally avoided because it is reserved for onboard SD clock on this board. |
| Battery ADC | GPIO17 | Defined in `battery_status.h`; assumes an approximate 200K/100K divider. |
| GPS RX | GPIO44 | UART1, 9600 baud. |
| GPS TX | GPIO43 | UART1, 9600 baud. |
| NeoPixel data | GPIO40 | Two addressable LEDs. |

## Required Arduino Libraries

The firmware uses:

| Library / Core | Used For |
| --- | --- |
| `esp32:esp32` Arduino core | ESP32-S3 platform, Wi-Fi, BLE, Preferences, WebServer, DNSServer, ADC, UART, heap metrics. |
| `GFX Library for Arduino` | AMOLED QSPI display driver and canvas. |
| `Adafruit NeoPixel` | GPIO40 addressable LEDs. |

The code directly implements BME280 register access and NMEA parsing, so the Adafruit BME280 and GPS libraries are not required by this sketch even if installed locally.

## Required Build Configuration

Use the ESP32S3 Dev Module target with the `huge_app` partition scheme. This is required for this firmware: the compiled sketch is larger than the default 1.2 MB application slot, and using the default partition scheme can cause upload or boot failures.

For a known-good flash, keep USB CDC On Boot disabled, use Hardware CDC and JTAG USB mode, enable OPI PSRAM, and use the Huge APP partition. Do not switch to the default partition scheme for this sketch.

Known-good Arduino CLI FQBN:

```sh
arduino-cli compile --fqbn esp32:esp32:esp32s3:PartitionScheme=huge_app Lightning_Detector_v1_0
```

The important board options are:

| Option | Required / Known-good Value | Why |
| --- | --- | --- |
| Board | `esp32:esp32:esp32s3` / ESP32S3 Dev Module | Matches the Waveshare ESP32-S3 AMOLED board target used by this sketch. |
| Partition Scheme | `Huge APP (3MB No OTA/1MB SPIFFS)` / `PartitionScheme=huge_app` | Required. The default 1.2 MB app partition is too small for this firmware. |
| Flash Size | `4MB (32Mb)` / `FlashSize=4M` | Known-good default for the target board package. |
| Flash Mode | `QIO 80MHz` / `FlashMode=qio` | Known-good default for this board package. |
| CPU Frequency | `240MHz (WiFi)` / `CPUFreq=240` | Required for Wi-Fi and BLE performance. |
| USB Mode | `Hardware CDC and JTAG` / `USBMode=hwcdc` | Known-good upload/debug mode. |
| Upload Mode | `UART0 / Hardware CDC` / `UploadMode=default` | Known-good upload path. |
| USB CDC On Boot | `Disabled` / `CDCOnBoot=default` | Known-good default; serial monitor still uses `115200` after firmware starts. |
| PSRAM | `OPI PSRAM` / `PSRAM=opi` | Required. The 450 x 600 16-bit canvas needs PSRAM; without it, display initialization can fail and the panel can stay black. |
| Erase Flash Before Upload | `Disabled` / `EraseFlash=none` | Keeps saved HOME/ROAMING mode and Wi-Fi preferences unless a full reset is intended. |

Fully explicit CLI compile example:

```sh
arduino-cli compile --fqbn esp32:esp32:esp32s3:USBMode=hwcdc,CDCOnBoot=default,UploadMode=default,CPUFreq=240,FlashMode=qio,FlashSize=4M,PartitionScheme=huge_app,DebugLevel=none,PSRAM=opi,LoopCore=1,EventsCore=1,EraseFlash=none Lightning_Detector_v1_0
```

## Operating Modes

The device has two persisted operating modes.

### ROAMING

ROAMING is the default startup mode. It turns Wi-Fi off, starts BLE advertising, and applies the AS3935 roaming/outdoor profile.

In this mode:

| Service | State |
| --- | --- |
| Wi-Fi station | Off |
| Captive portal | Off |
| HTTP API | Off |
| BLE | Advertising / connected |
| AS3935 profile | `ROAMING`, outdoor AFE gain |

### HOME

HOME mode uses Wi-Fi and the local HTTP API. BLE is stopped, saved Wi-Fi credentials are used if present, and the AS3935 home/indoor profile is applied.

If credentials are missing or repeated connection failures reach the configured limit, the firmware starts a captive setup AP named:

```text
Lightning-Setup-xxxxxx
```

The portal exposes a simple form at `/` for saving SSID and password. Saved credentials are stored in ESP32 `Preferences` under namespace `ld-home-wifi`.

## Startup Flow

The `setup()` sequence is:

1. Start serial at `115200`.
2. Initialize `AppState`, training mode, and boot log messages.
3. Start LEDs and set both boot LEDs deep blue.
4. Enable display power through TCA9554, initialize AMOLED canvas, and show a text boot screen for at least 3 seconds.
5. Initialize mode, AS3935 profile manager, Wi-Fi, captive portal, HTTP API, BLE, GPS, microphone, and battery monitor.
6. Initialize touch, BME280, and AS3935.
7. Apply current mode, scan I2C, update first battery/device metrics, release boot LEDs, and render the LIVE page.

The I2C startup includes bus recovery, short timeouts, scans, and fault detection. If SDA/SCL are held low or an impossible number of devices are seen, the code marks the bus faulted and skips touch, BME280, and AS3935 initialization.

## Main Loop

The `loop()` performs non-blocking updates in this order:

1. GPS UART parsing.
2. AS3935 IRQ polling and event handling.
3. Serial training input handling.
4. Pending strike prompt timeout or window expiry.
5. Storm inactivity reset.
6. Derived state updates and strike marker aging.
7. LED severity and flash updates.
8. Battery sampling.
9. Microphone thunder detection.
10. BME280 polling.
11. Heap, PSRAM, and CPU metrics.
12. Network services: Wi-Fi, portal, HTTP API, BLE.
13. Touch gestures and taps.
14. UI rendering.

## Lightning Detection

The AS3935 is configured by direct I2C register access. On setup it:

1. Probes addresses `0x03`, `0x02`, and `0x01`.
2. Sends preset default command `0x3C` with value `0x96`.
3. Sends RCO calibration command `0x3D` with value `0x96`.
4. Clears the power-down bit in AFE register `0x00`.
5. Applies the HOME or ROAMING AFE gain profile.

The IRQ line is polled on GPIO39. Once high for at least 2 ms, the firmware reads AS3935 interrupt register `0x03` and handles:

| Interrupt | Value | Behavior |
| --- | --- | --- |
| Noise | `0x01` | Records a timed noise event for confidence scoring. |
| Disturber | `0x04` | Records a timed disturber event for confidence scoring. |
| Lightning | `0x08` | Reads distance register `0x07`, creates a strike candidate, opens a thunder listening window, and scores the candidate. |

Valid AS3935 distance buckets are: `1`, `5`, `6`, `8`, `10`, `12`, `14`, `17`, `20`, `24`, `27`, `31`, `34`, `37`, and `40` km.

## Strike Confidence Scoring

A lightning interrupt becomes a `StrikeCandidate` rather than immediately incrementing the session count.

Initial score:

```text
60 + session_bias
```

The score is clamped to `0..100`.

Decision thresholds:

| Score | Decision |
| --- | --- |
| `>= 75` | Accept |
| `45..74` | Prompt, if training mode and prompt rate limit allow |
| `< 45` | Suppress, while still allowing thunder-window rescoring until expiry |

Inputs that raise confidence include:

| Signal | Examples |
| --- | --- |
| Falling pressure | 1 h, 3 h, and 12 h pressure drops. |
| Low pressure | Current pressure below 1008 hPa or 1000 hPa. |
| Cooling and rising humidity | Temperature drop and humidity rise over 1 h. |
| Combined storm environment | Falling pressure plus rising humidity and/or falling temperature. |
| Recent lightning pattern | Accepted strikes in the last 10 and 30 minutes. |
| Increasing strike rate | Last 10 minutes exceeds previous 10 minutes. |
| Incoming range trend | New candidate is closer than current range. |
| Coherent range pattern | Far-mid-near progression or stable repeated distance. |
| Thunder correlation | Low-frequency microphone rumble detected inside the AS3935 distance-based timing window. |

Inputs that lower confidence include:

| Signal | Examples |
| --- | --- |
| Unlikely first close strike | First recent strike reported at 10 km or less is penalized unless the environment is strongly storm-like. |
| Frequent disturbers | Multiple AS3935 disturber interrupts in 10 minutes. |
| Frequent noise events | Multiple AS3935 noise interrupts in 10 minutes. |
| Stable fair-weather environment | Stable pressure, temperature, and humidity can reduce confidence. |
| Dry stable air | Humidity below 50% with stable humidity trend. |
| Non-thunder acoustic evidence | High-frequency dominant audio, short impulses, handling noise, knocks, or clicks can slightly reduce confidence. |

### Serial Training Prompt

Training mode is available in this release:

```cpp
const bool TRAINING_MODE_ENABLED = true;
const bool AUTO_LEARN_ENABLED = true;
```

If a candidate enters the prompt band and prompt rate limiting allows it, the serial console asks for a response within 20 seconds:

| Serial Input | Meaning | Effect |
| --- | --- | --- |
| `Y` | Yes, real strike | Accepts candidate and increases session bias by `+5`. |
| `N` | No, false strike | Suppresses candidate and decreases session bias by `-8`. |
| `U` | Unsure | Suppresses candidate with no bias change. |
| `R` | Reset training bias | Sets session bias back to `0`. |
| Timeout | No response | Suppresses candidate and decreases session bias by `-4`. |

Prompt rate limit is 3 prompts per 10 minutes. The session bias is bounded from `-20` to `+20` and is not persisted across reboot.

## Thunder Detection

The microphone runs on GPIO1 with 12-bit ADC and 11 dB attenuation. It is a secondary confidence input only.

Core rule:

```text
No AS3935 lightning interrupt = no strike candidate.
```

The microphone never creates lightning events. It only evaluates acoustic evidence after an AS3935 lightning candidate and contributes a bounded confidence delta to that candidate.

Accepted candidates remain acoustically trackable until their thunder window expires, so later rumble can update the displayed confidence score without creating another strike.

Thunder is treated as low-frequency dominant audio. The target band is 40-180 Hz, with 300 Hz and above treated as likely speech, clicks, handling noise, or other non-thunder transients. The code samples short ADC frames during an active listening window, applies lightweight first-order band filters, and compares low-band energy against high-band energy and an adaptive ambient low-band baseline.

### Distance-Based Listening Windows

Thunder delay is approximated as 3 seconds per km, and the AS3935 distance bucket controls when the microphone evidence is considered valid:

| AS3935 Distance | Listening Window After Candidate | Weight |
| --- | --- | --- |
| 1-5 km | 1-20 s | Full |
| 6-13 km | 10-50 s | Full |
| 14-23 km | 30-90 s | Reduced |
| 24-40 km | 60-150 s | Minimal |
| Unknown / out of range | 2-45 s | Reduced |

Lack of thunder does not strongly penalize distant strikes because terrain, wind, and atmospheric conditions may make thunder inaudible.

### Acoustic Confirmation

A valid thunder confirmation requires:

| Requirement | Implementation |
| --- | --- |
| Low-frequency energy | `low_band_energy > ambient_low_baseline * THUNDER_LOW_ENERGY_MULTIPLIER` |
| Low-frequency dominance | `low_to_high_ratio > THUNDER_RATIO_THRESHOLD` |
| Sustained rumble | Signal must persist for at least `THUNDER_MIN_RUMBLE_MS`. |
| Spike rejection | Short events below `THUNDER_IMPULSE_REJECT_MS` are rejected as impulses. |

Non-thunder rejection includes sharp impulses, high-frequency dominant sounds, short spikes, and broadband transients without a low-frequency rumble tail.

### Confidence Delta

Acoustic evidence contributes one bounded additive factor to the broader confidence score:

| Acoustic Evidence | Score Delta |
| --- | --- |
| Strong low-frequency rumble in expected window | `+15` before distance weighting |
| Moderate rumble in expected window | `+10` before distance weighting |
| Weak but plausible rumble | `+5` before distance weighting |
| No usable thunder evidence | `0` |
| High-frequency dominant noise | `-5` |
| Strong short impulse / handling / knock | `-10` |

Distance weighting keeps the strongest acoustic influence at 1-13 km, reduces it from 14-23 km, and keeps it minimal beyond 24 km.

Important constants:

| Constant | Value | Purpose |
| --- | --- | --- |
| `MIC_SAMPLE_INTERVAL_MS` | 20 ms | DSP frame update interval. |
| `MIC_DSP_SAMPLE_RATE_HZ` | 4000 Hz | In-frame ADC sample rate for low/high energy estimates. |
| `MIC_DSP_SAMPLE_COUNT` | 80 | Samples per DSP frame. |
| `THUNDER_LOW_ENERGY_MULTIPLIER` | 3.0 | Low-band energy threshold against ambient baseline. |
| `THUNDER_RATIO_THRESHOLD` | 1.8 | Minimum low-band to high-band energy ratio. |
| `THUNDER_MIN_RUMBLE_MS` | 500 ms | Minimum sustained rumble duration. |
| `THUNDER_STRONG_RUMBLE_MS` | 1200 ms | Strong-rumble duration threshold. |
| `THUNDER_IMPULSE_REJECT_MS` | 200 ms | Rejects short spikes and clicks. |
| `THUNDER_WINDOW_MS` | 150 s | Maximum display/state thunder window for accepted strikes. |
| `THUNDER_COOLDOWN_MS` | 12 s | Cooldown after confirmed thunder. |

The microphone baseline adapts quickly when idle and slowly during a thunder listening window. Confirmed thunder computes an approximate audio distance:

```text
audio_distance_km = thunder_delay_seconds / 3
```

This is a rough approximation used for display and scoring support, not a calibrated range estimate.

## Environmental Sensing

The BME280 driver is implemented directly in `Lightning_Detector_v1_0.ino`.

Setup behavior:

1. Probe `0x76`, then `0x77`.
2. Verify chip ID `0x60`.
3. Reset with `0xB6`.
4. Read calibration blocks from `0x88` and `0xE1`.
5. Configure humidity, measurement, and filter registers.

Runtime behavior:

| Value | Behavior |
| --- | --- |
| Read interval | Every 5 seconds. |
| UI trend sample interval | Every 10 seconds in `app_helpers.cpp`. |
| Confidence environment sample interval | Every 5 minutes. |
| Confidence sample buffer | 145 samples, enough for roughly 12 hours at 5 minute intervals. |

Temperature, pressure, and humidity compensation follow the BME280 datasheet style integer formulas, then convert to display units.

## GPS

GPS is read from UART1 at 9600 baud. The parser supports checksummed:

| Sentence | Data Used |
| --- | --- |
| `GPGGA` / `GNGGA` | Fix quality, satellites, altitude, latitude, longitude. |
| `GPRMC` / `GNRMC` | Time, date, speed, latitude, longitude, valid fix flag. |

The GPS is considered active if a character has been received within 5 seconds. If the stream goes inactive, fix validity and satellite count are cleared.

## Battery Monitor

Battery status uses `analogReadMilliVolts()` on GPIO17. The measured ADC voltage is multiplied by `BATTERY_VOLTAGE_DIVIDER`, default `3.0f`, then smoothed with an exponential moving average.

The default voltage curve maps approximately:

| Voltage | Percent |
| --- | --- |
| 4.20 V | 100% |
| 4.10 V | 90% |
| 4.00 V | 80% |
| 3.90 V | 65% |
| 3.80 V | 50% |
| 3.70 V | 35% |
| 3.60 V | 20% |
| 3.50 V | 10% |
| 3.30 V | 5% |
| 3.20 V | 0% |

Status flags:

| Condition | Status |
| --- | --- |
| Percent <= 20 | Low |
| Percent <= 10 | Critical |
| Voltage > 4.6 V | High warning |
| ADC invalid or out of range | Unknown |

## LEDs

Two NeoPixels on GPIO40 provide glanceable severity state:

| LED Index | Meaning |
| --- | --- |
| `0` | Strike rate severity. |
| `1` | Strike range severity. |

Both LEDs are deep blue during boot. Accepted strikes trigger a short flash. Colors are semantic:

| Condition | Color |
| --- | --- |
| No data / no activity | Deep blue |
| Low severity | Green |
| Moderate | Yellow |
| Elevated | Orange |
| High | Red |
| Extreme / very close or very high rate | Deep red |

Rate thresholds are defined in `semanticRgbForRate()`. Distance thresholds are defined in `semanticRgbForDistance()`.

## Touch UI

The dashboard has 8 pages:

| Page | Purpose |
| --- | --- |
| `LIVE` | Main distance, rate, session count, thunder state, and severity bar. |
| `RADAR` | Range rings and recent strike markers. Markers are randomized by angle because AS3935 provides distance, not direction. |
| `STORM` | Nearest strike, last strike, range trend, peak rate, and storm state. |
| `TRENDS` | Rate, temperature, humidity, and pressure graphs. |
| `ATMOS` | BME280 pressure, temperature, humidity, and link to trends. |
| `DISPLAY` | Brightness control. Contrast and gamma are shown as disabled placeholders. |
| `NETWORK` | HOME/ROAMING state, Wi-Fi/BLE status, reset Wi-Fi, and mode toggle. |
| `SYSTEM` | Battery, heap/PSRAM, CPU, mode, sensors, and GPS status. |

Gestures:

| Gesture | Behavior |
| --- | --- |
| Swipe right | Next page. |
| Swipe left | Previous page. |
| Tap LIVE distance card | Open RADAR. |
| Tap LIVE rate/thunder areas | Open TRENDS. |
| Tap ATMOS cards | Open related trend tab. |
| Tap TRENDS tabs | Switch rate/temp/humidity/pressure graph. |
| Tap DISPLAY +/- | Adjust brightness from 1 to 10. |
| Tap NETWORK reset | Show Wi-Fi reset confirmation. |
| Tap NETWORK mode button | Toggle HOME/ROAMING and persist it. |

Touch coordinates are mapped from FT6336 portrait coordinates into the landscape UI space.

## Network Features

### Captive Portal

When HOME mode needs credentials, the device starts an AP and DNS captive portal:

| Endpoint | Method | Purpose |
| --- | --- | --- |
| `/` | GET | HTML setup form. |
| `/save` | POST | Save `ssid` and `password`. |
| Any other path | Any | Redirect to `/`. |

Credentials are passed to `DeviceWifiManager`, saved in `Preferences`, and then used for a station connection attempt.

### HTTP API

The HTTP API runs only in HOME mode after Wi-Fi is connected.

| Endpoint | Response |
| --- | --- |
| `/` | JSON list of available endpoints. |
| `/api/status` | Mode, AS3935 profile, Wi-Fi status, SSID, IP, battery, uptime, GPS activity, web server state. |
| `/api/lightning` | Rate, latest distance, trend band, session strike count, latest strike age, AS3935 profile. |
| `/api/environment` | Temperature, humidity, pressure, GPS date, GPS time. |
| `/api/gps` | Active/fix flags, satellites, coordinates, altitude, speed, date, and time. |

Example status response shape:

```json
{
  "network_mode": "HOME",
  "as3935_profile": "HOME",
  "wifi_status": "CONNECTED",
  "ssid": "example",
  "ip": "192.168.1.23",
  "battery_percent": 82,
  "battery_voltage": 3.88,
  "uptime_s": 123,
  "gps_active": true,
  "gps_satellites": 7,
  "web_server": "ON"
}
```

### BLE Service

BLE is used in ROAMING mode. Device name:

```text
Lightning Detector
```

Service UUID:

```text
7f9d0001-7d2a-4d1d-9b8f-ad0000000001
```

Characteristics are read/notify:

| UUID | Value |
| --- | --- |
| `7f9d0002-7d2a-4d1d-9b8f-ad0000000001` | Strike rate per minute. |
| `7f9d0003-7d2a-4d1d-9b8f-ad0000000001` | Distance text. |
| `7f9d0004-7d2a-4d1d-9b8f-ad0000000001` | Session strike count. |
| `7f9d0005-7d2a-4d1d-9b8f-ad0000000001` | Environment summary. |
| `7f9d0006-7d2a-4d1d-9b8f-ad0000000001` | Battery percent text. |
| `7f9d0007-7d2a-4d1d-9b8f-ad0000000001` | Status text. |
| `7f9d0008-7d2a-4d1d-9b8f-ad0000000001` | AS3935 profile text. |

Notifications are sent when key values change or at least every 5 seconds while BLE is active.

## Build and Upload

From the repository root:

```sh
arduino-cli compile --fqbn esp32:esp32:esp32s3:PartitionScheme=huge_app Lightning_Detector_v1_0
```

For repeatable release builds, use the fully explicit FQBN from [Required Build Configuration](#required-build-configuration).

Upload example, replacing the port:

```sh
arduino-cli upload -p /dev/cu.usbmodemXXXX --fqbn esp32:esp32:esp32s3:PartitionScheme=huge_app Lightning_Detector_v1_0
```

Fully explicit upload example:

```sh
arduino-cli upload -p /dev/cu.usbmodemXXXX --fqbn esp32:esp32:esp32s3:USBMode=hwcdc,CDCOnBoot=default,UploadMode=default,CPUFreq=240,FlashMode=qio,FlashSize=4M,PartitionScheme=huge_app,DebugLevel=none,PSRAM=opi,LoopCore=1,EventsCore=1,EraseFlash=none Lightning_Detector_v1_0
```

Serial monitor:

```sh
arduino-cli monitor -p /dev/cu.usbmodemXXXX -c baudrate=115200
```

The firmware prints I2C scans, sensor setup status, microphone levels, confidence decisions, training prompts, mode transitions, Wi-Fi status, BLE advertising, and accepted strike summaries.

## Configuration Points

Most tuning values live near the top of `Lightning_Detector_v1_0.ino`.

| Area | Constants |
| --- | --- |
| Boot screen | `BOOT_MIN_HOLD_MS` |
| Touch gestures | `SWIPE_MIN_DELTA_PX`, `TAP_MAX_MOVE_PX`, `GESTURE_COOLDOWN_MS` |
| BME280 | `BME280_READ_INTERVAL_MS` |
| Microphone | `MIC_SAMPLE_INTERVAL_MS`, `MIC_DSP_SAMPLE_RATE_HZ`, `MIC_DSP_SAMPLE_COUNT`, band-energy thresholds, rumble duration thresholds |
| Strike confidence | `STRIKE_BASE_SCORE`, `STRIKE_ACCEPT_SCORE`, `STRIKE_PROMPT_MIN_SCORE`, prompt limits, session bias limits |
| Training | `TRAINING_MODE_ENABLED`, `AUTO_LEARN_ENABLED`, `ALLOW_TRAINING_RESET` |
| Environment scoring | `ENV_SAMPLE_COUNT`, `ENV_SAMPLE_INTERVAL_MS` |
| Storm reset | `STORM_INACTIVITY_RESET_MS` |

Hardware overrides:

| Header | Overrides |
| --- | --- |
| `battery_status.h` | `BATTERY_ADC_PIN`, `BATTERY_VOLTAGE_DIVIDER`, `BATTERY_DEBUG_LOG` |

## Troubleshooting

### Display does not initialize

Check the shared I2C bus first. The AMOLED power gate is controlled through the TCA9554 at `0x20`; if that expander is not reachable, display power may not be enabled. The serial log prints scans after display power and sensor setup.

### I2C scan shows every address or too many devices

The firmware treats this as an I2C bus fault. Check SDA/SCL shorts, pullups, sensor power, and whether an attached sensor is in the wrong bus mode.

### Touch is not responding

Touch setup is skipped if the I2C bus is faulted. Otherwise the firmware resets the FT6336 up to 3 times and probes address `0x38`. Check GPIO3 reset wiring, I2C lines, and panel variant.

### AS3935 not found

The firmware probes only `0x03`, `0x02`, and `0x01`. Confirm the AS3935 address select pins, I2C mode, power, ground, and IRQ wiring to GPIO39.

### BME280 not found

Only `0x76` and `0x77` are supported. The code requires chip ID `0x60`, so BMP280-style devices without humidity compensation will not work as BME280 replacements.

### Microphone constantly reports thunder candidates

The detector now looks for sustained low-frequency dominance rather than a single ADC spike. Check GPIO1 wiring, analog output level, grounding, and whether the module has an adjustable gain/potentiometer. If an installation is noisy, tune `THUNDER_LOW_ENERGY_MULTIPLIER`, `THUNDER_RATIO_THRESHOLD`, and the rumble duration constants before changing the broader confidence score thresholds.

### Battery reads unknown or unrealistic

Confirm the ADC pin and voltage divider on the exact board revision. The default assumes GPIO17 and an approximate 3:1 multiplier.

### Wi-Fi portal appears repeatedly

HOME mode starts provisioning if credentials are missing or after 3 connection failures. Use the NETWORK page to reset saved Wi-Fi credentials, then reconnect through the `Lightning-Setup-xxxxxx` AP.

### BLE not visible

BLE only starts in ROAMING mode. Toggle from the NETWORK page or clear the saved mode preference by changing the mode in firmware.

## Known Limitations

| Limitation | Detail |
| --- | --- |
| Direction is synthetic | AS3935 provides distance, not bearing. RADAR marker angles are randomized for visualization. |
| Training is session-only | Confidence bias is not persisted in `Preferences`. Reboot resets it. |
| Captive portal is plain HTTP | Credentials are submitted over the setup AP without TLS. |
| JSON is manually assembled | The API escapes strings where helper is used, but it does not use a full JSON library. |
| Contrast/gamma controls are placeholders | DISPLAY page only implements brightness changes. |
| Battery percent is approximate | It uses a simple Li-ion voltage curve and no charge-state detection. |
| No AS3935 tuning register UI | Home/roaming AFE gain is applied, but noise floor, watchdog, and spike rejection are not exposed on the UI. |

## Quick Field Checklist

1. Open serial monitor at `115200`.
2. Confirm I2C scan shows expected devices such as `0x20`, `0x38`, BME280 `0x76/0x77`, and AS3935 `0x01/0x02/0x03`.
3. Confirm display and touch initialize.
4. Confirm BME280 live updates appear every 5 seconds.
5. Confirm AS3935 profile matches the selected HOME/ROAMING mode.
6. Confirm microphone idle baseline is stable before testing thunder correlation.
7. Use ROAMING mode for BLE field use or HOME mode for Wi-Fi/API testing.
8. Watch serial confidence logs for `ACCEPT`, `PROMPT`, and `SUPPRESS` decisions during test events.

##Important Notes**
The CJMCU AS3935 board defaults to spi mode, it should be wired as below to enable i2c and set the correct address.

VCC  -> 3.3V
GND  -> GND
SCL  -> SCL
MOSI -> SDA
IRQ  -> GPIO39
CS   -> GND
MISO -> GND
SI   -> 3.3V
A0   -> GND
A1   -> 3.3V
