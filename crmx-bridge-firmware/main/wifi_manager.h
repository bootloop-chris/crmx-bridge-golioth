#pragma once

#include "esp_err.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize WiFi manager
 * 
 * @return ESP_OK on success
 */
esp_err_t wifi_manager_init(void);

/**
 * @brief Connect to WiFi using stored credentials
 * 
 * @return ESP_OK on success
 */
esp_err_t wifi_manager_connect(void);

/**
 * @brief Check if WiFi is connected
 * 
 * @return true if connected, false otherwise
 */
bool wifi_manager_is_connected(void);

/**
 * @brief Wait for WiFi connection (blocking)
 * 
 * @param timeout_ms Timeout in milliseconds, 0 for no timeout
 * @return ESP_OK if connected within timeout, ESP_ERR_TIMEOUT otherwise
 */
esp_err_t wifi_manager_wait_for_connection(uint32_t timeout_ms);

/**
 * @brief Set WiFi credentials
 * 
 * @param ssid WiFi SSID
 * @param password WiFi password
 * @return ESP_OK on success
 */
esp_err_t wifi_manager_set_credentials(const char* ssid, const char* password);

/**
 * @brief Get WiFi IP address
 * 
 * @param ip_str Buffer to store IP address string (minimum 16 bytes)
 * @return ESP_OK on success
 */
esp_err_t wifi_manager_get_ip_address(char* ip_str);

#ifdef __cplusplus
}
#endif