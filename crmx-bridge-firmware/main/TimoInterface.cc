#include "TimoInterface.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define INIT_GUARD()                                                           \
  do {                                                                         \
    if (!is_init) {                                                            \
      ESP_LOGE(TAG, "Device is not initialized!");                             \
      return ESP_ERR_INVALID_STATE;                                            \
    }                                                                          \
  } while (false)

static const char *TAG = "TIMO";

using namespace TIMO;

// TODO: rename
bool TimoInterface::is_ready() {
  // Device is ready if IRQ pin is low.
  return is_init && !gpio_get_level(hw_config.nirq_pin);
}

bool TimoInterface::wait_for_ready(const int64_t timeout_ms) {
  int64_t start_time = esp_timer_get_time();
  while (!is_ready()) {
    vTaskDelay(1);
    if (esp_timer_get_time() - start_time > timeout_ms * 1000) {
      return false;
    }
  }
  return true;
}

esp_err_t TimoInterface::send_cmd(const SpiCmd cmd, const uint8_t addr) {

  INIT_GUARD();

  if (addr > TIMO_REG_ADDR_MAX) {
    ESP_LOGE(TAG, "Address %u is larger than the maximum allowed (%u)", addr,
             TIMO_REG_ADDR_MAX);
    return ESP_ERR_INVALID_ARG;
  }

  // Zero out the transaction
  spi_transaction_t t;
  memset(&t, 0, sizeof(t));

  // Command is 8 bits
  t.length = 8;
  t.tx_data[0] = (static_cast<uint8_t>(cmd) | addr);

  // D/C needs to be set to 0
  // TODO: does it??
  t.user = (void *)0;
  t.flags = SPI_TRANS_USE_TXDATA;

  return ESP_ERROR_CHECK_WITHOUT_ABORT(spi_device_transmit(handle, &t));
}

esp_err_t TimoInterface::transact_spi(const bool write, const uint8_t addr,
                                      const size_t len) {
  // Check params
  INIT_GUARD();

  if (len > max_reg_size) {
    ESP_LOGE(TAG, "Requested register size %zu is more than the maximum(%zu)",
             len, max_reg_size);
    return ESP_ERR_INVALID_ARG;
  }

  // Send the command
  esp_err_t ret = ESP_ERROR_CHECK_WITHOUT_ABORT(
      send_cmd(write ? SpiCmd::WRITE_REG : SpiCmd::READ_REG, addr));
  if (ret != ESP_OK) {
    return ret;
  }

  // Wait for device ready for remainder of transaction.
  if (!wait_for_ready(transaction_timeout_ms)) {
    std::string readwrite_str = write ? "write" : "read";
    ESP_LOGE(TAG, "Timed out waiting for %s CMD response, addr 0x%x",
             readwrite_str.c_str(), addr);
    return ESP_ERR_TIMEOUT;
  }

  spi_transaction_t t;
  memset(&t, 0, sizeof(t));
  t.length = 8 * SpiTxRxBuf<max_reg_size>::spi_transaction_size(len);
  t.tx_buffer = tx_buf.raw();
  t.rx_buffer = rx_buf.raw();

  ret = ESP_ERROR_CHECK_WITHOUT_ABORT(spi_device_transmit(handle, &t));
  if (ret != ESP_OK) {
    return ret;
  }

  // TODO: check IRQ SPI device busy flag and return an error if it's high

  return ESP_OK;
}

esp_err_t TimoInterface::init(const spi_host_device_t bus) {
  if (is_init) {
    ESP_LOGE(TAG, "Cannot initialize Timo twice!");
    return ESP_ERR_INVALID_STATE;
  }

  esp_err_t ret;
  ret = ESP_ERROR_CHECK_WITHOUT_ABORT(
      spi_bus_add_device(bus, &hw_config.spi_devcfg, &handle));
  if (ret != ESP_OK) {
    return ret;
  }

  gpio_config_t timo_io_conf = {
      .pin_bit_mask = (1ULL << hw_config.nirq_pin),
      .mode = GPIO_MODE_INPUT,
      .pull_up_en = GPIO_PULLUP_DISABLE,
      .pull_down_en = GPIO_PULLDOWN_DISABLE,
      .intr_type = GPIO_INTR_DISABLE,
  };
  ret = ESP_ERROR_CHECK_WITHOUT_ABORT(gpio_config(&timo_io_conf));
  if (ret != ESP_OK) {
    return ret;
  }

  is_init = true;
  return ESP_OK;
}

esp_err_t TimoInterface::set_sw_config(const TimoSoftwareConfig &_sw_config) {

  INIT_GUARD();

  esp_err_t res = ESP_OK;

  CONFIG config;
  config.set(CONFIG::RADIO_ENABLE, _sw_config.radio_en)
      .set(CONFIG::RADIO_TX_RX_MODE, _sw_config.tx_rx_mode)
      .set(CONFIG::UART_EN, false);
  res = ESP_ERROR_CHECK_WITHOUT_ABORT(write_reg(config));
  if (res != ESP_OK) {
    return res;
  }
  sw_config.radio_en = _sw_config.radio_en;
  sw_config.tx_rx_mode = _sw_config.tx_rx_mode;

  vTaskDelay(pdMS_TO_TICKS(10));

  res = set_rf_protocol(_sw_config.rf_protocol);
  if (res != ESP_OK) {
    return res;
  }

  vTaskDelay(pdMS_TO_TICKS(10));

  res = set_rf_power(_sw_config.rf_power);
  if (res != ESP_OK) {
    return res;
  }

  vTaskDelay(pdMS_TO_TICKS(10));

  res = set_universe_color(_sw_config.universe_color);
  if (res != ESP_OK) {
    return res;
  }

  vTaskDelay(pdMS_TO_TICKS(10));

  res = set_device_name(_sw_config.device_name);
  if (res != ESP_OK) {
    return res;
  }

  vTaskDelay(pdMS_TO_TICKS(10));

  // For now, assert DMX spec to default
  DMX_SPEC dmx_spec;
  dmx_spec.set(DMX_SPEC::N_CHANNELS_MSB, 0x02)
      .set(DMX_SPEC::N_CHANNELS_LSB, 0x0)
      .set(DMX_SPEC::INTERSLOT_TIME_MSB, 0x0)
      .set(DMX_SPEC::INTERSLOT_TIME_LSB, 0x0)
      .set(DMX_SPEC::REFRESH_PERIOD_MSB, 0x0)
      .set(DMX_SPEC::REFRESH_PERIOD_B2, 0x0)
      .set(DMX_SPEC::REFRESH_PERIOD_B1, 0x61)
      .set(DMX_SPEC::REFRESH_PERIOD_LSB, 0xA8);
  res = ESP_ERROR_CHECK_WITHOUT_ABORT(write_reg(dmx_spec));
  if (res != ESP_OK) {
    return res;
  }

  //   vTaskDelay(pdMS_TO_TICKS(10));

  //   res =
  //   ESP_ERROR_CHECK_WITHOUT_ABORT(set_dmx_source(sw_config.dmx_source)); if
  //   (res != ESP_OK) {
  //     return res;
  //   }

  vTaskDelay(pdMS_TO_TICKS(10));

  // Finally, enable internal DMX from SPI
  DMX_CONTROL dmx_control;
  dmx_control.set(DMX_CONTROL::ENABLE, true);
  res = ESP_ERROR_CHECK_WITHOUT_ABORT(write_reg(dmx_control));
  if (res != ESP_OK) {
    return res;
  }

  vTaskDelay(pdMS_TO_TICKS(10));

  return ESP_OK;
}

esp_err_t TimoInterface::set_radio_en(const bool en) {
  INIT_GUARD();

  CONFIG config;
  config.set(CONFIG::RADIO_ENABLE, en)
      .set(CONFIG::RADIO_TX_RX_MODE, sw_config.tx_rx_mode)
      .set(CONFIG::UART_EN, true);
  esp_err_t res = ESP_ERROR_CHECK_WITHOUT_ABORT(write_reg(config));
  if (res != ESP_OK) {
    return res;
  }

  ESP_LOGI(TAG, "Radio en set to %d", static_cast<int>(en));
  sw_config.radio_en = en;
  return ESP_OK;
}

esp_err_t TimoInterface::set_tx_rx_mode(
    const TIMO::CONFIG::RADIO_TX_RX_MODE_T tx_rx_mode) {
  INIT_GUARD();

  CONFIG config;
  config.set(CONFIG::RADIO_ENABLE, sw_config.radio_en)
      .set(CONFIG::RADIO_TX_RX_MODE, tx_rx_mode)
      .set(CONFIG::UART_EN, false);
  esp_err_t res = ESP_ERROR_CHECK_WITHOUT_ABORT(write_reg(config));
  if (res != ESP_OK) {
    return res;
  }

  ESP_LOGI(TAG, "TX/RX mode set to %d", static_cast<int>(tx_rx_mode));
  sw_config.tx_rx_mode = tx_rx_mode;
  return ESP_OK;
}

esp_err_t
TimoInterface::set_rf_power(const TIMO::RF_POWER::OUTPUT_POWER_T pwr) {
  INIT_GUARD();

  RF_POWER reg;
  reg.set(RF_POWER::OUTPUT_POWER, pwr);
  esp_err_t res = ESP_ERROR_CHECK_WITHOUT_ABORT(write_reg(reg));
  if (res != ESP_OK) {
    return res;
  }

  ESP_LOGI(TAG, "RF power set to %d", static_cast<int>(pwr));
  sw_config.rf_power = pwr;
  return ESP_OK;
}

esp_err_t TimoInterface::set_rf_protocol(
    const TIMO::RF_PROTOCOL::TX_PROTOCOL_T protocol) {
  INIT_GUARD();

  RF_PROTOCOL reg;
  reg.set(RF_PROTOCOL::TX_PROTOCOL, protocol);
  esp_err_t res = ESP_ERROR_CHECK_WITHOUT_ABORT(write_reg(reg));
  if (res != ESP_OK) {
    return res;
  }

  ESP_LOGI(TAG, "RF protocol set to %d", static_cast<int>(protocol));
  sw_config.rf_protocol = protocol;
  return ESP_OK;
}

esp_err_t TimoInterface::set_universe_color(const Color color) {
  INIT_GUARD();

  UNIVERSE_COLOR reg;
  reg.set(UNIVERSE_COLOR::RED, color.red)
      .set(UNIVERSE_COLOR::BLUE, color.blue)
      .set(UNIVERSE_COLOR::GREEN, color.green);
  esp_err_t res = ESP_ERROR_CHECK_WITHOUT_ABORT(write_reg(reg));
  if (res != ESP_OK) {
    return res;
  }

  ESP_LOGI(TAG, "Universe color set to 0x%x, 0x%x, 0x%x", color.red,
           color.green, color.blue);
  sw_config.universe_color = color;
  return ESP_OK;
}

esp_err_t TimoInterface::set_device_name(const std::string device_name) {
  INIT_GUARD();

  DEVICE_NAME reg;
  strncpy(reinterpret_cast<char *>(reg.data.data()), device_name.c_str(),
          reg.data.size());
  esp_err_t res = ESP_ERROR_CHECK_WITHOUT_ABORT(write_reg(reg));
  if (res != ESP_OK) {
    return res;
  }

  ESP_LOGI(TAG, "Device name set to '%s'", device_name.c_str());
  sw_config.device_name = device_name;
  return ESP_OK;
}

esp_err_t
TimoInterface::set_dmx_source(const TIMO::DMX_SOURCE::DATA_SOURCE_T source) {
  INIT_GUARD();

  DMX_SOURCE reg;
  reg.set(DMX_SOURCE::DATA_SOURCE, source);
  esp_err_t res = ESP_ERROR_CHECK_WITHOUT_ABORT(write_reg(reg));
  if (res != ESP_OK) {
    return res;
  }

  ESP_LOGI(TAG, "RF protocol set to %d", static_cast<int>(source));
  sw_config.dmx_source = source;
  return ESP_OK;
}

TimoStatus TimoInterface::get_status() {
  if (!is_init) {
    ESP_LOGE(TAG, "Device is not initialized!");
    return TimoStatus{};
  }

  STATUS status;
  esp_err_t res = ESP_ERROR_CHECK_WITHOUT_ABORT(read_reg(status));
  if (res != ESP_OK) {
    return TimoStatus{};
  }

  return TimoStatus{
      .status_valid = true,
      .is_in_update_mode = static_cast<bool>(status.get(STATUS::UPDATE_MODE)),
      .dmx_available = static_cast<bool>(status.get(STATUS::DMX)),
      .rdm_identify_active = static_cast<bool>(status.get(STATUS::IDENTIFY)),
      .rf_link_active = static_cast<bool>(status.get(STATUS::RF_LINK)),
      .rf_linked = static_cast<bool>(status.get(STATUS::LINKED)),
  };
}

esp_err_t
TimoInterface::get_dmx_source(TIMO::DMX_SOURCE::DATA_SOURCE_T &dmx_source) {
  INIT_GUARD();

  DMX_SOURCE reg;
  esp_err_t res = ESP_ERROR_CHECK_WITHOUT_ABORT(read_reg(reg));
  if (res != ESP_OK) {
    return res;
  }
  dmx_source =
      static_cast<DMX_SOURCE::DATA_SOURCE_T>(reg.get(DMX_SOURCE::DATA_SOURCE));

  return ESP_OK;
}

esp_err_t TimoInterface::write_dmx(const std::array<uint8_t, 512> &data) {

  static constexpr size_t block_size = 128;
  static constexpr size_t num_blocks = 4;

  static_assert(
      std::tuple_size_v<std::remove_reference_t<decltype(data)>> / block_size ==
          num_blocks &&
      std::tuple_size_v<std::remove_reference_t<decltype(data)>> % block_size ==
          0);

  // Check params
  INIT_GUARD();

  SpiTxRxBuf<block_size> tx_buf;

  for (size_t block_idx = 0; block_idx < num_blocks; block_idx++) {
    esp_err_t ret = send_cmd(SpiCmd::WRITE_DMX);
    if (ret != ESP_OK) {
      return ret;
    }

    // Wait for device ready for remainder of transaction.
    if (!wait_for_ready(transaction_timeout_ms)) {
      ESP_LOGE(TAG, "Timed out waiting for DMX CMD response");
      return ESP_ERR_TIMEOUT;
    }

    tx_buf.prep_tx();
    memcpy(tx_buf.data.data(), &(data.data()[block_idx * block_size]),
           block_size);

    spi_transaction_t t;
    memset(&t, 0, sizeof(t));
    t.length = 8 * tx_buf.spi_transaction_size(block_size);
    t.tx_buffer = tx_buf.raw();
    t.rx_buffer = nullptr;

    ret = ESP_ERROR_CHECK_WITHOUT_ABORT(spi_device_transmit(handle, &t));
    if (ret != ESP_OK) {
      return ret;
    }

    vTaskDelay(pdMS_TO_TICKS(1));
  }

  return ESP_OK;
}
