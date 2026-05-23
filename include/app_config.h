/*
 * Project-wide tunables. Edit here, not scattered across .c files.
 */
#pragma once

#include <stdint.h>

/* ------------------------------------------------------------------ */
/* Panel: Waveshare ESP32-S3-ePaper-13.3E6                            */
/* Pinout copied verbatim from the official ESP-IDF demo:             */
/*   waveshareteam/ESP32-S3-ePaper-13.3E6 (epaper_port.h)             */
/* ------------------------------------------------------------------ */
#define EPD_PIN_SCLK   9
#define EPD_PIN_MOSI   46
#define EPD_PIN_CS_M   10   /* drives the left  half (cols   0..599)  */
#define EPD_PIN_CS_S   3    /* drives the right half (cols 600..1199) */
#define EPD_PIN_DC     11
#define EPD_PIN_RST    2
#define EPD_PIN_BUSY   12
#define EPD_PIN_PWR    1    /* active-high panel power enable          */

#define EPD_SPI_HOST   SPI3_HOST
#define EPD_SPI_HZ     (10 * 1000 * 1000)

/* Panel geometry. Native orientation is portrait. */
#define EPD_WIDTH      1200
#define EPD_HEIGHT     1600
#define EPD_BUF_BYTES  ((EPD_WIDTH * EPD_HEIGHT) / 2)   /* 4bpp packed = 960000 */

/* 6-color palette indices (nibble values). 4 and 7 are reserved. */
#define EPD_COL_BLACK   0x0
#define EPD_COL_WHITE   0x1
#define EPD_COL_YELLOW  0x2
#define EPD_COL_RED     0x3
#define EPD_COL_BLUE    0x5
#define EPD_COL_GREEN   0x6

/* ------------------------------------------------------------------ */
/* Application behavior                                               */
/* ------------------------------------------------------------------ */

/* How long to deep-sleep between MQTT checks. 15 minutes is a good
 * trade for a 6-color panel that itself takes ~30s to refresh. */
#define SLEEP_INTERVAL_S   (15 * 60)

/* Cap on how long we'll wait for a retained MQTT message after
 * subscribing, before giving up and going back to sleep. */
#define MQTT_WAIT_MS       8000

/* WiFi STA connect attempt: retry count and per-attempt timeout. */
#define WIFI_CONNECT_RETRIES   5
#define WIFI_CONNECT_TIMEOUT_MS 15000

/* How long the provisioning portal stays up after a failed STA
 * connect. Power gets burned the whole time, so don't make it huge. */
#define PROVISION_PORTAL_TIMEOUT_S  (10 * 60)

/* SoftAP credentials shown to the user during provisioning. */
#define PROVISION_AP_SSID    "InkyDash-Setup"
#define PROVISION_AP_PASS    "inkydash"     /* >= 8 chars or use open AP */

/* ------------------------------------------------------------------ */
/* MQTT contract (matches inky_mqtt.py topic conventions)             */
/* ------------------------------------------------------------------ */
/* Override any of these at compile time with -D, or via menuconfig if
 * you'd rather Kconfig them. Defaults match the Python listener. */
#ifndef MQTT_BROKER_URI
#define MQTT_BROKER_URI    "mqtt://homeassistant.local:1883"
#endif
#ifndef MQTT_TOPIC_UPDATE
#define MQTT_TOPIC_UPDATE  "inky/update"
#endif
#ifndef MQTT_CLIENT_ID
#define MQTT_CLIENT_ID     "esp-inky-dash"
#endif

/* NVS namespace and keys */
#define NVS_NS_WIFI        "wifi"
#define NVS_KEY_SSID       "ssid"
#define NVS_KEY_PASS       "pass"

#define NVS_NS_STATE       "state"
#define NVS_KEY_LAST_HASH  "last_hash"   /* sha256 of last rendered URL */
