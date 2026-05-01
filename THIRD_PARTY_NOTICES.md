# Third-Party Notices

This project is licensed under the MIT License. That license applies to the
project source code in this repository, except where a file says otherwise.

This repository does not vendor the Arduino core or external Arduino libraries.
They are installed separately through Arduino CLI or Arduino IDE. If you
redistribute compiled firmware, packaged source, or a complete build bundle,
include the applicable third-party license texts and notices listed below.

This file is a good-faith attribution and compliance aid, not legal advice.

## Direct Arduino Library Dependencies

### GFX Library for Arduino

- Package name: `GFX Library for Arduino`
- Version checked: `1.6.5`
- Author/maintainer: Moon On Our Nation <moononournation@gmail.com>
- Installed include used by this project: `Arduino_GFX_Library.h`
- Upstream: https://github.com/moononournation/Arduino_GFX
- License: BSD license

Copyright notice from the installed library:

```text
Copyright (c) 2012 Adafruit Industries. All rights reserved.
```

Redistribution note:

Source and binary redistribution are permitted if the copyright notice, license
conditions, and warranty disclaimer from the library's `license.txt` are kept
with the redistributed source or accompanying documentation.

### Adafruit NeoPixel

- Package name: `Adafruit NeoPixel`
- Version checked: `1.15.4`
- Author/maintainer: Adafruit <info@adafruit.com>
- Installed include used by this project: `Adafruit_NeoPixel.h`
- Upstream: https://github.com/adafruit/Adafruit_NeoPixel
- License: GNU Lesser General Public License, version 3 or later

Redistribution note:

This project may be distributed under MIT while using Adafruit NeoPixel as a
separate LGPL-licensed library. If you distribute firmware or another binary
that includes or links this library, include the LGPL license text and comply
with LGPL requirements for the library portion, including preserving notices and
allowing recipients to replace, relink, or otherwise use a modified version of
the LGPL-covered library where required by the license.

Additional note:

The installed Adafruit NeoPixel library also contains platform-specific source
files with their own notices, including Apache-2.0 notices in ESP-specific code.
Keep the library's `COPYING`, source file notices, and README license section
with any redistribution of that library.

## Arduino Core And Platform Dependencies

### Arduino ESP32 Core

- Package ID: `esp32:esp32`
- Version checked: `3.3.8`
- Provides includes used by this project, including:
  - `Arduino.h`
  - `Wire.h`
  - `WiFi.h`
  - `WebServer.h`
  - `DNSServer.h`
  - `Preferences.h`
  - `HardwareSerial.h`
  - `BLEDevice.h`
  - `BLEServer.h`
  - `BLEUtils.h`
- Upstream: https://github.com/espressif/arduino-esp32

The Arduino ESP32 core and its bundled components include multiple third-party
licenses. Preserve the upstream license files and notices when redistributing a
complete build environment, core source, or firmware package.

The BLE library included with the checked ESP32 core installation contains an
Apache License 2.0 `LICENSE` file and a `NOTICE` file. The installed notice
summarizes the following copyright attributions:

```text
Copyright 2017-2026 Espressif Systems (Shanghai) PTE LTD
Copyright 2017 Neil Kolban
Copyright 2020-2025 Ryan Powell <ryan@nable-embedded.io> and esp-nimble-cpp,
NimBLE-Arduino contributors.
```

The BLE notice also identifies code based on or derived from:

- `esp32-snippets`, Copyright 2017 Neil Kolban, Apache-2.0
- `NimBLE-Arduino` / `esp-nimble-cpp`, Copyright 2020-2025 Ryan Powell and
  contributors, Apache-2.0

## Standard Library Headers

The project also includes standard C/C++ headers such as `math.h`, `string.h`,
`stdlib.h`, and `ctype.h`. These are supplied by the selected compiler,
toolchain, and platform package. If you redistribute the full toolchain or SDK,
keep the corresponding upstream license files with that distribution.

## Practical Release Checklist

- Keep this repository's `LICENSE` file with all source distributions.
- Keep this `THIRD_PARTY_NOTICES.md` file with source and firmware releases.
- If distributing binaries or firmware images, also include the full license
  texts for LGPL-3.0-or-later, Apache-2.0, and the BSD license used by
  `GFX Library for Arduino`.
- Do not describe the entire compiled firmware as "MIT only"; the project code
  is MIT, but third-party components keep their own licenses.
- If shipping a commercial product, have counsel review the LGPL obligations
  for the firmware distribution model.
