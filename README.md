# ESP-Inky-Dash-Client

Battery-powered ESP32-S3 firmware for the [Waveshare ESP32-S3-ePaper-13.3E6](https://www.waveshare.com/esp32-s3-epaper-13.3e6.htm) — a 13.3", 1200×1600, 6-colour Spectra E6 e-paper panel paired with an ESP32-S3-WROOM-2-N32R16V module.

The device wakes on a timer, publishes a heartbeat with its battery state, pulls a retained MQTT message containing an image URL, refreshes the panel if the image has changed, and goes back to deep sleep. WiFi credentials and MQTT broker details are provisioned on first boot via a SoftAP captive portal.

This is the embedded counterpart to the Pi-side [Inky_Dash_Listener](https://github.com/dmellok/Inky_Dash_Listener) MQTT daemon, designed so the same broker can drive either a Pi-attached Inky Impression or this battery-powered standalone unit — the firmware listens on its own `inky/esp32/*` topic namespace so both can coexist on one broker.

## Hardware

| Component | Detail |
| --- | --- |
| Module | ESP32-S3-WROOM-2-N32R16V (32 MB flash, 16 MB octal PSRAM) |
| Panel | 13.3" Spectra E6, 1200×1600 native portrait, 6 colours, 4 bpp packed |
| Battery | Single-cell Li-Po (3.3–4.2 V) via the onboard ETA6098 charger; battery sense on GPIO8 / ADC1 ch7 through a 1:3 divider |
| Power profile | ~80 mA during the 5–15 s WiFi/MQTT/HTTP burst, ~60 mA during the ~30 s panel refresh, single-digit µA in deep sleep |

Pinout (see [include/app_config.h](include/app_config.h)) matches the official Waveshare ESP-IDF demo. The panel has two SPI chip-selects (`CS_M`, `CS_S`) which drive the left and right halves of the display independently.

## Wake cycle

```
boot
  ├─ cold boot (RESET / power-on)?   → paint 6-band colour splash (panel sanity check)
  ├─ no WiFi creds anywhere?         → captive portal → reboot
  ├─ STA connect fails?              → captive portal → reboot
  ├─ publish heartbeat               (battery_mv/pct, rssi, ip)
  ├─ no retained MQTT message?       → sleep (nothing new to show)
  ├─ URL unchanged since last wake?  → sleep (skip ~30 s refresh)
  ├─ fetch URL → decode → paint      → store URL hash
  └─ deep sleep OR loop:
       ├─ USB host attached?         → short-delay restart loop (dev mode)
       └─ otherwise                  → deep sleep for the configured interval
```

The "no sleep when USB-host attached" logic uses ESP-IDF's `usb_serial_jtag_is_connected()` to read host SOF packets — a USB charger / power bank (no SOFs) is treated as battery operation. Force-override either way with `DEV_DISABLE_SLEEP` in `secrets.h`.

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

For fast iteration without USB plugged in (e.g. headless testing), also define `DEV_DISABLE_SLEEP` in `secrets.h` — the firmware will loop on `DEV_LOOP_INTERVAL_S` (default 10 s) instead of deep-sleeping. With USB plugged in this is automatic; the manual flag is only needed otherwise.

## MQTT contract

The firmware uses three topics under the `inky/esp32/` namespace. The update and config topics are read on every wake; the status topic is written on every wake.

| Topic | Direction | Retained | Purpose |
|---|---|---|---|
| `inky/esp32/update` | publisher → device | yes | URL of the next image to render |
| `inky/esp32/config` | publisher → device | yes | Runtime device settings |
| `inky/esp32/status` | device → broker | yes | Wake-time heartbeat |

All topic names are overridable in `secrets.h` (`MQTT_DEFAULT_TOPIC`, `MQTT_DEFAULT_CONFIG_TOPIC`, `MQTT_DEFAULT_STATUS_TOPIC`).

### Update payload

Bare URL or JSON:

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

### Config payload

JSON; applied on next wake and persisted to NVS so it survives reboots.

```json
{ "sleep_interval_s": 600 }
```

Clamped to `[30, 604800]` (30 s – 7 days). Bad values are rejected with a log warning and the previously-stored interval is kept. Falls back to the compile-time `SLEEP_INTERVAL_S` (15 min) when NVS is empty.

### Status (heartbeat) payload

Published once per wake, immediately after the broker connection succeeds:

```json
{
  "battery_mv": 3950,
  "battery_pct": 67,
  "rssi": -42,
  "ip": "192.168.50.234"
}
```

- `battery_mv` — millivolts at the cell. `0` means the ADC read failed (treat as unknown, not flat).
- `battery_pct` — 0–100, derived from a two-segment piecewise-linear Li-Po curve.
- `rssi` — wifi signal in dBm at the time of the heartbeat.
- `ip` — STA IPv4 address.

Retained, so a dashboard can show "last known state" without the device being awake. A heartbeat significantly older than the configured sleep interval indicates a dead battery or other failure.

## Battery sizing

Per-wake cost on a single-cell Li-Po, default 15 min sleep interval:

- **Hash-skip wake** (no render): ~0.13 mAh
- **Render wake** (download + paint): ~0.75 mAh
- **Deep sleep**: ~1.2 mAh/day

Expected runtime by usage:

| Use case | Daily draw | 2000 mAh | 5000 mAh | 10000 mAh |
|---|---|---|---|---|
| Photo frame (1 update/day) | ~14 mAh | 5 months | **12 months** | 24 months |
| Dashboard (hourly updates) | ~30 mAh | 2 months | **5.5 months** | 11 months |
| Frequent (every wake renders) | ~75 mAh | 1 month | 2 months | 4 months |

For "months of usage" with a typical update cadence, a 5000–10000 mAh single-cell Li-Po pouch cell is the sweet spot. Bumping `sleep_interval_s` via the config topic from 15 min to 30 or 60 min proportionally cuts wake-cycle cost.

## Project layout

```
ESP-Inky-Dash-Client/
├── platformio.ini             # board, partitions, monitor settings
├── partitions.csv             # 14 MB factory app + NVS
├── sdkconfig.defaults         # PSRAM, mbedTLS cert bundle, MQTT 3.1.1
├── include/
│   ├── app_config.h           # pinout + all behaviour tunables
│   ├── secrets.example.h      # template for local credential overrides
│   └── secrets.h              # (git-ignored) your local overrides
└── src/
    ├── main.c                 # boot → splash → connect → render → sleep
    ├── epd_driver.{c,h}       # Waveshare 13.3E6 panel driver + colour-bar splash
    ├── heartbeat.{c,h}        # battery / RSSI / IP read + JSON formatter
    ├── wifi_manager.{c,h}     # NVS-backed STA connect with retry
    ├── provisioning.{c,h}     # SoftAP + DNS hijack + HTTP form captive portal
    ├── mqtt_config.{c,h}      # NVS-backed broker URI / topic / credentials
    ├── mqtt_handler.{c,h}     # single-shot subscribe + dispatch + heartbeat publish
    ├── image_fetcher.{c,h}    # HTTPS download into PSRAM
    └── image_decoder.{c,h}    # raw .bin pass-through (JPEG path is a stub)
```

## Credits

The panel driver in [src/epd_driver.c](src/epd_driver.c) is a port of the official ESP-IDF demo published by Waveshare at [waveshareteam/ESP32-S3-ePaper-13.3E6](https://github.com/waveshareteam/ESP32-S3-ePaper-13.3E6). The init byte sequence and command set are panel-specific and kept byte-for-byte exact.

## License

MIT
