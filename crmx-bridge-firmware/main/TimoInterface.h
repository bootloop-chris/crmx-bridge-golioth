#pragma once

#include "Color.h"
#include "TimoReg.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_log.h"
#include <array>
#include <cstring>
#include <string>

struct TimoHardwareConfig {
  gpio_num_t nirq_pin;
  spi_device_interface_config_t spi_devcfg;
};

struct TimoSoftwareConfig {
  bool radio_en;
  TIMO::CONFIG::RADIO_TX_RX_MODE_T tx_rx_mode;
  TIMO::RF_PROTOCOL::TX_PROTOCOL_T rf_protocol;
  TIMO::DMX_SOURCE::DATA_SOURCE_T dmx_source;
  TIMO::RF_POWER::OUTPUT_POWER_T rf_power;
  RGBColor universe_color;
  std::string device_name;
};

struct TimoStatus {
  bool status_valid;
  bool is_in_update_mode;
  bool dmx_available;
  bool rdm_identify_active;
  bool rf_link_active;
  bool rf_linked;

  void print_status() {
    printf("TIMO Status:\n"
           "         status valid: %d\n"
           "       In update mode: %d\n"
           "        DMX available: %d\n"
           "  RDM identify active: %d\n"
           "       RF link active: %d\n"
           "            RF linked: %d\n",
           status_valid, is_in_update_mode, dmx_available, rdm_identify_active,
           rf_link_active, rf_linked);
  }
};

class TimoInterface {
protected:
  // Device name register is 32 bytes
  static constexpr size_t max_reg_size = 32;

  static constexpr int64_t transaction_timeout_ms = 100;

  template <size_t data_size> struct __attribute__((packed)) SpiTxRxBuf {
    uint8_t irq_flags;
    std::array<uint8_t, data_size> data;

    uint8_t *raw() { return reinterpret_cast<uint8_t *>(this); }
    void clear() {
      irq_flags = 0;
      memset(data.data(), 0, data_size);
    }
    void prep_tx() { irq_flags = 0xFF; }
    static constexpr size_t spi_transaction_size(const size_t data_len) {
      return (data_len > data_size ? data_size : data_len) + 1;
    }
  };

  // clang-format off
  enum class SpiCmd : uint8_t {
    READ_REG =                0b00000000, // Read from a register. 6 LSB = register address.
    WRITE_REG =               0b01000000, // Write to a register. 6 LSB = register address.
    READ_DMX =                0b10000001, // Read the latest received DMX values from the window set up by the DMX_WINDOW register.
    READ_ASC =                0b10000010, // Read the latest received ASC frame.
    READ_RDM =                0b10000011, // Read the received RDM request (RX mode) or response (TX mode).
    READ_RXTX =               0b10000100, // Read available RXTX interface data.
    WRITE_DMX =               0b10010001, // Write DMX to the internal DMX generation buffer.
    WRITE_RDM =               0b10010010, // Write an RDM response (RX mode) or RDM request (TX mode).
    WRITE_RXTX =              0b10010011, // Write RXTX interface data.
    RADIO_DISCOVERY =         0b10100000, // Write Radio DUB command.
    RADIO_DISCOVERY_RESULT =  0b10100001, // Read Radio DUB result.
    RADIO_MUTE =              0b10100010, // Write Radio mute command.
    RADIO_MUTE_RESULT =       0b10100011, // Read Radio mute response.
    RDM_DISCOVERY =           0b10100100, // Write RDM DUB command.
    RDM_DISCOVERY_RESULT =    0b10100101, // Read RDM DUB result.
    NODE_QUERY =              0b10100110, // Write radio node query command.
    NODE_QUERY_RESPONSE =     0b10100111, // Read radio node query response.
    NOP =                     0b11111111, // No operation. Can be used as a shortcut to read the IRQ_FLAGS register.
  };
  // clang-format on

public:
  TimoInterface(const TimoHardwareConfig &_hw_config)
      : hw_config(_hw_config), handle(nullptr), is_init(false), tx_buf(),
        rx_buf() {}

  esp_err_t init(const spi_host_device_t bus);

  // Config functions
  esp_err_t set_sw_config(const TimoSoftwareConfig &_sw_config);
  const TimoSoftwareConfig &get_sw_config() const { return sw_config; }

  esp_err_t set_radio_en(const bool en);
  bool get_radio_en() const { return sw_config.radio_en; }
  esp_err_t set_rf_power(const TIMO::RF_POWER::OUTPUT_POWER_T pwr);
  TIMO::RF_POWER::OUTPUT_POWER_T get_rf_power() const {
    return sw_config.rf_power;
  }
  esp_err_t set_rf_protocol(const TIMO::RF_PROTOCOL::TX_PROTOCOL_T protocol);
  TIMO::RF_PROTOCOL::TX_PROTOCOL_T get_rf_protocol() const {
    return sw_config.rf_protocol;
  }
  esp_err_t set_tx_rx_mode(const TIMO::CONFIG::RADIO_TX_RX_MODE_T tx_rx_mode);
  TIMO::CONFIG::RADIO_TX_RX_MODE_T get_tx_rx_mode() const {
    return sw_config.tx_rx_mode;
  }
  esp_err_t set_universe_color(const RGBColor color);
  RGBColor get_universe_color() const { return sw_config.universe_color; }
  esp_err_t set_device_name(const std::string device_name);
  std::string get_device_name() const { return sw_config.device_name; }
  esp_err_t set_dmx_source(const TIMO::DMX_SOURCE::DATA_SOURCE_T source);

  // Status functions
  esp_err_t get_dmx_source(TIMO::DMX_SOURCE::DATA_SOURCE_T &source);
  TimoStatus get_status();

  // DMX functions
  esp_err_t write_dmx(const std::array<uint8_t, 512> &data);

protected:
  bool is_ready();
  bool wait_for_ready(const int64_t timeout_ms);
  esp_err_t send_cmd(const SpiCmd cmd, const uint8_t addr = 0);
  esp_err_t transact_spi(const bool write, const uint8_t addr,
                         const size_t len);

  template <uint8_t addr, typename T, size_t len>
  esp_err_t write_reg(const Register<addr, T, len> &reg,
                      const bool verify = true, TickType_t verify_delay = pdMS_TO_TICKS(10)) {

    static_assert(pdMS_TO_TICKS(10) > 2, "Default delay should be more than 1 tick");
    memcpy(tx_buf.data.data(), reg.raw(), reg.raw_len());
    tx_buf.prep_tx();
    rx_buf.clear();
    esp_err_t res = ESP_ERROR_CHECK_WITHOUT_ABORT(
        transact_spi(true, reg.address, reg.raw_len()));
    if (res != ESP_OK) {
      return res;
    }

    if (verify) {
      vTaskDelay(verify_delay);
      Register<addr, T, len> readback;
      esp_err_t res = read_reg(readback);
      if (res != ESP_OK || reg != readback) {
        ESP_LOGE("TIMO", "Register %x verify failed: %x != %x", addr,
                 reg.data[0], readback.data[0]);
        return ESP_FAIL;
      }
    }

    return ESP_OK;
  }

  template <uint8_t addr, typename T, size_t len>
  esp_err_t read_reg(Register<addr, T, len> &reg) {
    // If we're not writing, clear the tx buffer
    tx_buf.clear();
    tx_buf.prep_tx();
    rx_buf.clear();
    esp_err_t res = ESP_ERROR_CHECK_WITHOUT_ABORT(
        transact_spi(false, reg.address, reg.raw_len()));
    if (res != ESP_OK) {
      return res;
    }
    memcpy(reg.data.data(), rx_buf.data.data(), reg.raw_len());
    return ESP_OK;
  }

  const TimoHardwareConfig &hw_config;
  TimoSoftwareConfig sw_config;
  spi_device_handle_t handle;
  bool is_init;
  SpiTxRxBuf<max_reg_size> tx_buf;
  SpiTxRxBuf<max_reg_size> rx_buf;
};

namespace {
class Test : TimoInterface {
  static_assert(sizeof(SpiTxRxBuf<max_reg_size>) == max_reg_size + 1);
  static_assert(sizeof(SpiTxRxBuf<max_reg_size>) ==
                SpiTxRxBuf<max_reg_size>::spi_transaction_size(max_reg_size));
};
} // namespace
