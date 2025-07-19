#pragma once

#include "Color.h"
#include "TimoReg.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "nvs_handle.hpp"
#include "util.h"
#include <algorithm>
#include <cstring>
#include <vector>

class SettingsHandler;

namespace SettingsInternal {
static constexpr const char *TAG = "NVS";

class Infra {
public:
  bool init();

  template <typename T> esp_err_t read(const char *const key, T &val) {
    if (!is_init || !nvs_handle) {
      ESP_LOGE(TAG, "Read called before storage is initialized!");
      return ESP_ERR_NVS_INVALID_HANDLE;
    }
    esp_err_t err = nvs_handle->get_item(key, val);

    if (err != ESP_OK) {
      ESP_LOGE(TAG, "Error (%s) reading key %s!\n", esp_err_to_name(err), key);
    }
    return err;
  }

  template <typename T> esp_err_t write(const char *const key, const T val) {
    if (!is_init || !nvs_handle) {
      ESP_LOGE(TAG, "Write called before storage is initialized!");
      return ESP_ERR_NVS_INVALID_HANDLE;
    }
    esp_err_t err = nvs_handle->set_item(key, val);
    if (err != ESP_OK) {
      ESP_LOGE(TAG, "Error (%s) setting key %s!\n", esp_err_to_name(err), key);
      return err;
    }
    err = nvs_handle->commit();
    if (err != ESP_OK) {
      ESP_LOGE(TAG, "Error (%s) committing write to key %s!\n",
               esp_err_to_name(err), key);
    }
    return err;
  }

private:
  bool is_init = false;

  std::unique_ptr<nvs::NVSHandle> nvs_handle;
};
} // namespace SettingsInternal

template <typename _T> struct Setting {
  using T = _T;

  Setting(SettingsHandler &_handler, const char *const _key,
          const T _default_val)
      : key(_key), default_val(_default_val), val(_default_val),
        handler(_handler) {
    assert(strlen(key) < 15);
  }

  esp_err_t read() {
    T val_before = val;
    esp_err_t res = handler.infra.read(key, val);
    if (val_before != val) {
      handler.notify_delegates();
    }
    return res;
  }
  esp_err_t write(const T _val) {
    esp_err_t res = handler.infra.write(key, _val);
    if (res == ESP_OK) {
      val = _val;
    }
    return res;
  }

  T get() const { return val; }
  operator const T &() const { return val; }

  const char *const key;
  const T default_val;

protected:
  T val;
  SettingsHandler &handler;
};

struct SettingsChangeDelegate {
  virtual void on_settings_update(const SettingsHandler &settings) = 0;
};

class SettingsHandler {
  static constexpr const char *output_en_key = "output_en";
  static constexpr const char *input_key = "input";
  static constexpr const char *output_key = "output";
  static constexpr const char *timo_opt_pwr_key = "timo_opt_pwr";
  static constexpr const char *timo_rf_prot_key = "timo_rf_prot";
  static constexpr const char *univ_clr_r_key = "univ_clr_r";
  static constexpr const char *univ_clr_g_key = "univ_clr_g";
  static constexpr const char *univ_clr_b_key = "univ_clr_b";
  static constexpr const char *dev_name_key = "dev_name";

public:
  using RFPowerT = TIMO::RF_POWER::OUTPUT_POWER_T;
  using TxRxT = TIMO::CONFIG::RADIO_TX_RX_MODE_T;
  using RfProtocolT = TIMO::RF_PROTOCOL::TX_PROTOCOL_T;

  SettingsHandler()
      : output_en(*this, output_en_key, false),
        input(*this, input_key, DmxSourceSink::none),
        output(*this, output_key, DmxSourceSink::none),
        tmo_opt_pwr(*this, timo_opt_pwr_key, RFPowerT::PWR_3_MW),
        rf_protocol(*this, timo_rf_prot_key, RfProtocolT::CRMX),
        univ_clr_r(*this, univ_clr_r_key, RGBColor::Red().red),
        univ_clr_g(*this, univ_clr_g_key, RGBColor::Red().green),
        univ_clr_b(*this, univ_clr_b_key, RGBColor::Red().blue)
  /*, device_name(dev_name_key, "CRMXBridge")*/ {}

  static SettingsHandler &shared();

  void read_all();

  esp_err_t add_delegate(SettingsChangeDelegate *delegate) {
    if (delegate == nullptr) {
      return ESP_ERR_INVALID_ARG;
    }

    if (!is_init) {
      ESP_LOGE(TAG, "SettingsHandler is not initialized - could not add "
                    "SettingsChangeDelegate!");
      return ESP_ERR_INVALID_STATE;
    }

    if (xSemaphoreTake(delegate_semaphore, pdMS_TO_TICKS(2)) == pdTRUE) {
      auto iter =
          std::find(change_delegates.begin(), change_delegates.end(), delegate);

      if (iter == change_delegates.end()) {
        change_delegates.push_back(delegate);
      }
      xSemaphoreGive(delegate_semaphore);
    } else {
      ESP_LOGE(TAG, "Could not take delegate change semaphore");
      return ESP_ERR_INVALID_STATE;
    }
    return ESP_OK;
  }

  esp_err_t remove_delegate(SettingsChangeDelegate *delegate) {
    if (!is_init) {
      ESP_LOGE(TAG, "SettingsHandler is not initialized - could not remove "
                    "SettingsChangeDelegate!");
      return ESP_ERR_INVALID_STATE;
    }

    if (xSemaphoreTake(delegate_semaphore, pdMS_TO_TICKS(2)) == pdTRUE) {
      auto iter =
          std::find(change_delegates.begin(), change_delegates.end(), delegate);
      if (iter != change_delegates.end()) {
        change_delegates.erase(iter);
      }
      xSemaphoreGive(delegate_semaphore);
    } else {
      ESP_LOGE(TAG, "Could not take delegate change semaphore");
      return ESP_ERR_INVALID_STATE;
    }
    return ESP_OK;
  }

  // Computed setting values
  TxRxT get_timo_tx_rx() const {
    if (output.get() == DmxSourceSink::timo) {
      return TxRxT::TX;
    }
    return TxRxT::RX;
  }

  bool get_timo_radio_en() const {
    if (output_en.get() && (output.get() == DmxSourceSink::timo ||
                            input.get() == DmxSourceSink::timo)) {
      return true;
    }
    return false;
  }

  RGBColor get_universe_color() const {
    return RGBColor{
        .red = univ_clr_r.get(),
        .green = univ_clr_g.get(),
        .blue = univ_clr_b.get(),
    };
  }

  // I/O settings
  Setting<bool> output_en;
  Setting<DmxSourceSink> input;
  Setting<DmxSourceSink> output;
  // Timo Settings
  Setting<RFPowerT> tmo_opt_pwr;
  Setting<RfProtocolT> rf_protocol;
  Setting<uint8_t> univ_clr_r;
  Setting<uint8_t> univ_clr_g;
  Setting<uint8_t> univ_clr_b;
  // TODO: make blob settings work.
  // Setting<std::string> device_name;

protected:
  static constexpr const char *TAG = "NVS";

  void init();
  void notify_delegates();

  bool is_init = false;

  SettingsInternal::Infra infra;
  std::vector<SettingsChangeDelegate *> change_delegates;
  SemaphoreHandle_t delegate_semaphore;

  friend class Setting<bool>;
  friend class Setting<DmxSourceSink>;
  friend class Setting<RFPowerT>;
  friend class Setting<RfProtocolT>;
  friend class Setting<uint8_t>;
};
