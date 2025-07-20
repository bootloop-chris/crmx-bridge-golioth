#include "wifi_task.h"
#include "wifi_manager.h"
#include "device_config.h"
#include "SettingsHandler.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "cJSON.h"

static const char* TAG = "wifi_task";

static bool s_wifi_connected = false;
static bool s_golioth_connected = false;
static int64_t s_last_telemetry_time = 0;

extern "C" void wifi_task(void *pvParameters)
{
    ESP_LOGI(TAG, "Starting Wifi task");

    // Initialize WiFi
    ESP_ERROR_CHECK(wifi_manager_init());
    
    // Set WiFi credentials (you may want to load these from NVS or settings)
    if (strlen(DEFAULT_WIFI_SSID) > 0 && strlen(DEFAULT_WIFI_PASS) > 0) {
        wifi_manager_set_credentials(DEFAULT_WIFI_SSID, DEFAULT_WIFI_PASS);
        wifi_manager_connect();
    } else {
        ESP_LOGW(TAG, "WiFi credentials not configured. Please set them via settings or NVS.");
    }

    while (true) {
        // Check WiFi connection status
        bool wifi_connected = wifi_manager_is_connected();
        if (wifi_connected != s_wifi_connected) {
            s_wifi_connected = wifi_connected;
            ESP_LOGI(TAG, "WiFi %s", wifi_connected ? "connected" : "disconnected");
        }

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}