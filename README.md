# Fingerprint Keyboard

A fingerprint-gated "keyboard". Touch the sensor with a registered finger and your
password is typed into the focused window on your PC. No password is stored or
typed until your fingerprint matches.

## Idea

An ESP32-C3 SuperMini reads a fingerprint sensor. When a fingerprint matches, the
board sends the password for that user over your local WiFi as an encrypted UDP
packet. A small Python daemon on the PC receives it, decrypts it, and types it
using `wtype` — so it works anywhere (login screens, browsers, terminals).

Passwords are encrypted with RSA-OAEP at **build time**: `make build` encrypts the
plaintext passwords from `secrets.h` with a public key and compiles only the
ciphertext into the firmware. The ESP32 itself contains no plaintext passwords and
no crypto code — it just sends the pre-encrypted bytes. Only the PC, which holds
the private key, can decrypt them.

## Hardware

- ESP32-C3 SuperMini
- SFM-V1.7 Semiconductor Integrated Touch Capacitive Acquisition And
  Identification Fingerprint Sensor Module (UART communication, 115200 baud)

| Sensor | ESP32-C3 |
|--------|----------|
| Yellow (TXD) | GPIO 20 (UART RX) |
| Black (RXD)  | GPIO 21 (UART TX) |
| Blue (touch) | GPIO 0 (IRQ) |
| Green (3.3V) | 3.3V |
| White (3.3V) | 3.3V |
| Red (GND)    | GND |

![ESP32-C3 SuperMini pinout](esp32-c3-super-mini-pinout.svg)

Requires the **SFM-V1.7** library by Matrixchung (`#include "sfm.hpp"`,
class `SFM_Module`).

## ESP32 Setup

Create the file `fingerprint_keyboard/secrets.h` with this template and fill in
the real values (`WIFI_SSID`, `WIFI_PASS`, `SERVER_HOST` = your PC's IP,
`AUTH_TOKEN`, and `PASSWORD_1`/`PASSWORD_2`, one per enrolled finger):

```cpp
#ifndef SECRETS_H
#define SECRETS_H

#define WIFI_SSID    "your-wifi-ssid"
#define WIFI_PASS    "your-wifi-password"
#define SERVER_HOST  "SERVER_HOST"
#define AUTH_TOKEN   "your-token"

const char PASSWORD_1[] = "password-for-finger-1";
const char PASSWORD_2[] = "password-for-finger-2";

#endif
```

The plaintext passwords never reach the device: at build time
`typer/gen_cipher.py` encrypts them with `typer/public_key.pem` into
`fingerprint_keyboard/cipher.h` (gitignored), which is the only thing compiled
into the firmware.

This file is gitignored, so your credentials stay out of the repository. For extra
security, `secrets.h` is owned by `root:root` with mode `600` (root-only), so
building and flashing **require sudo**:

```bash
make flash    # prompts for your sudo password
make monitor  # optional, serial output
```

The Makefile wraps `arduino-cli` with `sudo` (keeping `HOME` so your esp32 cores
and libraries are still found). To edit the secrets: `sudo nano fingerprint_keyboard/secrets.h`.

Serial commands: `e` = enroll a finger (3 presses), `d` = delete all users.

## PC Setup (daemon)

```bash
sudo pacman -S wtype python-cryptography   # keystroke typer + RSA decryption
cd typer
python3 gen_keys.py         # one-time: creates private_key.pem + public_key.pem
nano .env                   # set AUTH_TOKEN (must match the ESP32's secrets.h)
python3 typer.py            # run it and keep it open
```

`private_key.pem` is the only thing that can decrypt your passwords — it is
gitignored and mode 600. Back it up if you don't want to re-enroll after a
reinstall; regenerating the keypair means reflashing the ESP32.

To auto-start the daemon on login, add to `~/.config/hypr/autostart.conf` (Hyprland):

```
exec-once = bash -c 'while true; do python3 /path/to/typer/typer.py >> /tmp/typer.log 2>&1; sleep 2; done'
```

This is written for a Hyprland setup, but the daemon is just a Python script — it
works equally well under systemd (e.g. a `systemd --user` service). The Hyprland
`exec-once` wrapper was simply more convenient here.

## Security notes

- `AUTH_TOKEN` must match between the ESP32 (`secrets.h`) and `typer/.env`.
- Passwords are RSA-2048-OAEP encrypted at build time; the ESP32 flash and the
  WiFi packets contain only ciphertext. Only `typer/private_key.pem` decrypts.
- The ciphertext is fixed per password, so a captured packet could be replayed
  by someone already on your LAN. If that matters, encrypt on the fly instead.
- Open UDP port `4444` on your PC's firewall only to the ESP32 (e.g.
  `sudo ufw allow from 192.168.0.204 to any port 4444 proto udp`).
