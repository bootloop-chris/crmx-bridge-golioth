#include "wifi_task.h"
#include "wifi_manager.h"
#include "device_config.h"
#include "SettingsHandler.h"
#include "DmxSwitcher.h"
#include "golioth_nvs.h"
#include "golioth_credentials.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "cJSON.h"
#include <golioth/client.h>
#include <golioth/rpc.h>
#include <string.h>

static const char* TAG = "wifi_task";

static bool s_wifi_connected = false;
static bool s_golioth_connected = false;
static int64_t s_last_telemetry_time = 0;

// Golioth client and RPC globals
static struct golioth_client *s_client = nullptr;
static struct golioth_rpc *s_rpc = nullptr;
static SemaphoreHandle_t s_golioth_connected_sem = nullptr;

// RPC callback for setting DMX values
static enum golioth_rpc_status on_set_dmx_channel(zcbor_state_t *request_params_array,
                                                   zcbor_state_t *response_detail_map,
                                                   void *callback_arg)
{
    double dmx_address_double, value_double;
    bool ok;

    // Decode the two double parameters: address and value
    ok = zcbor_float_decode(request_params_array, &dmx_address_double)
        && zcbor_float_decode(request_params_array, &value_double);
    if (!ok)
    {
        ESP_LOGE(TAG, "RPC: Failed to decode DMX parameters");
        return GOLIOTH_RPC_INVALID_ARGUMENT;
    }

    // Floor the doubles to integers as requested
    int dmx_address = (int)dmx_address_double;
    int value = (int)value_double;

    ESP_LOGI(TAG, "RPC: Set DMX channel %d to value %d", dmx_address, value);

    // Call the DMX switcher to set the value
    DmxSwitcher &switcher = DmxSwitcher::get_switcher();
    esp_err_t err = switcher.set_dmx_value(dmx_address, value);
    
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "RPC: Failed to set DMX value: %s", esp_err_to_name(err));
        return GOLIOTH_RPC_INVALID_ARGUMENT;
    }

    // Return success status in response
    ok = zcbor_tstr_put_lit(response_detail_map, "status")
        && zcbor_tstr_put_lit(response_detail_map, "success")
        && zcbor_tstr_put_lit(response_detail_map, "address")
        && zcbor_int32_put(response_detail_map, dmx_address)
        && zcbor_tstr_put_lit(response_detail_map, "value")
        && zcbor_int32_put(response_detail_map, value);
    if (!ok)
    {
        ESP_LOGE(TAG, "RPC: Failed to encode response");
        return GOLIOTH_RPC_RESOURCE_EXHAUSTED;
    }

    return GOLIOTH_RPC_OK;
}

// Golioth client event callback
static void on_client_event(struct golioth_client *client,
                            enum golioth_client_event event,
                            void *arg)
{
    bool is_connected = (event == GOLIOTH_CLIENT_EVENT_CONNECTED);
    if (is_connected && s_golioth_connected_sem)
    {
        xSemaphoreGive(s_golioth_connected_sem);
    }
    s_golioth_connected = is_connected;
    ESP_LOGI(TAG, "Golioth client %s", is_connected ? "connected" : "disconnected");
}

extern "C" void wifi_task(void *pvParameters)
{
    ESP_LOGI(TAG, "Starting Wifi task with Golioth support");

    // Initialize Golioth NVS
    golioth_nvs_init();
    
    // Initialize WiFi
    ESP_ERROR_CHECK(wifi_manager_init());
    
    // Determine WiFi credentials (NVS first, then defaults)
    const char *wifi_ssid = nvs_read_wifi_ssid();
    const char *wifi_pass = nvs_read_wifi_password();
    
    // Try defaults if NVS doesn't have them
    if (strcmp(wifi_ssid, NVS_DEFAULT_STR) == 0 && strlen(DEFAULT_WIFI_SSID) > 0) {
        wifi_ssid = DEFAULT_WIFI_SSID;
    }
    if (strcmp(wifi_pass, NVS_DEFAULT_STR) == 0 && strlen(DEFAULT_WIFI_PASS) > 0) {
        wifi_pass = DEFAULT_WIFI_PASS;
    }
    
    // Set WiFi credentials and connect
    if (strcmp(wifi_ssid, NVS_DEFAULT_STR) != 0 && strcmp(wifi_pass, NVS_DEFAULT_STR) != 0) {
        wifi_manager_set_credentials(wifi_ssid, wifi_pass);
        wifi_manager_connect();
        
        // Wait for WiFi to connect before proceeding with Golioth
        while (!wifi_manager_is_connected()) {
            ESP_LOGI(TAG, "Waiting for WiFi connection...");
            vTaskDelay(pdMS_TO_TICKS(2000));
        }
        s_wifi_connected = true;
        ESP_LOGI(TAG, "WiFi connected!");
        
        // Initialize Golioth client
        const struct golioth_client_config *config = golioth_sample_credentials_get();
        if (config && config->credentials.psk.psk_id_len > 0 && config->credentials.psk.psk_len > 0) {
            s_client = golioth_client_create(config);
            if (s_client) {
                ESP_LOGI(TAG, "Golioth client created successfully");
                
                s_golioth_connected_sem = xSemaphoreCreateBinary();
                
                // Initialize RPC
                s_rpc = golioth_rpc_init(s_client);
                if (s_rpc) {
                    ESP_LOGI(TAG, "Golioth RPC initialized");
                    
                    // Register event callback
                    golioth_client_register_event_callback(s_client, on_client_event, NULL);
                    
                    // Wait for Golioth connection
                    ESP_LOGI(TAG, "Waiting for Golioth connection...");
                    if (xSemaphoreTake(s_golioth_connected_sem, pdMS_TO_TICKS(30000)) == pdTRUE) {
                        ESP_LOGI(TAG, "Connected to Golioth!");
                        
                        // Register the DMX RPC
                        int err = golioth_rpc_register(s_rpc, "set_dmx_channel", on_set_dmx_channel, NULL);
                        if (err == 0) {
                            ESP_LOGI(TAG, "DMX RPC 'set_dmx_channel' successfully registered");
                        } else {
                            ESP_LOGE(TAG, "Failed to register DMX RPC: %d", err);
                        }
                    } else {
                        ESP_LOGW(TAG, "Failed to connect to Golioth within timeout");
                    }
                } else {
                    ESP_LOGE(TAG, "Failed to initialize Golioth RPC");
                }
            } else {
                ESP_LOGE(TAG, "Failed to create Golioth client");
            }
        } else {
            ESP_LOGW(TAG, "Golioth credentials not configured. RPC will not be available.");
        }
    } else {
        ESP_LOGW(TAG, "WiFi credentials not configured. Please set them via NVS or device_config.h");
    }

    while (true) {
        // Check WiFi connection status
        bool wifi_connected = wifi_manager_is_connected();
        if (wifi_connected != s_wifi_connected) {
            s_wifi_connected = wifi_connected;
            ESP_LOGI(TAG, "WiFi %s", wifi_connected ? "connected" : "disconnected");
        }
        
        // Log connection status periodically
        static int status_counter = 0;
        if (++status_counter >= 30) { // Every 30 seconds
            status_counter = 0;
            ESP_LOGI(TAG, "Status: WiFi=%s, Golioth=%s", 
                     s_wifi_connected ? "OK" : "DISCONNECTED",
                     s_golioth_connected ? "OK" : "DISCONNECTED");
        }

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}