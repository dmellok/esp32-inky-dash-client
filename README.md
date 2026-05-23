# ESP-Inky-Dash-Client

Battery-powered ESP32-S3 firmware for the [Waveshare ESP32-S3-ePaper-13.3E6](https://www.waveshare.com/esp32-s3-epaper-13.3e6.htm) — a 13.3", 1200×1600, 6-colour Spectra E6 e-paper panel paired with an ESP32-S3-WROOM-2-N32R16V module.

The device wakes on a timer, pulls a retained MQTT message containing an image URL, refreshes the panel if the image has changed, and goes back to deep sleep. WiFi credentials are provisioned on first boot via a SoftAP captive portal.

This is the embedded counterpart to the Pi-side [Inky_Dash_Listener](https://github.com/dmellok/Inky_Dash_Listener) MQTT daemon, designed so the same `inky/update` topic drives either a Pi-attached Inky Impression or this battery-powered standalone unit.

## Hardware

| Component | Detail |
| --- | --- |
| Module | ESP32-S3-WROOM-2-N32R16V (32 MB flash, 16 MB octal PSRAM) |
| Panel | 13.3" Spectra E6, 1200×1600 native portrait, 6 colours, 4 bpp packed |
| Power | Anything that survives ~80 mA during the 5–15 s WiFi/MQTT/HTTP burst and ~30 s panel refresh, then sleeps at single-digit µA |

Pinout (see [include/app_config.h](include/app_config.h)) matches the official Waveshare ESP-IDF demo. The panel has two SPI chip-selects (`CS_M`, `CS_S`) which drive the left and right halves of the display independently.

## Wake cycle

```
boot
  ├─ no WiFi creds in NVS?           → captive portal → reboot
  ├─ STA connect fails?              → captive portal → reboot
  ├─ no retained MQTT message?       → sleep (nothing new to show)
  ├─ URL unchanged since last wake?  → sleep (skip ~30s refresh)
  ├─ fetch URL → decode → paint      → store URL hash, sleep
  └─ deep sleep for SLEEP_INTERVAL_S (default 15 min)
```

Behaviour tunables (sleep interval, retry counts, AP credentials, MQTT topic / broker URI) all live at the top of [include/app_config.h](include/app_config.h).

## Build & flash

Requires [PlatformIO](https://platformio.org/). ESP-IDF 5.x and the Xtensa toolchain are pulled automatically on first build.

```bash
pio run                  # build
pio run -t upload        # flash via USB
pio device monitor       # 115200 baud, exception decoder enabled
```

First boot brings up a SoftAP named `InkyDash-Setup` (password `inkydash`). Join it from your phone; the captive-portal prompt opens a form for your home WiFi credentials and MQTT broker. After submit the device reboots, joins your network, and enters the normal wake cycle.

### Dev shortcut: `secrets.h`

To skip the captive portal during iteration, copy [include/secrets.example.h](include/secrets.example.h) to `include/secrets.h` and uncomment the `WIFI_DEFAULT_*` / `MQTT_DEFAULT_*` macros you want baked into the build. `secrets.h` is git-ignored. Precedence on each wake is NVS (set via portal) → `secrets.h` values → empty (portal triggers).

## MQTT contract

Publish a **retained** message to topic `inky/update` (configurable). The firmware accepts either form:

```text
https://example.com/dashboard.bin
```

```json
{ "url": "https://example.com/dashboard.bin" }
```

The URL must point to either:

1. **Panel-native raw frame** (recommended for battery efficiency) — exactly 960,000 bytes of packed 4 bpp nibbles, 1600 rows × 600 bytes/row. The left half of each row (300 bytes) goes to the left controller, the right half to the right controller. Palette values: `0`=black, `1`=white, `2`=yellow, `3`=red, `5`=blue, `6`=green. URLs ending in `.bin` or `.epd`, or served with `Content-Type: application/octet-stream`, are auto-detected.

2. **JPEG / PNG** — *not yet implemented in v1*. The decoder dispatch in [src/image_decoder.c](src/image_decoder.c) has a clearly marked TODO block describing exactly what to add (pull in `espressif/esp_new_jpeg` via the IDF component manager, decode to RGB888 at panel resolution, Floyd-Steinberg dither to the 6-colour palette).

The firmware persists the SHA-256 of the rendered URL in NVS so unchanged retained messages don't trigger needless panel refreshes.

### Runtime config: `inky/esp32/config`

A separate retained topic (default `inky/esp32/config`, overridable via `MQTT_DEFAULT_CONFIG_TOPIC`) lets you change device behaviour without re-flashing. Publish JSON; the firmware applies it on next wake and persists it to NVS so it survives reboots.

Currently honoured:

```json
{ "sleep_interval_s": 600 }
```

Clamped to `[30, 604800]` (30 s – 7 days). Bad values are rejected with a log warning and the previously-stored interval is kept. Falls back to the compile-time `SLEEP_INTERVAL_S` (15 min) when NVS is empty.

## Project layout

```
ESP-Inky-Dash-Client/
├── platformio.ini             # board, partitions, monitor settings
├── partitions.csv             # 14 MB factory app + NVS
├── sdkconfig.defaults         # PSRAM, mbedTLS cert bundle, MQTT 3.1.1
├── include/
│   └── app_config.h           # pinout + all behaviour tunables
└── src/
    ├── main.c                 # boot → check → render → sleep state machine
    ├── epd_driver.{c,h}       # Waveshare 13.3E6 panel driver
    ├── wifi_manager.{c,h}     # NVS-backed STA connect with retry
    ├── provisioning.{c,h}     # SoftAP + DNS hijack + HTTP form captive portal
    ├── mqtt_handler.{c,h}     # single-shot retained-message grabber
    ├── image_fetcher.{c,h}    # HTTPS download into PSRAM
    └── image_decoder.{c,h}    # raw .bin pass-through (JPEG path is a stub)
```

## Credits

The panel driver in [src/epd_driver.c](src/epd_driver.c) is a port of the official ESP-IDF demo published by Waveshare at [waveshareteam/ESP32-S3-ePaper-13.3E6](https://github.com/waveshareteam/ESP32-S3-ePaper-13.3E6). The init byte sequence and command set are panel-specific and kept byte-for-byte exact.

## License

MIT
