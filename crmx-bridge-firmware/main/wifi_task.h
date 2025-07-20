#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Main Golioth integration task
 * 
 * Handles WiFi connection, Golioth client management, and telemetry reporting
 * 
 * @param pvParameters Task parameters (unused)
 */
void wifi_task(void *pvParameters);

#ifdef __cplusplus
}
#endif