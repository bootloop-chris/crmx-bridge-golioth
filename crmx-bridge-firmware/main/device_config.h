#pragma once

// Default WiFi credentials (can be overridden via NVS or Golioth settings)
#define DEFAULT_WIFI_SSID    ""  // Set your default WiFi SSID here
#define DEFAULT_WIFI_PASS    ""  // Set your default WiFi password here

// Default Golioth credentials (can be overridden via NVS)
#define DEFAULT_GOLIOTH_PSK_ID  ""  // Set your Golioth PSK ID here
#define DEFAULT_GOLIOTH_PSK     ""  // Set your Golioth PSK here

// Telemetry reporting interval
#define TELEMETRY_INTERVAL_MS   (30 * 1000)  // 30 seconds

// Enable/disable features
#define ENABLE_GOLIOTH_LOGS     1
#define ENABLE_TELEMETRY        1
#define ENABLE_OTA_UPDATES      1