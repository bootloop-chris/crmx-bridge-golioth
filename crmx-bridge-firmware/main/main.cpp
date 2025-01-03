
#include "DmxSwitcher.h"
#include "TimoInterface.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_dmx.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "soc/soc_caps.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const static char *TAG = "EXAMPLE";

static constexpr adc_channel_t pwr_btn_sns_chann = ADC_CHANNEL_3;
static constexpr gpio_num_t pwr_in_ctrl_pin = GPIO_NUM_5;
static constexpr int pwr_btn_press_thresh_mv = 2500;

static constexpr spi_host_device_t timo_spi_bus = SPI2_HOST;
static constexpr spi_bus_config_t spi_bus_cfg = {
    .mosi_io_num = GPIO_NUM_13,
    .miso_io_num = GPIO_NUM_11,
    .sclk_io_num = GPIO_NUM_12,
    .quadwp_io_num = -1,
    .quadhd_io_num = -1,
    .max_transfer_sz = 0,
};

// TODO: adjust queue size
static constexpr TimoHardwareConfig timo_config = {
    .nirq_pin = GPIO_NUM_9,
    .spi_devcfg =
        {
            .mode = 0,                         // SPI mode 0 (CPOL 0, CPHA0)
            .clock_speed_hz = 1 * 1000 * 1000, // 10 MHz
            .spics_io_num = GPIO_NUM_10,       // CS pin
            .queue_size = 7, // 7 transactions queued at a time?
        },
};

TimoInterface timo_interface{timo_config};

//------------- DMX Configs ---------------//

struct dmx_port_config_t {
  dmx_port_t port;
  int tx_pin;
  int rx_pin;
  gpio_num_t de_pin;
  gpio_num_t nre_pin;
};

static constexpr dmx_port_config_t dmx_in_cfg = {
    .port = DMX_NUM_0,
    .tx_pin = 21,
    .rx_pin = 47,
    .de_pin = GPIO_NUM_14,
    .nre_pin = GPIO_NUM_48,
};

static constexpr dmx_port_config_t dmx_out_cfg = {
    .port = DMX_NUM_1,
    .tx_pin = 17,
    .rx_pin = 18,
    .de_pin = GPIO_NUM_16,
    .nre_pin = GPIO_NUM_8,
};

static bool adc_calibration_init(adc_unit_t unit, adc_channel_t channel,
                                 adc_atten_t atten,
                                 adc_cali_handle_t *out_handle);
static void adc_calibration_deinit(adc_cali_handle_t handle);

// --------------------- DMX ------------------------ //

extern "C" void onboard_dmx_task(void *pvParameters) {
  DmxInterface *interface = static_cast<DmxInterface *>(pvParameters);

  if (interface == nullptr) {
    ESP_LOGE(TAG, "Invalid DMX Interface");
    return;
  }

  uint8_t dmx_data[DMX_PACKET_SIZE];
  dmx_packet_t dmx_packet;

  while (true) {
    if (dmx_receive(dmx_in_cfg.port, &dmx_packet, DMX_TIMEOUT_TICK)) {
      if (dmx_packet.err == DMX_OK) {
        size_t data_len = dmx_read(dmx_in_cfg.port, dmx_data, dmx_packet.size);
        printf("DMX Data (%d): ", data_len);
        for (size_t i = 0; i < 8 && i < data_len; i++) {
          printf("%x ", dmx_data[i]);
        }
        printf("\n");
        dmx_write(dmx_out_cfg.port, dmx_data, data_len);
        dmx_send(dmx_out_cfg.port);
        printf("Sent DMX Packet\n");
      } else {
        printf("An error occurred receiving DMX!\n");
      }
    }
    vTaskDelay(pdMS_TO_TICKS(5));
  }
}

void init_dmx(DmxInterface &interface) {
  // First, use the default DMX configuration...
  dmx_config_t dmx_config = DMX_CONFIG_DEFAULT;

  // Install the send and recieve drivers.
  dmx_driver_install(dmx_in_cfg.port, &dmx_config, nullptr, 0);
  dmx_driver_install(dmx_out_cfg.port, &dmx_config, nullptr, 0);

  // Set the pins for both drivers.
  dmx_set_pin(dmx_in_cfg.port, dmx_in_cfg.tx_pin, dmx_in_cfg.rx_pin, -1);
  dmx_set_pin(dmx_out_cfg.port, dmx_out_cfg.tx_pin, dmx_out_cfg.rx_pin, -1);

  // Configure DE/RE pins (true = tx; false = rx).
  ESP_ERROR_CHECK(gpio_set_level(dmx_in_cfg.nre_pin, false));
  ESP_ERROR_CHECK(gpio_set_level(dmx_in_cfg.de_pin, false));
  ESP_ERROR_CHECK(gpio_set_level(dmx_out_cfg.nre_pin, true));
  ESP_ERROR_CHECK(gpio_set_level(dmx_out_cfg.de_pin, true));

  gpio_config_t dmx_io_conf = {
      .pin_bit_mask =
          (1ULL << dmx_in_cfg.de_pin) | (1ULL << dmx_in_cfg.nre_pin) |
          (1ULL << dmx_out_cfg.de_pin) | (1ULL << dmx_out_cfg.nre_pin),
      .mode = GPIO_MODE_OUTPUT,
      .pull_up_en = GPIO_PULLUP_DISABLE,
      .pull_down_en = GPIO_PULLDOWN_DISABLE,
      .intr_type = GPIO_INTR_DISABLE,
  };

  ESP_ERROR_CHECK(gpio_config(&dmx_io_conf));
}

void init_timo(DmxInterface &interface) {
  // Initialize the SPI bus
  ESP_ERROR_CHECK(
      spi_bus_initialize(timo_spi_bus, &spi_bus_cfg, SPI_DMA_CH_AUTO));
  // Add the TIMO to it
  // TODO: don't abort, just log error code.
  ESP_ERROR_CHECK(timo_interface.init(timo_spi_bus));

  using namespace TIMO;
  const TimoSoftwareConfig sw_config = {
      .radio_en = true,
      .tx_rx_mode = CONFIG::RADIO_TX_RX_MODE_T::TX,
      .rf_protocol = RF_PROTOCOL::TX_PROTOCOL_T::CRMX,
      .dmx_source = DMX_SOURCE::DATA_SOURCE_T::NO_DATA,
      .rf_power = RF_POWER::OUTPUT_POWER_T::PWR_40_MW,
      .universe_color = Color::Green(),
      .device_name = "device",
      .universe_name = "universe",
  };
  ESP_ERROR_CHECK(timo_interface.set_sw_config(sw_config));
}

extern "C" void app_main(void) {
  //-------------ADC1 Init---------------//
  adc_oneshot_unit_handle_t adc1_handle;
  adc_oneshot_unit_init_cfg_t init_config1 = {.unit_id = ADC_UNIT_1};
  ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config1, &adc1_handle));

  //-------------ADC1 Config---------------//
  adc_oneshot_chan_cfg_t config = {
      .atten = ADC_ATTEN_DB_12,
      .bitwidth = ADC_BITWIDTH_DEFAULT,
  };
  ESP_ERROR_CHECK(
      adc_oneshot_config_channel(adc1_handle, pwr_btn_sns_chann, &config));

  //-------------ADC1 Calibration Init---------------//
  adc_cali_handle_t adc1_cali_chan0_handle = NULL;
  bool do_calibration1_chan0 = adc_calibration_init(
      ADC_UNIT_1, pwr_btn_sns_chann, config.atten, &adc1_cali_chan0_handle);

  // Power pin config
  gpio_config_t io_conf = {
      .pin_bit_mask = (1ULL << pwr_in_ctrl_pin),
      .mode = GPIO_MODE_OUTPUT,
      .pull_up_en = GPIO_PULLUP_DISABLE,
      .pull_down_en = GPIO_PULLDOWN_DISABLE,
      .intr_type = GPIO_INTR_DISABLE,
  };

  ESP_ERROR_CHECK(gpio_config(&io_conf));

  // Hold up the power rail
  gpio_set_level(pwr_in_ctrl_pin, true);

  // Init DMX Switcher before initializing any sources or sinks.
  DmxSwitcher &switcher = DmxSwitcher::get_switcher();
  ESP_ERROR_CHECK(switcher.init());

  init_dmx(switcher.get_onboard_interface());
  init_timo(switcher.get_timo_interface());

  uint64_t pwr_btn_press_start = 0;
  bool pwr_btn_released = false;

  while (1) {
    int adc_raw = 0;
    int adc_volts = 0;
    // ESP_ERROR_CHECK(adc_oneshot_read(adc1_handle, pwr_btn_sns_chann,
    // &adc_raw)); if (do_calibration1_chan0) {
    //   ESP_ERROR_CHECK(
    //       adc_cali_raw_to_voltage(adc1_cali_chan0_handle, adc_raw,
    //       &adc_volts));
    //   if (adc_volts < pwr_btn_press_thresh_mv) {
    //     pwr_btn_released = true;
    //   }
    //   if (!pwr_btn_released || adc_volts < pwr_btn_press_thresh_mv) {
    //     pwr_btn_press_start = esp_timer_get_time();
    //   }

    //   if (esp_timer_get_time() - pwr_btn_press_start > 1000000) {
    //     ESP_LOGI(TAG, "Power button press detected, powering down.");
    //     vTaskDelay(pdMS_TO_TICKS(1000));
    //     gpio_set_level(pwr_in_ctrl_pin, false);
    //     vTaskDelay(portMAX_DELAY);
    //   }
    // }

    /* Loopback - RX on in, TX on out. */
    if (dmx_receive(dmx_in_cfg.port, &dmx_packet, DMX_TIMEOUT_TICK)) {
      if (dmx_packet.err == DMX_OK) {
        size_t data_len = dmx_read(dmx_in_cfg.port, dmx_data, dmx_packet.size);
        printf("DMX Data (%d): ", data_len);
        for (size_t i = 0; i < 8 && i < data_len; i++) {
          printf("%x ", dmx_data[i]);
        }
        printf("\n");
        dmx_write(dmx_out_cfg.port, dmx_data, data_len);
        dmx_send(dmx_out_cfg.port);
        printf("Sent DMX Packet\n");
      } else {
        printf("An error occurred receiving DMX!\n");
      }
    }

    vTaskDelay(pdMS_TO_TICKS(1));
  }

  // Tear Down
  ESP_ERROR_CHECK(adc_oneshot_del_unit(adc1_handle));
  if (do_calibration1_chan0) {
    adc_calibration_deinit(adc1_cali_chan0_handle);
  }
}

/*---------------------------------------------------------------
        ADC Calibration
---------------------------------------------------------------*/
static bool adc_calibration_init(adc_unit_t unit, adc_channel_t channel,
                                 adc_atten_t atten,
                                 adc_cali_handle_t *out_handle) {
  adc_cali_handle_t handle = NULL;
  esp_err_t ret = ESP_FAIL;
  bool calibrated = false;

#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
  if (!calibrated) {
    ESP_LOGI(TAG, "calibration scheme version is %s", "Curve Fitting");
    adc_cali_curve_fitting_config_t cali_config = {
        .unit_id = unit,
        .chan = channel,
        .atten = atten,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    ret = adc_cali_create_scheme_curve_fitting(&cali_config, &handle);
    if (ret == ESP_OK) {
      calibrated = true;
    }
  }
#endif

#if ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED
  if (!calibrated) {
    ESP_LOGI(TAG, "calibration scheme version is %s", "Line Fitting");
    adc_cali_line_fitting_config_t cali_config = {
        .unit_id = unit,
        .atten = atten,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    ret = adc_cali_create_scheme_line_fitting(&cali_config, &handle);
    if (ret == ESP_OK) {
      calibrated = true;
    }
  }
#endif

  *out_handle = handle;
  if (ret == ESP_OK) {
    ESP_LOGI(TAG, "Calibration Success");
  } else if (ret == ESP_ERR_NOT_SUPPORTED || !calibrated) {
    ESP_LOGW(TAG, "eFuse not burnt, skip software calibration");
  } else {
    ESP_LOGE(TAG, "Invalid arg or no memory");
  }

  return calibrated;
}

static void adc_calibration_deinit(adc_cali_handle_t handle) {
#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
  ESP_LOGI(TAG, "deregister %s calibration scheme", "Curve Fitting");
  ESP_ERROR_CHECK(adc_cali_delete_scheme_curve_fitting(handle));

#elif ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED
  ESP_LOGI(TAG, "deregister %s calibration scheme", "Line Fitting");
  ESP_ERROR_CHECK(adc_cali_delete_scheme_line_fitting(handle));
#endif
}