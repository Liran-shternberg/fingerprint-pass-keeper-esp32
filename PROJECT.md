# Fingerprint Sensor (ESP32-C3 SuperMini) Project

## Overview

This project drives an **SFM-V1.7 fingerprint sensor** from an **ESP32-C3 SuperMini**
(board FQBN `esp32:esp32:makergo_c3_supermini`).

The sensor uses the SFM-V1.7 UART protocol (0xF5 framing, 115200 baud, 3C3R 3-press
enrollment). It was previously misidentified as an HLK-ZW0906; the HLK/Adafruit
fingerprint libraries do **not** work with it.

## Library

Use the **SFM-V1.7** library (by Matrixchung), installed locally at:

```
/home/liran/Arduino/libraries/SFM-V1.7
```

Key API facts:

- Include the header as `#include "sfm.hpp"` (NOT `SFM_V1_7.h`).
- Class name is `SFM_Module`, not `SFM_V1_7`.
- Constructor for ESP32: `SFM_Module(vccPin, irqPin, rxPin, txPin)`
  where `rxPin`/`txPin` are the ESP32 UART pins and `vccPin` is driven HIGH
  by the library to power the module.
- Baud is hardcoded to **115200**.
- You MUST call `SFM.setPinInterrupt(sfmPinInt1)` in `setup()`.
- Call `SFM.enable()` after power-on (there's also a low-power `SFM.disable()`).
- Useful methods:
  - `isConnected()`, `getUuid()`, `getUserCount()`
  - `isTouched()` — touch detection via the IRQ pin (blue wire)
  - `recognition_1vN(uid)` — 1:N match, returns ACK and fills `uid` (0 = no match)
  - `register_3c3r_1st(uid)`, `register_3c3r_2nd()`, `register_3c3r_3rd(uid)`
    — 3C3R (3-press) enrollment state machine. Must be run in order; restart
    from step #1 on any failure.
  - `setRingColor(color)` — ring LED control.

## Wiring (ESP32-C3 SuperMini <-> Sensor)

| Sensor wire | Sensor pin  | ESP32-C3 pin |
|-------------|-------------|--------------|
| Yellow      | TXD         | GPIO 20 (UART RX) |
| Black       | RXD         | GPIO 21 (UART TX) |
| Blue        | Touch/Detect| GPIO 0 (IRQ)      |
| Red         | GND         | GND               |
| Green       | 3.3V        | 3.3V              |
| White       | not used    | —                 |

Notes:

- Yellow (sensor TX) goes to ESP32 **RX**, Black (sensor RX) goes to ESP32 **TX**.
- Blue is a touch/IRQ line that asserts when a finger is placed on the sensor.
- Green wire is the sensor power rail (3.3V TTL).
- The SFM library constructor needs a `vccPin`; when powering the module
  directly from 3.3V, pass any unused GPIO (e.g. GPIO 10) — it just gets set HIGH.

## Build System

`make` targets in the project Makefile:

```bash
make build SKETCH=<folder>     # compile
make flash SKETCH=<folder>     # flash to board
make monitor SKETCH=<folder>   # serial monitor
```

FQBN is `esp32:esp32:makergo_c3_supermini`.

## Status / Prior Results

- Sensor responds to the SFM library: `Connected: 1`, UUID readable.
- Recognition runs but returns `uid: 0` — no fingerprints enrolled yet
  (user count 0).
- Enrollment is done via the 3C3R flow: put finger down, lift for 2s,
  press again, lift 2s, press a third time.

## Working Notes

- GPIO 8 LED is **active-low** (`LOW` = LED on).
- Bluetooth/WiFi coexistence is a known issue on this board: disable BT via
  `esp_bt_controller_disable()` and lower TX power to `WIFI_POWER_8_5dBm` if WiFi
  is also used.
- The previous `esp32_test` directory (`/home/liran/Projects/esp32_test`) held
  experiments (`blink_test`, `ble_test`, `fingerprint_test`, `sniff_test`,
  `SFM_Test`). This directory is the fresh, dedicated project.
