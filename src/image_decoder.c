#include "image_decoder.h"
#include "app_config.h"

#include <string.h>
#include <strings.h>

#include "esp_heap_caps.h"
#include "esp_log.h"

static const char *TAG = "decode";

static bool ends_with(const char *s, const char *suf)
{
    if (!s || !suf) return false;
    size_t sl = strlen(s), fl = strlen(suf);
    return sl >= fl && strcasecmp(s + sl - fl, suf) == 0;
}

/* "Raw panel-native" classifier. We're deliberately strict here -- the
 * panel will happily render garbage if we feed it the wrong size. */
static bool looks_like_raw(const fetched_image_t *src, const char *url)
{
    if (src->len != EPD_BUF_BYTES) return false;

    if (url) {
        /* Strip any ?query before checking extension. */
        const char *q = strchr(url, '?');
        size_t plen = q ? (size_t)(q - url) : strlen(url);
        char path[256];
        if (plen >= sizeof(path)) plen = sizeof(path) - 1;
        memcpy(path, url, plen);
        path[plen] = '\0';
        if (ends_with(path, ".bin") || ends_with(path, ".epd")) return true;
    }
    if (src->content_type[0]) {
        if (strncasecmp(src->content_type, "application/octet-stream", 24) == 0) return true;
        if (strncasecmp(src->content_type, "image/x-epd-raw", 15) == 0) return true;
    }
    /* Content-type missing but size matches exactly: treat as raw. */
    return true;
}

esp_err_t image_decode_to_frame(const fetched_image_t *src,
                                const char *url,
                                uint8_t **out_frame)
{
    if (!src || !out_frame) return ESP_ERR_INVALID_ARG;
    *out_frame = NULL;

    if (looks_like_raw(src, url)) {
        ESP_LOGI(TAG, "raw panel-native frame (%u bytes)", (unsigned)src->len);
        uint8_t *frame = heap_caps_malloc(EPD_BUF_BYTES, MALLOC_CAP_SPIRAM);
        if (!frame) return ESP_ERR_NO_MEM;
        memcpy(frame, src->data, EPD_BUF_BYTES);
        *out_frame = frame;
        return ESP_OK;
    }

    /* ---------------- JPEG / PNG path -------------------------------
     * Intentionally not implemented yet. To enable, add to
     * `idf_component.yml`:
     *
     *     dependencies:
     *       espressif/esp_new_jpeg: "^0.6.0"
     *
     * then in this function:
     *   - call jpeg_decoder_process() to get an RGB888 buffer at the
     *     target panel resolution (use the decoder's scale-to-fit hint
     *     for cheap downscaling of huge inputs);
     *   - Floyd-Steinberg dither RGB -> {black,white,yellow,red,blue,green}
     *     while packing two nibbles per byte into the output frame.
     *
     * A reasonable palette in sRGB is:
     *     BLACK  (  0,  0,  0)   WHITE  (255,255,255)
     *     YELLOW (255,255,  0)   RED    (255,  0,  0)
     *     BLUE   (  0,  0,255)   GREEN  (  0,255,  0)
     * Pick the nearest in linear RGB and propagate the quantization
     * error to the right/below neighbors with the standard 7/16, 3/16,
     * 5/16, 1/16 weights.
     * ---------------------------------------------------------------- */
    ESP_LOGE(TAG,
        "JPEG/PNG decode not yet enabled (url='%s', type='%s', %u bytes). "
        "Either pre-render to %u-byte panel-native .bin server-side, or "
        "add the esp_new_jpeg component and fill in image_decoder.c.",
        url ? url : "?", src->content_type, (unsigned)src->len,
        (unsigned)EPD_BUF_BYTES);
    return ESP_ERR_NOT_SUPPORTED;
}
