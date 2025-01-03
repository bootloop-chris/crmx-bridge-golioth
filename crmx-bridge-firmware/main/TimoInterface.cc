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
    ESP_LOGE(TAG, "Timed out waiting for read register CMD response");
    return ESP_ERR_TIMEOUT;
  }

  spi_transaction_t t;
  memset(&t, 0, sizeof(t));
  t.length = 8 * SpiTxRxBuf::spi_transaction_size(len);
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
      .set(CONFIG::UART_EN, true);
  res = ESP_ERROR_CHECK_WITHOUT_ABORT(write_reg(config));
  if (res != ESP_OK) {
    return res;
  }
  sw_config.radio_en = _sw_config.radio_en;
  sw_config.tx_rx_mode = _sw_config.tx_rx_mode;

  res = set_rf_protocol(_sw_config.rf_protocol);
  if (res != ESP_OK) {
    return res;
  }

  res = set_dmx_source(_sw_config.dmx_source);
  if (res != ESP_OK) {
    return res;
  }

  res = set_rf_power(_sw_config.rf_power);
  if (res != ESP_OK) {
    return res;
  }

  res = set_universe_color(_sw_config.universe_color);
  if (res != ESP_OK) {
    return res;
  }

  res = set_device_name(_sw_config.device_name);
  if (res != ESP_OK) {
    return res;
  }

  res = set_universe_name(_sw_config.universe_name);
  if (res != ESP_OK) {
    return res;
  }

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
  sw_config.radio_en = en;
  return ESP_OK;
}

esp_err_t TimoInterface::set_tx_rx_mode(
    const TIMO::CONFIG::RADIO_TX_RX_MODE_T tx_rx_mode) {
  INIT_GUARD();

  CONFIG config;
  config.set(CONFIG::RADIO_ENABLE, sw_config.radio_en)
      .set(CONFIG::RADIO_TX_RX_MODE, tx_rx_mode)
      .set(CONFIG::UART_EN, true);
  esp_err_t res = ESP_ERROR_CHECK_WITHOUT_ABORT(write_reg(config));
  if (res != ESP_OK) {
    return res;
  }
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
  sw_config.rf_protocol = protocol;
  return ESP_OK;
}

esp_err_t TimoInterface::set_dmx_source(
    const TIMO::DMX_SOURCE::DATA_SOURCE_T dmx_source) {
  INIT_GUARD();

  DMX_SOURCE reg;
  reg.set(DMX_SOURCE::DATA_SOURCE, dmx_source);
  esp_err_t res = ESP_ERROR_CHECK_WITHOUT_ABORT(write_reg(reg));
  if (res != ESP_OK) {
    return res;
  }
  sw_config.dmx_source = dmx_source;
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
  sw_config.device_name = device_name;
  return ESP_OK;
}

esp_err_t TimoInterface::set_universe_name(const std::string universe_name) {
  INIT_GUARD();

  UNIVERSE_NAME reg;
  strncpy(reinterpret_cast<char *>(reg.data.data()), universe_name.c_str(),
          reg.data.size());
  esp_err_t res = ESP_ERROR_CHECK_WITHOUT_ABORT(write_reg(reg));
  if (res != ESP_OK) {
    return res;
  }
  sw_config.universe_name = universe_name;
  return ESP_OK;
}
