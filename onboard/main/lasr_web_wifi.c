#include "lasr_web_wifi.h"
#include "lasr_web_http.h"
#include "bsp/esp-bsp.h"
#include "esp_hosted.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_random.h"
#include "esp_mac.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdio.h>
#include <string.h>

static const char *TAG = "lasr_wifi";
static volatile bool s_ready;

static bool read_or_create_key(char *key, size_t size) {
    nvs_handle_t nvs = 0;
    if (!key || size < 13 ||
        nvs_open("lasr_wifi", NVS_READWRITE, &nvs) != ESP_OK) {
        ESP_LOGE(TAG, "Cannot access private Wi-Fi credentials in NVS");
        return false;
    }
    size_t stored = size;
    esp_err_t err = nvs_get_str(nvs, "ap_password", key, &stored);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        const char alphabet[] = "abcdefghjkmnpqrstuvwxyz23456789";
        unsigned char random_bytes[12] = {0};
        esp_fill_random(random_bytes, sizeof(random_bytes));
        for (size_t i = 0; i < sizeof(random_bytes); ++i) {
            key[i] = alphabet[random_bytes[i] % (sizeof(alphabet) - 1)];
        }
        key[12] = '\0';
        err = nvs_set_str(nvs, "ap_password", key);
        if (err == ESP_OK) err = nvs_commit(nvs);
    }
    nvs_close(nvs);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Unable to securely provision Wi-Fi password: %s",
                 esp_err_to_name(err));
        return false;
    }
    return true;
}

static void ap_event(void *arg, esp_event_base_t event,
                     int32_t id, void *data) {
    (void)arg; (void)data;
    if (event == WIFI_EVENT && id == WIFI_EVENT_AP_START) {
        s_ready = true;
        ESP_LOGI(TAG, "Tab5 LASR hotspot ready. Browser URL: http://192.168.4.1/");
    } else if (event == WIFI_EVENT && id == WIFI_EVENT_AP_STOP) {
        s_ready = false;
        ESP_LOGW(TAG, "Hotspot stopped; camera preview remains local");
    }
}

static void network_task(void *arg) {
    (void)arg;
    do {
        esp_err_t err = bsp_feature_enable(BSP_FEATURE_WIFI, true);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "C6 WLAN_PWR_EN failed: %s", esp_err_to_name(err));
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(250)); // Allow C6 rail to settle.
        err = esp_hosted_init();
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "ESP-Hosted init failed: %s", esp_err_to_name(err));
            break;
        }
        err = esp_hosted_connect_to_slave();
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "C6 firmware incompatible or not responding: %s",
                     esp_err_to_name(err));
            break; // Never attempt C6 reflash automatically.
        }
        err = nvs_flash_init();
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "NVS init failed (%s); never erase NVS automatically",
                     esp_err_to_name(err));
            break;
        }
        char password[20] = {0};
        if (!read_or_create_key(password, sizeof(password))) break;
        uint8_t mac[6] = {0};
        if (esp_efuse_mac_get_default(mac) != ESP_OK) break;
        char ssid[28] = {0};
        if (snprintf(ssid, sizeof(ssid), "Tab5LASR-%02X%02X", mac[4], mac[5]) <= 0)
            break;
        if ((err = esp_netif_init()) != ESP_OK) break;
        err = esp_event_loop_create_default();
        if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) break;
        if (!esp_netif_create_default_wifi_ap()) break;
        wifi_init_config_t config = WIFI_INIT_CONFIG_DEFAULT();
        if ((err = esp_wifi_init(&config)) != ESP_OK) break;
        if ((err = esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                              ap_event, NULL)) != ESP_OK) break;
        wifi_config_t ap_config = {0};
        memcpy(ap_config.ap.ssid, ssid, strlen(ssid));
        memcpy(ap_config.ap.password, password, strlen(password));
        ap_config.ap.ssid_len = strlen(ssid);
        ap_config.ap.authmode = WIFI_AUTH_WPA2_PSK;
        ap_config.ap.max_connection = 1;
        if ((err = esp_wifi_set_mode(WIFI_MODE_AP)) != ESP_OK ||
            (err = esp_wifi_set_config(WIFI_IF_AP, &ap_config)) != ESP_OK ||
            (err = esp_wifi_start()) != ESP_OK) {
            ESP_LOGE(TAG, "C6 AP start failed: %s", esp_err_to_name(err));
            break;
        }
        ESP_LOGI(TAG, "Hotspot SSID=%s password=%s (read once, save securely)",
                 ssid, password);
        if (!lasr_web_http_start()) {
            ESP_LOGE(TAG, "Local HTTP camera viewer failed");
        }
    } while (0);
    vTaskDelete(NULL);
}

bool lasr_web_wifi_start(void) {
    if (xTaskCreate(network_task, "lasr_wifi", 8192, NULL, 3, NULL) != pdPASS) {
        ESP_LOGE(TAG, "Cannot create hosted C6 network task");
        return false;
    }
    return true;
}

bool lasr_web_wifi_ready(void) { return s_ready; }
