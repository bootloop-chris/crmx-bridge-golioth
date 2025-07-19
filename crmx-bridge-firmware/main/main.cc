
#include "DmxSwitcher.h"
#include "SettingsHandler.h"
#include "TimoInterface.h"
#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "driver/ledc.h"
#include "driver/spi_master.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_dmx.h"
#include "esp_err.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_log.h"
#include "esp_lvgl_port.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "limits.h"
#include "lvgl.h"
#include "rotary_encoder.h"
#include "soc/soc_caps.h"
#include "ssd1106.h"
#include "ui/ui.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const static char *TAG = "main";

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
            .mode = 0, // SPI mode 0 (CPOL 0, CPHA0)
            .cs_ena_pretrans = 6,
            .clock_speed_hz = 1 * 1000 * 1000, // 1 MHz
            .spics_io_num = GPIO_NUM_10,       // CS pin
            .queue_size = 7, // 7 transactions queued at a time?
        },
};

TimoInterface timo_interface{timo_config};

static constexpr gpio_num_t led_r_pin = GPIO_NUM_15;
static constexpr gpio_num_t led_g_pin = GPIO_NUM_7;
static constexpr gpio_num_t led_b_pin = GPIO_NUM_6;
static constexpr gpio_num_t btn_ok_pin = GPIO_NUM_35;
static constexpr gpio_num_t btn_bk_pin = GPIO_NUM_36;
static constexpr gpio_num_t btn_enc_pin = GPIO_NUM_41;
static constexpr gpio_num_t enc_a_pin = GPIO_NUM_37;
static constexpr gpio_num_t enc_b_pin = GPIO_NUM_38;

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

static void set_led_color(const RGBColor &color) {
  ledc_set_duty_and_update(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, color.red, 0);
  ledc_set_duty_and_update(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1, color.green, 0);
  ledc_set_duty_and_update(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_2, color.blue, 0);
}

// --------------------- DMX ------------------------ //

extern "C" void onboard_dmx_task(void *pvParameters) {
  DmxInterface *interface = static_cast<DmxInterface *>(pvParameters);

  if (interface == nullptr) {
    ESP_LOGE(TAG, "Invalid DMX Interface");
    return;
  }

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

  dmx_packet_t rx_meta;

  DmxPacket tx_packet;
  DmxPacket rx_packet;

  ESP_LOGI(TAG, "Finished Onboard DMX Init");

  while (true) {
    if (dmx_receive(dmx_in_cfg.port, &rx_meta, DMX_TIMEOUT_TICK)) {
      if (rx_meta.err != DMX_OK ||
          rx_meta.size > sizeof(rx_packet.full_packet)) {
        ESP_LOGE(TAG, "DMX Packet Error: %d, %zu", rx_meta.err, rx_meta.size);
      } else {
        size_t data_len = dmx_read(dmx_in_cfg.port, &rx_packet.full_packet,
                                   rx_packet.full_packet.size());
        if (data_len == rx_packet.full_packet.size()) {
          interface->send(rx_packet);
        } else {
          ESP_LOGE(TAG, "Recieved short DMX packet: %zu", data_len);
        }
      }
    }

    if (interface->recieve(tx_packet, 0)) {
      size_t written_len = dmx_write(dmx_out_cfg.port, &tx_packet.full_packet,
                                     tx_packet.full_packet.size());
      if (written_len != tx_packet.full_packet.size()) {
        ESP_LOGE(TAG, "Wrote short DMX Packet: %zu", written_len);
      }
      written_len = dmx_send(dmx_out_cfg.port);
      if (written_len != tx_packet.full_packet.size()) {
        ESP_LOGE(TAG, "Sent short DMX Packet: %zu", written_len);
      }
    }
    vTaskDelay(pdMS_TO_TICKS(2));
  }
}

class TaskNotifySettingsDelegate : public SettingsChangeDelegate {
public:
  TaskNotifySettingsDelegate(TaskHandle_t _task_handle)
      : task_handle(_task_handle) {}

  void on_settings_update(const SettingsHandler &settings) override {
    if (task_handle != nullptr) {
      xTaskNotify(task_handle, true, eSetValueWithOverwrite);
    }
  }

private:
  TaskHandle_t task_handle;
};

static TimoSoftwareConfig timo_config_from_settings() {
  SettingsHandler &settings = SettingsHandler::shared();

  using namespace TIMO;
  // return TimoSoftwareConfig{
  //     .radio_en = settings.get_timo_radio_en(),
  //     .tx_rx_mode = settings.get_timo_tx_rx(),
  //     .rf_protocol = settings.rf_protocol,
  //     .dmx_source = DMX_SOURCE::DATA_SOURCE_T::NO_DATA,
  //     .rf_power = settings.tmo_opt_pwr,
  //     .universe_color = settings.get_universe_color(),
  //     // TODO: make device name setting work.
  //     // .device_name = settings.device_name,
  //     .device_name = "CRMXBridge",
  // };
  return TimoSoftwareConfig{
    .radio_en = true,
    .tx_rx_mode = CONFIG::RADIO_TX_RX_MODE_T::TX,
    .rf_protocol = RF_PROTOCOL::TX_PROTOCOL_T::CRMX,
    .dmx_source = DMX_SOURCE::DATA_SOURCE_T::NO_DATA,
    .rf_power = RF_POWER::OUTPUT_POWER_T::PWR_3_MW,
    .universe_color = RGBColor::Red(),
    .device_name = "CRMXGolioth",
  };
}

extern "C" void timo_dmx_task(void *pvParameters) {
  DmxInterface *interface = static_cast<DmxInterface *>(pvParameters);

  if (interface == nullptr) {
    ESP_LOGE(TAG, "Invalid DMX Interface");
    return;
  }

  // Give TIMO a while to power on.
  vTaskDelay(pdMS_TO_TICKS(2000));

  // Initialize the SPI bus
  ESP_ERROR_CHECK(
      spi_bus_initialize(timo_spi_bus, &spi_bus_cfg, SPI_DMA_CH_AUTO));
  ESP_ERROR_CHECK(timo_interface.init(timo_spi_bus));

  // Settings are user-generated and therefore fallable. Allow the things to
  // boot even if setting settings fails.
  ESP_ERROR_CHECK_WITHOUT_ABORT(
      timo_interface.set_sw_config(timo_config_from_settings()));

  ESP_LOGI(TAG, "Finished Timo Init");

  int64_t last_print = esp_timer_get_time();

  TickType_t xLastWakeTime;
  DmxPacket packet;

  while (true) {
    if (interface->recieve(packet, pdMS_TO_TICKS(5))) {
      if (timo_interface.write_dmx(packet.full_packet.data) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to write dmx from source %d",
                 static_cast<int>(packet.source));
      }
    }

    // TODO: Timo RX

    // If a notification was recieved, a settings update occurred.
    if (xTaskNotifyWait(0b1, 0, NULL, 0) == pdTRUE) {
      TimoSoftwareConfig new_config = timo_config_from_settings();
      if (new_config != timo_interface.get_sw_config()) {
        timo_interface.set_sw_config(new_config);
      }

      // TODO: move this elsewhere
      set_led_color(SettingsHandler::shared().get_universe_color());
    }

    vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(2));
  }
}

void init_led() {

  static constexpr auto SPEED_MODE = LEDC_LOW_SPEED_MODE;
  static constexpr auto TIMER = LEDC_TIMER_0;

  // Prepare and then apply the LEDC PWM timer configuration
  ledc_timer_config_t ledc_timer = {
      .speed_mode = SPEED_MODE,
      .duty_resolution = LEDC_TIMER_8_BIT,
      .timer_num = TIMER,
      .freq_hz = 4000, // Set output frequency at 4 kHz
      .clk_cfg = LEDC_AUTO_CLK,
  };
  ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer));

  static constexpr std::array<gpio_num_t, 3> led_pins = {
      {led_r_pin, led_g_pin, led_b_pin}};
  static constexpr std::array<ledc_channel_t, 3> led_channels = {
      {LEDC_CHANNEL_0, LEDC_CHANNEL_1, LEDC_CHANNEL_2}};

  for (size_t i = 0; i < 3; i++) {
    ledc_channel_config_t ledc_channel = {
        .gpio_num = led_pins[i],
        .speed_mode = SPEED_MODE,
        .channel = led_channels[i],
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = TIMER,
        .duty = 0, // Set duty to 0%
        .hpoint = 0,
        .flags = {.output_invert = 1},
    };
    ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel));
  }
  ESP_ERROR_CHECK(ledc_fade_func_install(0));

  set_led_color(SettingsHandler::shared().get_universe_color());
}

#define LCD_PIXEL_CLOCK_HZ (400 * 1000)
#define PIN_NUM_SDA 40
#define PIN_NUM_SCL 39
#define PIN_NUM_RST -1
#define I2C_HW_ADDR 0x3C

// The pixel number in horizontal and vertical
#define LCD_H_RES 128
#define LCD_V_RES 64
// Bit number used to represent command and parameter
#define LCD_CMD_BITS 8
#define LCD_PARAM_BITS 8

QueueHandle_t encoder_queue = nullptr;

int16_t enc_get_new_moves() {
  static int32_t last_position = 0;

  if (!encoder_queue) {
    return 0;
  }

  rotary_encoder_event_t event = {0};
  if (xQueueReceive(encoder_queue, &event, 0) == pdTRUE) {
    const int16_t val =
        static_cast<int16_t>(last_position - event.state.position);
    last_position = event.state.position;
    return val;
  }
  return 0;
}

void encoder_read(lv_indev_t *indev, lv_indev_data_t *data) {
  data->enc_diff = enc_get_new_moves();

  if (gpio_get_level(btn_enc_pin))
    data->state = LV_INDEV_STATE_RELEASED;
  else
    data->state = LV_INDEV_STATE_PRESSED;
}

void init_lvgl() {
  // =============== From LVGL monochrome example ===============
  ESP_LOGI(TAG, "Initialize I2C bus");
  i2c_master_bus_handle_t i2c_bus = NULL;
  i2c_master_bus_config_t bus_config = {
      .i2c_port = I2C_NUM_0,
      .sda_io_num = GPIO_NUM_40,
      .scl_io_num = GPIO_NUM_39,
      .clk_source = I2C_CLK_SRC_DEFAULT,
      .glitch_ignore_cnt = 7,
      .flags =
          {
              .enable_internal_pullup = true,
          },
  };
  ESP_ERROR_CHECK(i2c_new_master_bus(&bus_config, &i2c_bus));

  ESP_LOGI(TAG, "Install panel IO");
  esp_lcd_panel_io_handle_t io_handle = NULL;
  esp_lcd_panel_io_i2c_config_t io_config = {
      .dev_addr = I2C_HW_ADDR,
      .user_ctx = nullptr,
      .control_phase_bytes = 1,
      .dc_bit_offset = 6,
      .lcd_cmd_bits = LCD_CMD_BITS,
      .lcd_param_bits = LCD_PARAM_BITS,
      .scl_speed_hz = LCD_PIXEL_CLOCK_HZ,
  };
  ESP_ERROR_CHECK(esp_lcd_new_panel_io_i2c_v2(i2c_bus, &io_config, &io_handle));

  esp_lcd_panel_handle_t panel_handle = NULL;
  esp_lcd_panel_dev_config_t panel_config = {
      .reset_gpio_num = -1,
      .bits_per_pixel = 1,
  };

  ESP_LOGI(TAG, "Install SSD1106 panel driver");
  ESP_ERROR_CHECK(new_ssd1106(io_handle, &panel_config, &panel_handle));

  ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_handle));
  ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle));
  ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_handle, true));

  ESP_LOGI(TAG, "Initialize LVGL");
  const lvgl_port_cfg_t lvgl_cfg = ESP_LVGL_PORT_INIT_CONFIG();
  lvgl_port_init(&lvgl_cfg);

  const lvgl_port_display_cfg_t disp_cfg = {
      .io_handle = io_handle,
      .panel_handle = panel_handle,
      .buffer_size = LCD_H_RES * LCD_V_RES,
      .double_buffer = true,
      .hres = LCD_H_RES,
      .vres = LCD_V_RES,
      .monochrome = true,
      .rotation =
          {
              .swap_xy = false,
              .mirror_x = false,
              .mirror_y = false,
          },
      .color_format = LV_COLOR_FORMAT_RGB565,
      .flags =
          {
              .sw_rotate = false,
              .swap_bytes = false,
              .full_refresh = true,
              .direct_mode = true,
          },
  };
  lv_disp_t *disp = lvgl_port_add_disp(&disp_cfg);

  // Setup input device
  lv_indev_t *indev = lv_indev_create();
  lv_indev_set_type(indev, LV_INDEV_TYPE_ENCODER);
  lv_indev_set_read_cb(indev, encoder_read);

  ESP_LOGI(TAG, "Display LVGL Scroll Text");
  // Lock the mutex due to the LVGL APIs are not thread-safe
  if (lvgl_port_lock(0)) {
    /* Rotation of the screen */
    lv_disp_set_rotation(disp, LV_DISPLAY_ROTATION_0);

    ui_init();

    // Release the mutex
    lvgl_port_unlock();
  }
}

void hmi_task(void *pvParameters) {

  // === Encoder ===
  rotary_encoder_info_t encoder = {};
  ESP_ERROR_CHECK(gpio_install_isr_service(0));
  ESP_ERROR_CHECK(rotary_encoder_init(&encoder, enc_a_pin, enc_b_pin));
  ESP_ERROR_CHECK(rotary_encoder_enable_half_steps(&encoder, false));

  // Create a queue for events from the rotary encoder driver.
  // Tasks can read from this queue to receive up to date position
  // information.
  encoder_queue = rotary_encoder_create_queue();
  ESP_ERROR_CHECK(rotary_encoder_set_queue(&encoder, encoder_queue));

  const gpio_config_t config = {
      .pin_bit_mask = (1ULL << btn_enc_pin),
      .mode = GPIO_MODE_INPUT,
      .pull_up_en = GPIO_PULLUP_DISABLE,
      .pull_down_en = GPIO_PULLDOWN_DISABLE,
      .intr_type = GPIO_INTR_DISABLE,
  };
  ESP_ERROR_CHECK(gpio_config(&config));

  // === LED ===
  init_led();

  static constexpr TickType_t update_period = pdMS_TO_TICKS(10);
  TickType_t xLastWakeTime;

  while (1) {
    // Wait for incoming events on the event queue.
    // rotary_encoder_event_t event = {0};
    // if (xQueueReceive(encoder_queue, &event, update_period - 1) == pdTRUE) {

    //   val += static_cast<int>(last_position - event.state.position);
    //   val = val > 359 ? val - 360 : val;
    //   val = val < 0 ? val + 360 : val;
    //   last_position = event.state.position;

    //   const HSLColor hsl_color = {
    //       .hue = static_cast<float>(val),
    //       .sat = 1.f,
    //       .lightness = 0.1f,
    //   };
    //   set_led_color(hsl_color.to_rgb());
    // }
    if (gpio_get_level(btn_bk_pin)) {
    }
    vTaskDelayUntil(&xLastWakeTime, update_period);
  }
  ESP_ERROR_CHECK(rotary_encoder_uninit(&encoder));
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

  // Initialize settings
  SettingsHandler &settings = SettingsHandler::shared();

  // Init DMX Switcher before initializing any sources or sinks.
  DmxSwitcher &switcher = DmxSwitcher::get_switcher();
  ESP_ERROR_CHECK(switcher.init());

  // Immediately set the source and sync to the cached value.
  switcher.set_src_sink(settings.input, settings.output);
  switcher.set_output_en(settings.output_en);

  // Start onboard DMX
  TaskHandle_t onboard_dmx_task_handle;
  if (xTaskCreatePinnedToCore(onboard_dmx_task, "onboard_dmx", 4096,
                              &switcher.get_onboard_interface(), 3,
                              &onboard_dmx_task_handle, 1) != pdPASS ||
      onboard_dmx_task_handle == nullptr) {
    ESP_LOGE(TAG, "Failed to create onboard_dmx task");
    abort();
  }

  // Start TIMO
  TaskHandle_t timo_dmx_task_handle = nullptr;
  if (xTaskCreatePinnedToCore(timo_dmx_task, "timo_dmx", 4096,
                              &switcher.get_timo_interface(), 3,
                              &timo_dmx_task_handle, 1) != pdPASS ||
      timo_dmx_task_handle == nullptr) {
    ESP_LOGE(TAG, "Failed to create timo_dmx task");
    abort();
  }
  TaskNotifySettingsDelegate timo_task_notify_settings_change{
      timo_dmx_task_handle};
  settings.add_delegate(&timo_task_notify_settings_change);

  TaskHandle_t hmi_task_handle;
  if (xTaskCreatePinnedToCore(hmi_task, "hmi", 4096 * 4, nullptr, 4,
                              &hmi_task_handle, 0) != pdPASS ||
      hmi_task_handle == nullptr) {
    ESP_LOGE(TAG, "Failed to create hmi task");
    abort();
  }

  vTaskDelay(pdMS_TO_TICKS(1));

  // Init graphics
  init_lvgl();

  // Handle power in the main task
  uint64_t pwr_btn_press_start = 0;
  bool pwr_btn_released = false;
  while (1) {
    int adc_raw = 0;
    int adc_volts = 0;
    ESP_ERROR_CHECK(adc_oneshot_read(adc1_handle, pwr_btn_sns_chann, &adc_raw));
    if (do_calibration1_chan0) {
      ESP_ERROR_CHECK(
          adc_cali_raw_to_voltage(adc1_cali_chan0_handle, adc_raw, &adc_volts));
      if (adc_volts < pwr_btn_press_thresh_mv) {
        pwr_btn_released = true;
      }
      if (!pwr_btn_released || adc_volts < pwr_btn_press_thresh_mv) {
        pwr_btn_press_start = esp_timer_get_time();
      }

      if (esp_timer_get_time() - pwr_btn_press_start > 1000000) {
        ESP_LOGI(TAG, "Power button press detected, powering down.");
        vTaskDelay(pdMS_TO_TICKS(1000));
        gpio_set_level(pwr_in_ctrl_pin, false);
        vTaskDelay(portMAX_DELAY);
      }
    }

    vTaskDelay(pdMS_TO_TICKS(100));
  }

  // Tear Down
  ESP_ERROR_CHECK(adc_oneshot_del_unit(adc1_handle));
  if (do_calibration1_chan0) {
    adc_calibration_deinit(adc1_cali_chan0_handle);
  }

  settings.remove_delegate(&timo_task_notify_settings_change);

  switcher.deinit();
  vTaskDelete(onboard_dmx_task_handle);
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