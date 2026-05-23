/*
 * Image-payload -> panel-native frame buffer dispatch.
 *
 * The "hybrid" pipeline this firmware was sized for:
 *
 *   1. Pre-rendered path (ENABLED in v1):
 *        Server delivers a 960000-byte file in panel-native 4bpp packed
 *        format. We download it and stream straight to the panel.
 *        Detected by: content-type "application/octet-stream", or URL
 *        ending in ".bin"/".epd", and length exactly EPD_BUF_BYTES.
 *
 *   2. On-device JPEG path (STUB in v1; see image_decoder.c):
 *        Decode JPEG, scale to 1200x1600, Floyd-Steinberg dither to the
 *        6-color palette. Needs the esp_jpeg managed component pulled in
 *        via idf_component_manager; left as a clearly-marked TODO until
 *        you've validated the raw path end-to-end on real hardware.
 */
#pragma once

#include <stdint.h>
#include <stddef.h>
#include "esp_err.h"
#include "image_fetcher.h"

/* Allocate (in PSRAM) and produce an EPD_BUF_BYTES-sized panel frame.
 * On success, *out_frame is owned by the caller and must be free()d.
 * Hints (url, content_type) come from image_fetch(); either may be empty. */
esp_err_t image_decode_to_frame(const fetched_image_t *src,
                                const char *url,
                                uint8_t **out_frame);
