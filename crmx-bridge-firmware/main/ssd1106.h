#ifndef SSD1106_H
#define SSD1106_H

#include "esp_lcd_panel_interface.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"

#define ssd1106_num_pages 8
#define ssd1106_max_width 132

typedef struct {
  uint8_t _segs[ssd1106_max_width];
} PAGE_t;

typedef struct {
  esp_lcd_panel_io_handle_t io;
  int bits_per_pixel;
  PAGE_t _pages[ssd1106_num_pages];
  esp_lcd_panel_t base;
} ssd1106_t;

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t new_ssd1106(const esp_lcd_panel_io_handle_t io,
                      const esp_lcd_panel_dev_config_t *panel_dev_config,
                      esp_lcd_panel_handle_t *ret_panel);

#ifdef __cplusplus
}
#endif

#endif
