
#include "SettingsHandler.h"
#include "esp_log.h"

static SettingsHandler settings_shared{};

namespace SettingsInternal {
bool Infra::init() {
  // Initialize NVS
  bool should_reset = false;
  esp_err_t err = nvs_flash_init();
  if (err == ESP_ERR_NVS_NO_FREE_PAGES ||
      err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    // NVS partition was truncated and needs to be erased
    // Retry nvs_flash_init
    ESP_ERROR_CHECK(nvs_flash_erase());
    err = nvs_flash_init();
    should_reset = true;
  } else if (err != ESP_OK) {
    ESP_LOGE(TAG, "Error (%s) initializing NVS flash!", esp_err_to_name(err));
  }

  // Handle will automatically close when going out of scope or when it's reset.
  nvs_handle = nvs::open_nvs_handle("storage", NVS_READWRITE, &err);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Error (%s) opening NVS handle!", esp_err_to_name(err));
    should_reset = true;
  }
  is_init = true;
  return should_reset;
}
} // namespace SettingsInternal

SettingsHandler &SettingsHandler::shared() {
  if (!settings_shared.is_init) {
    settings_shared.init();
  }
  return settings_shared;
}

void SettingsHandler::init() {

  const bool should_reset = infra.init();

  if (!is_init) {
    delegate_semaphore = xSemaphoreCreateMutex();
    is_init = true;
  }

  // Reset if nvs was uninitialized.
  if (should_reset) {
    output_en.write(output_en.default_val);
    input.write(input.default_val);
    output.write(output.default_val);
    tmo_opt_pwr.write(tmo_opt_pwr.default_val);
    rf_protocol.write(rf_protocol.default_val);
    univ_clr_r.write(univ_clr_r.default_val);
    univ_clr_g.write(univ_clr_g.default_val);
    univ_clr_b.write(univ_clr_b.default_val);
    // device_name.write(device_name.default_val);
  }

  // Read in all the settings.
  read_all();
}

void SettingsHandler::read_all() {
#define READ_SETTING(setting)                                                  \
  {                                                                            \
    const esp_err_t err = setting.read();                                      \
    if (err == ESP_ERR_NVS_NOT_FOUND) {                                        \
      ESP_LOGW(                                                                \
          TAG,                                                                 \
          "Attempted to read %s but key did not exist, setting to default!",   \
          setting.key);                                                        \
      setting.write(setting.default_val);                                      \
    }                                                                          \
  }

  READ_SETTING(output_en);
  READ_SETTING(input);
  READ_SETTING(output);
  READ_SETTING(tmo_opt_pwr);
  READ_SETTING(rf_protocol);
  READ_SETTING(univ_clr_r);
  READ_SETTING(univ_clr_g);
  READ_SETTING(univ_clr_b);
  // READ_SETTING(device_name);

#undef READ_SETTING

  notify_delegates();
}

void SettingsHandler::notify_delegates() {
  if (xSemaphoreTake(delegate_semaphore, pdMS_TO_TICKS(2)) == pdTRUE) {
    for (SettingsChangeDelegate *delegate : change_delegates) {
      if (delegate != nullptr) {
        delegate->on_settings_update(*this);
      }
    }
    xSemaphoreGive(delegate_semaphore);
  } else {
    ESP_LOGE(TAG, "Failed to take delegate semaphore to notify delegates!");
  }
}