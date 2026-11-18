#include "lasr_web_assets.h"
#include "esp_log.h"
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

static const char *TAG = "lasr_assets";
extern const uint8_t index_html_start[] asm("_binary_index_html_start");
extern const uint8_t index_html_end[] asm("_binary_index_html_end");
extern const uint8_t app_js_start[] asm("_binary_app_js_start");
extern const uint8_t app_js_end[] asm("_binary_app_js_end");
extern const uint8_t model_js_start[] asm("_binary_model_js_start");
extern const uint8_t model_js_end[] asm("_binary_model_js_end");
extern const uint8_t styles_css_start[] asm("_binary_styles_css_start");
extern const uint8_t styles_css_end[] asm("_binary_styles_css_end");

typedef struct {
    const uint8_t *begin, *end;
    const char *mime;
} asset_t;
static const asset_t s_assets[] = {
    {index_html_start, index_html_end, "text/html; charset=utf-8"},
    {app_js_start, app_js_end, "application/javascript; charset=utf-8"},
    {model_js_start, model_js_end, "application/javascript; charset=utf-8"},
    {styles_css_start, styles_css_end, "text/css; charset=utf-8"}
};
static const char *s_routes[] = {"/", "/app.js", "/model.js", "/styles.css"};

static esp_err_t asset_handler(httpd_req_t *req) {
    const asset_t *asset = (const asset_t *)req->user_ctx;
    if (!asset || !asset->begin || asset->end <= asset->begin)
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Asset unavailable");
    size_t size = (size_t)(asset->end - asset->begin);
    if (size && asset->begin[size - 1] == 0) --size; // EMBED_TXTFILES null terminator.
    httpd_resp_set_type(req, asset->mime);
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    httpd_resp_set_hdr(req, "X-Content-Type-Options", "nosniff");
    return httpd_resp_send(req, (const char *)asset->begin, (ssize_t)size);
}

bool lasr_web_assets_register(httpd_handle_t server) {
    if (!server) {
        ESP_LOGE(TAG, "HTTP server handle missing");
        return false;
    }
    for (size_t i = 0; i < sizeof(s_assets) / sizeof(s_assets[0]); ++i) {
        const httpd_uri_t uri = {
            .uri = s_routes[i], .method = HTTP_GET,
            .handler = asset_handler, .user_ctx = (void *)&s_assets[i]
        };
        const esp_err_t err = httpd_register_uri_handler(server, &uri);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Could not serve %s: %s", uri.uri, esp_err_to_name(err));
            return false;
        }
    }
    return true;
}
