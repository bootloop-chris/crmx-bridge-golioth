/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include "driver/i2c_master.h"
#include "esp_err.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_log.h"
#include "esp_lvgl_port.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lvgl.h"
#include <stdio.h>

#if CONFIG_EXAMPLE_LCD_CONTROLLER_SH1107
#include "esp_lcd_sh1107.h"
#else
#include "esp_lcd_panel_vendor.h"
#endif

static const char *TAG = "example";

#define I2C_HOST 0

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////// Please update the following configuration according to your
///LCD spec //////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#define EXAMPLE_LCD_PIXEL_CLOCK_HZ (400 * 1000)
#define EXAMPLE_PIN_NUM_SDA 18
#define EXAMPLE_PIN_NUM_SCL 23
#define EXAMPLE_PIN_NUM_RST -1
#define EXAMPLE_I2C_HW_ADDR 0x3C

// The pixel number in horizontal and vertical
#if CONFIG_EXAMPLE_LCD_CONTROLLER_SSD1306
#define EXAMPLE_LCD_H_RES 128
#define EXAMPLE_LCD_V_RES CONFIG_EXAMPLE_SSD1306_HEIGHT
#elif CONFIG_EXAMPLE_LCD_CONTROLLER_SH1107
#define EXAMPLE_LCD_H_RES 64
#define EXAMPLE_LCD_V_RES 128
#endif
// Bit number used to represent command and parameter
#define EXAMPLE_LCD_CMD_BITS 8
#define EXAMPLE_LCD_PARAM_BITS 8

extern void example_lvgl_demo_ui(lv_disp_t *disp);

void app_main(void) {
  ESP_LOGI(TAG, "Initialize I2C bus");
  i2c_master_bus_handle_t i2c_bus = NULL;
  i2c_master_bus_config_t bus_config = {
      .clk_source = I2C_CLK_SRC_DEFAULT,
      .glitch_ignore_cnt = 7,
      .i2c_port = I2C_HOST,
      .sda_io_num = EXAMPLE_PIN_NUM_SDA,
      .scl_io_num = EXAMPLE_PIN_NUM_SCL,
      .flags.enable_internal_pullup = true,
  };
  ESP_ERROR_CHECK(i2c_new_master_bus(&bus_config, &i2c_bus));

  ESP_LOGI(TAG, "Install panel IO");
  esp_lcd_panel_io_handle_t io_handle = NULL;
  esp_lcd_panel_io_i2c_config_t io_config = {
    .dev_addr = EXAMPLE_I2C_HW_ADDR,
    .scl_speed_hz = EXAMPLE_LCD_PIXEL_CLOCK_HZ,
    .control_phase_bytes = 1,                 // According to SSD1306 datasheet
    .lcd_cmd_bits = EXAMPLE_LCD_CMD_BITS,     // According to SSD1306 datasheet
    .lcd_param_bits = EXAMPLE_LCD_PARAM_BITS, // According to SSD1306 datasheet
#if CONFIG_EXAMPLE_LCD_CONTROLLER_SSD1306
    .dc_bit_offset = 6, // According to SSD1306 datasheet
#elif CONFIG_EXAMPLE_LCD_CONTROLLER_SH1107
    .dc_bit_offset = 0, // According to SH1107 datasheet
    .flags =
        {
            .disable_control_phase = 1,
        }
#endif
  };
  ESP_ERROR_CHECK(esp_lcd_new_panel_io_i2c(i2c_bus, &io_config, &io_handle));

  esp_lcd_panel_handle_t panel_handle = NULL;
  esp_lcd_panel_dev_config_t panel_config = {
    .bits_per_pixel = 1,
    .reset_gpio_num = EXAMPLE_PIN_NUM_RST,
#if (ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(5, 0, 0))
    .color_space = ESP_LCD_COLOR_SPACE_MONOCHROME,
#endif
  };
#if CONFIG_EXAMPLE_LCD_CONTROLLER_SSD1306
#if (ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 3, 0))
  esp_lcd_panel_ssd1306_config_t ssd1306_config = {
      .height = EXAMPLE_LCD_V_RES,
  };
  panel_config.vendor_config = &ssd1306_config;
#endif
  ESP_LOGI(TAG, "Install SSD1306 panel driver");
  ESP_ERROR_CHECK(
      esp_lcd_new_panel_ssd1306(io_handle, &panel_config, &panel_handle));
#elif CONFIG_EXAMPLE_LCD_CONTROLLER_SH1107
  ESP_LOGI(TAG, "Install SH1107 panel driver");
  ESP_ERROR_CHECK(
      esp_lcd_new_panel_sh1107(io_handle, &panel_config, &panel_handle));
#endif

  ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_handle));
  ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle));
  ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_handle, true));

#if CONFIG_EXAMPLE_LCD_CONTROLLER_SH1107
  ESP_ERROR_CHECK(esp_lcd_panel_invert_color(panel_handle, true));
#endif

  ESP_LOGI(TAG, "Initialize LVGL");
  const lvgl_port_cfg_t lvgl_cfg = ESP_LVGL_PORT_INIT_CONFIG();
  lvgl_port_init(&lvgl_cfg);

  const lvgl_port_display_cfg_t disp_cfg =
  {.io_handle = io_handle,
   .panel_handle = panel_handle,
   .buffer_size = EXAMPLE_LCD_H_RES * EXAMPLE_LCD_V_RES,
   .double_buffer = true,
   .hres = EXAMPLE_LCD_H_RES,
   .vres = EXAMPLE_LCD_V_RES,
   .monochrome = true,
#if LVGL_VERSION_MAJOR >= 9
   .color_format = LV_COLOR_FORMAT_RGB565,
#endif
   .rotation =
       {
           .swap_xy = false,
           .mirror_x = false,
           .mirror_y = false,
       },
   .flags = {
#if LVGL_VERSION_MAJOR >= 9
       .swap_bytes = false,
#endif
       .sw_rotate = false,
   } };
  lv_disp_t *disp = lvgl_port_add_disp(&disp_cfg);

  ESP_LOGI(TAG, "Display LVGL Scroll Text");
  // Lock the mutex due to the LVGL APIs are not thread-safe
  if (lvgl_port_lock(0)) {
    /* Rotation of the screen */
    lv_disp_set_rotation(disp, LV_DISPLAY_ROTATION_0);

    example_lvgl_demo_ui(disp);
    // Release the mutex
    lvgl_port_unlock();
  }
}
