/*
 * Local credential overrides — copy to secrets.h (which is git-ignored)
 * and uncomment the lines you want to bake into the build.
 *
 * Precedence on each wake:
 *   1. Values in NVS (set via the captive portal)  -- always win if present
 *   2. Values defined here                          -- fallback for empty NVS
 *   3. Otherwise                                    -- captive portal triggers
 *
 * Use these to bypass the portal during development -- no NVS, no phone-tap
 * dance every time you flash a fresh board. Leaving any of them undefined is
 * fine; the portal will collect whatever's missing.
 *
 * IMPORTANT: secrets.h itself must NEVER be committed. .gitignore is wired
 * to ignore it; double-check before pushing.
 */
#pragma once

/* ---- WiFi ---------------------------------------------------------- */
// #define WIFI_DEFAULT_SSID  "your-network"
// #define WIFI_DEFAULT_PASS  "your-password"     /* "" for open networks */

/* ---- MQTT ---------------------------------------------------------- */
// #define MQTT_DEFAULT_URI   "mqtt://192.168.1.50:1883"   /* mqtts:// for TLS */
// #define MQTT_DEFAULT_TOPIC "inky/esp32/update"
// #define MQTT_DEFAULT_USER  "broker-user"       /* leave undefined if open */
// #define MQTT_DEFAULT_PASS  "broker-pass"
