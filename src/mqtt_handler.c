#include "mqtt_handler.h"
#include "app_config.h"
#include "mqtt_config.h"

#include <string.h>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "mqtt_client.h"

static const char *TAG = "mqtt";

#define BIT_GOT_MSG  BIT0
#define BIT_FAILED   BIT1

typedef struct {
    EventGroupHandle_t events;
    mqtt_job_t *out;
    const char *topic;     /* topic we subscribed to (for logging) */
} ctx_t;

/* Cheap-and-good JSON URL extractor. Looks for "url" : "<value>" with
 * tolerance for whitespace and either quote style. Falls back to treating
 * the whole payload as a bare URL if it starts with "http". */
static bool extract_url(const char *data, size_t len, char *dst, size_t dst_sz)
{
    if (len == 0) return false;

    /* Bare URL case */
    if (len < dst_sz - 1 && (
            strncmp(data, "http://",  7) == 0 ||
            strncmp(data, "https://", 8) == 0)) {
        memcpy(dst, data, len);
        dst[len] = '\0';
        /* strip trailing whitespace/newlines */
        while (len && (dst[len-1] == ' ' || dst[len-1] == '\r' ||
                       dst[len-1] == '\n' || dst[len-1] == '\t')) {
            dst[--len] = '\0';
        }
        return dst[0] != '\0';
    }

    /* JSON case: find "url" */
    const char *p = memmem(data, len, "\"url\"", 5);
    if (!p) return false;
    p += 5;
    /* skip ws, colon, ws, opening quote */
    while (p < data + len && (*p == ' ' || *p == '\t')) p++;
    if (p >= data + len || *p != ':') return false;
    p++;
    while (p < data + len && (*p == ' ' || *p == '\t')) p++;
    if (p >= data + len || *p != '"') return false;
    p++;
    const char *end = memchr(p, '"', (data + len) - p);
    if (!end) return false;

    size_t vlen = end - p;
    if (vlen >= dst_sz) vlen = dst_sz - 1;
    memcpy(dst, p, vlen);
    dst[vlen] = '\0';
    return vlen > 0;
}

static void on_event(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    ctx_t *ctx = arg;
    esp_mqtt_event_handle_t e = data;

    switch ((esp_mqtt_event_id_t)id) {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "connected; subscribing to %s", ctx->topic);
        esp_mqtt_client_subscribe(e->client, ctx->topic, 1);
        break;
    case MQTT_EVENT_DATA:
        ESP_LOGI(TAG, "data on %.*s (%d bytes)",
                 e->topic_len, e->topic, e->data_len);
        if (extract_url(e->data, e->data_len, ctx->out->url, sizeof(ctx->out->url))) {
            ESP_LOGI(TAG, "url: %s", ctx->out->url);
            xEventGroupSetBits(ctx->events, BIT_GOT_MSG);
        } else {
            ESP_LOGW(TAG, "payload had no usable url");
        }
        break;
    case MQTT_EVENT_ERROR:
        ESP_LOGE(TAG, "mqtt error");
        xEventGroupSetBits(ctx->events, BIT_FAILED);
        break;
    default:
        break;
    }
}

esp_err_t mqtt_fetch_retained(mqtt_job_t *job)
{
    if (!job) return ESP_ERR_INVALID_ARG;
    memset(job, 0, sizeof(*job));

    mqtt_config_t cfg_nvs;
    mqtt_config_load(&cfg_nvs);

    ctx_t ctx = {
        .events = xEventGroupCreate(),
        .out = job,
        .topic = cfg_nvs.topic,
    };

    esp_mqtt_client_config_t cfg = {
        .broker.address.uri = cfg_nvs.uri,
        .credentials.client_id = MQTT_CLIENT_ID,
        .session.keepalive = 30,
    };
    if (cfg_nvs.user[0]) {
        cfg.credentials.username = cfg_nvs.user;
        cfg.credentials.authentication.password = cfg_nvs.pass;
    }

    esp_mqtt_client_handle_t cli = esp_mqtt_client_init(&cfg);
    if (!cli) { vEventGroupDelete(ctx.events); return ESP_FAIL; }

    esp_mqtt_client_register_event(cli, ESP_EVENT_ANY_ID, on_event, &ctx);
    esp_err_t err = esp_mqtt_client_start(cli);
    if (err != ESP_OK) {
        esp_mqtt_client_destroy(cli);
        vEventGroupDelete(ctx.events);
        return err;
    }

    EventBits_t bits = xEventGroupWaitBits(
        ctx.events, BIT_GOT_MSG | BIT_FAILED,
        pdFALSE, pdFALSE, pdMS_TO_TICKS(MQTT_WAIT_MS));

    esp_mqtt_client_stop(cli);
    esp_mqtt_client_destroy(cli);
    vEventGroupDelete(ctx.events);

    if (bits & BIT_GOT_MSG) return ESP_OK;
    if (bits & BIT_FAILED)  return ESP_FAIL;
    return ESP_ERR_TIMEOUT;
}
