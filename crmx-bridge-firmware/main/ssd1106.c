#include "ssd1106.h"
#include "driver/gpio.h"
#include "esp_check.h"
#include "esp_log.h"
#include "string.h"

// Some portions of the below component copied and modified from
// https://github.com/nopnop2002/esp-idf-ssd1306

// Following definitions are bollowed from
// http://robotcantalk.blogspot.com/2015/03/interfacing-arduino-with-ssd1306-driven.html

/* Control byte for i2c
Co : bit 8 : Continuation Bit
 * 1 = no-continuation (only one byte to follow)
 * 0 = the controller should expect a stream of bytes.
D/C# : bit 7 : Data/Command Select bit
 * 1 = the next byte or byte stream will be Data.
 * 0 = a Command byte or byte stream will be coming up next.
 Bits 6-0 will be all zeros.
Usage:
0x80 : Single Command byte
0x00 : Command Stream
0xC0 : Single Data byte
0x40 : Data Stream
*/
#define OLED_CONTROL_BYTE_CMD_SINGLE 0x80
#define OLED_CONTROL_BYTE_CMD_STREAM 0x00
#define OLED_CONTROL_BYTE_DATA_SINGLE 0xC0
#define OLED_CONTROL_BYTE_DATA_STREAM 0x40

// Fundamental commands (pg.28)
#define OLED_CMD_SET_CONTRAST 0x81 // follow with 0x7F
#define OLED_CMD_DISPLAY_RAM 0xA4
#define OLED_CMD_DISPLAY_ALLON 0xA5
#define OLED_CMD_DISPLAY_NORMAL 0xA6
#define OLED_CMD_DISPLAY_INVERTED 0xA7
#define OLED_CMD_DISPLAY_OFF 0xAE
#define OLED_CMD_DISPLAY_ON 0xAF

// Addressing Command Table (pg.30)
#define OLED_CMD_SET_MEMORY_ADDR_MODE 0x20
#define OLED_CMD_SET_HORI_ADDR_MODE 0x00 // Horizontal Addressing Mode
#define OLED_CMD_SET_VERT_ADDR_MODE 0x01 // Vertical Addressing Mode
#define OLED_CMD_SET_PAGE_ADDR_MODE 0x02 // Page Addressing Mode
#define OLED_CMD_SET_COLUMN_RANGE                                              \
  0x21 // can be used only in HORZ/VERT mode - follow with 0x00 and 0x7F =
       // COL127
#define OLED_CMD_SET_PAGE_RANGE                                                \
  0x22 // can be used only in HORZ/VERT mode - follow with 0x00 and 0x07 = PAGE7

// Hardware Config (pg.31)
#define OLED_CMD_SET_DISPLAY_START_LINE 0x40
#define OLED_CMD_SET_SEGMENT_REMAP_0 0xA0
#define OLED_CMD_SET_SEGMENT_REMAP_1 0xA1
#define OLED_CMD_SET_MUX_RATIO 0xA8 // follow with 0x3F = 64 MUX
#define OLED_CMD_SET_COM_SCAN_MODE 0xC8
#define OLED_CMD_SET_DISPLAY_OFFSET 0xD3 // follow with 0x00
#define OLED_CMD_SET_COM_PIN_MAP 0xDA    // follow with 0x12
#define OLED_CMD_NOP 0xE3                // NOP

// Timing and Driving Scheme (pg.32)
#define OLED_CMD_SET_DISPLAY_CLK_DIV 0xD5 // follow with 0x80
#define OLED_CMD_SET_PRECHARGE 0xD9       // follow with 0xF1
#define OLED_CMD_SET_VCOMH_DESELCT 0xDB   // follow with 0x30

// Charge Pump (pg.62)
#define OLED_CMD_SET_CHARGE_PUMP 0x8D // follow with 0x14

// Scrolling Command
#define OLED_CMD_HORIZONTAL_RIGHT 0x26
#define OLED_CMD_HORIZONTAL_LEFT 0x27
#define OLED_CMD_CONTINUOUS_SCROLL 0x29
#define OLED_CMD_DEACTIVE_SCROLL 0x2E
#define OLED_CMD_ACTIVE_SCROLL 0x2F
#define OLED_CMD_VERTICAL 0xA3

// TODO: put this in real config
#define OFFSET_X 2

#define TAG "SSD1106"

static esp_err_t ssd1106_del(esp_lcd_panel_t *_panel);
static esp_err_t ssd1106_reset(esp_lcd_panel_t *_panel);
static esp_err_t ssd1106_init(esp_lcd_panel_t *_panel);
static esp_err_t ssd1106_draw_bitmap(esp_lcd_panel_t *_panel, int x_start,
                                     int y_start, int x_end, int y_end,
                                     const void *color_data);
static esp_err_t ssd1106_invert_color(esp_lcd_panel_t *_panel,
                                      bool invert_color_data);
static esp_err_t ssd1106_mirror(esp_lcd_panel_t *_panel, bool mirror_x,
                                bool mirror_y);
static esp_err_t ssd1106_swap_xy(esp_lcd_panel_t *_panel, bool swap_axes);
static esp_err_t ssd1106_set_gap(esp_lcd_panel_t *_panel, int x_gap, int y_gap);
static esp_err_t ssd1106_disp_on_off(esp_lcd_panel_t *_panel, bool on);

esp_err_t new_ssd1106(const esp_lcd_panel_io_handle_t io,
                      const esp_lcd_panel_dev_config_t *panel_dev_config,
                      esp_lcd_panel_handle_t *ret_panel) {
  esp_err_t ret = ESP_OK;
  ssd1106_t *panel = NULL;

  ESP_GOTO_ON_FALSE(io && panel_dev_config && ret_panel, ESP_ERR_INVALID_ARG,
                    err, TAG, "invalid argument");
  ESP_GOTO_ON_FALSE(panel_dev_config->bits_per_pixel == 1, ESP_ERR_INVALID_ARG,
                    err, TAG, "bpp must be 1");
  panel = calloc(1, sizeof(ssd1106_t));
  ESP_GOTO_ON_FALSE(panel, ESP_ERR_NO_MEM, err, TAG,
                    "no mem for ssd1306 panel");

  panel->io = io;
  panel->bits_per_pixel = panel_dev_config->bits_per_pixel;
  panel->base.del = ssd1106_del;
  panel->base.reset = ssd1106_reset;
  panel->base.init = ssd1106_init;
  panel->base.draw_bitmap = ssd1106_draw_bitmap;
  panel->base.invert_color = ssd1106_invert_color;
  panel->base.set_gap = ssd1106_set_gap;
  panel->base.mirror = ssd1106_mirror;
  panel->base.swap_xy = ssd1106_swap_xy;
  panel->base.disp_on_off = ssd1106_disp_on_off;
  *ret_panel = &(panel->base);
  ESP_LOGD(TAG, "new ssd1106 panel @%p", panel);

  return ESP_OK;

err:
  if (panel) {
    if (panel_dev_config->reset_gpio_num >= 0) {
      gpio_reset_pin(panel_dev_config->reset_gpio_num);
    }
    free(panel);
  }
  return ret;
}

static esp_err_t ssd1106_del(esp_lcd_panel_t *_panel) {
  ssd1106_t *panel = __containerof(_panel, ssd1106_t, base);
  ESP_LOGD(TAG, "del ssd1306 panel @%p", panel);
  free(panel);
  return ESP_OK;
}

static esp_err_t ssd1106_reset(esp_lcd_panel_t *_panel) { return ESP_OK; }

static esp_err_t ssd1106_init(esp_lcd_panel_t *_panel) {
  ssd1106_t *panel = __containerof(_panel, ssd1106_t, base);

  // Zero-initialize internal buffer
  for (int i = 0; i < ssd1106_num_pages; i++) {
    memset(panel->_pages[i]._segs, 0, ssd1106_max_width);
  }

  esp_lcd_panel_io_handle_t io = panel->io;
  ESP_RETURN_ON_ERROR(
      esp_lcd_panel_io_tx_param(io, OLED_CMD_DISPLAY_OFF, NULL, 0), TAG,
      "io tx param OLED_CMD_DISPLAY_OFF failed");
  ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, OLED_CMD_SET_MUX_RATIO,
                                                (uint8_t[]){0x3F}, 1),
                      TAG, "io tx param SSD1306_CMD_SET_MUX_RATIO failed");
  // enable charge pump
  ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, OLED_CMD_SET_CHARGE_PUMP,
                                                (uint8_t[]){0x14}, 1),
                      TAG, "io tx param OLED_CMD_SET_CHARGE_PUMP failed");

  // Send empty buffer to display
  for (int p = 0; p < ssd1106_num_pages; p++) {
    uint8_t params[3] = {0x00, 0x10, 0xB0 | p};
    esp_lcd_panel_io_tx_param(io, OLED_CONTROL_BYTE_CMD_STREAM, params, 3);
    esp_lcd_panel_io_tx_color(io, -1, panel->_pages[p]._segs,
                              ssd1106_max_width);
  }

  //   ESP_RETURN_ON_ERROR(
  //       esp_lcd_panel_io_tx_param(io, SSD1306_CMD_MIRROR_X_OFF, NULL, 0),
  //       TAG, "io tx param SSD1306_CMD_MIRROR_X_OFF failed");
  //   ESP_RETURN_ON_ERROR(
  //       esp_lcd_panel_io_tx_param(io, SSD1306_CMD_MIRROR_Y_OFF, NULL, 0),
  //       TAG, "io tx param SSD1306_CMD_MIRROR_Y_OFF failed");

  //   dev->_width = width;
  //   dev->_height = height;
  //   dev->_pages = 8;
  //   if (dev->_height == 32)
  //     dev->_pages = 4;

  //   uint8_t out_buf[27];
  //   int out_index = 0;
  //   out_buf[out_index++] = OLED_CONTROL_BYTE_CMD_STREAM;
  //   out_buf[out_index++] = OLED_CMD_DISPLAY_OFF;   // AE
  //   out_buf[out_index++] = OLED_CMD_SET_MUX_RATIO; // A8
  //   if (dev->_height == 64)
  //     out_buf[out_index++] = 0x3F;
  //   if (dev->_height == 32)
  //     out_buf[out_index++] = 0x1F;
  //   out_buf[out_index++] = OLED_CMD_SET_DISPLAY_OFFSET; // D3
  //   out_buf[out_index++] = 0x00;
  //   // out_buf[out_index++] = OLED_CONTROL_BYTE_DATA_STREAM;	// 40
  //   out_buf[out_index++] = OLED_CMD_SET_DISPLAY_START_LINE; // 40
  //   // out_buf[out_index++] = OLED_CMD_SET_SEGMENT_REMAP;		// A1
  //   if (dev->_flip) {
  //     out_buf[out_index++] = OLED_CMD_SET_SEGMENT_REMAP_0; // A0
  //   } else {
  //     out_buf[out_index++] = OLED_CMD_SET_SEGMENT_REMAP_1; // A1
  //   }
  //   out_buf[out_index++] = OLED_CMD_SET_COM_SCAN_MODE;   // C8
  //   out_buf[out_index++] = OLED_CMD_SET_DISPLAY_CLK_DIV; // D5
  //   out_buf[out_index++] = 0x80;
  //   out_buf[out_index++] = OLED_CMD_SET_COM_PIN_MAP; // DA
  //   if (dev->_height == 64)
  //     out_buf[out_index++] = 0x12;
  //   if (dev->_height == 32)
  //     out_buf[out_index++] = 0x02;
  //   out_buf[out_index++] = OLED_CMD_SET_CONTRAST; // 81
  //   out_buf[out_index++] = 0xFF;
  //   out_buf[out_index++] = OLED_CMD_DISPLAY_RAM;       // A4
  //   out_buf[out_index++] = OLED_CMD_SET_VCOMH_DESELCT; // DB
  //   out_buf[out_index++] = 0x40;
  //   out_buf[out_index++] = OLED_CMD_SET_MEMORY_ADDR_MODE; // 20
  //   // out_buf[out_index++] = OLED_CMD_SET_HORI_ADDR_MODE;	// 00
  //   out_buf[out_index++] = OLED_CMD_SET_PAGE_ADDR_MODE; // 02
  //   // Set Lower Column Start Address for Page Addressing Mode
  //   out_buf[out_index++] = 0x00;
  //   // Set Higher Column Start Address for Page Addressing Mode
  //   out_buf[out_index++] = 0x10;
  //   out_buf[out_index++] = OLED_CMD_SET_CHARGE_PUMP; // 8D
  //   out_buf[out_index++] = 0x14;
  //   out_buf[out_index++] = OLED_CMD_DEACTIVE_SCROLL; // 2E
  //   out_buf[out_index++] = OLED_CMD_DISPLAY_NORMAL;  // A6
  //   out_buf[out_index++] = OLED_CMD_DISPLAY_ON;      // AF

  return ESP_OK;
}

static esp_err_t ssd1106_draw_bitmap(esp_lcd_panel_t *_panel, int x_start,
                                     int y_start, int x_end, int y_end,
                                     const void *color_data) {
  ESP_RETURN_ON_FALSE(_panel, ESP_ERR_INVALID_ARG, TAG,
                      "_panel is not defined");

  ssd1106_t *panel = __containerof(_panel, ssd1106_t, base);

  assert((x_start < x_end) && (y_start < y_end) &&
         "start position must be smaller than end position");
  esp_lcd_panel_io_handle_t io = panel->io;

  // ESP_LOGI(TAG, "Render box: x: %d - %d, y: %d - %d", x_start, x_end,
  // y_start,
  //          y_end);

  // adding extra gap
  //   x_start += ssd1306->x_gap;
  //   x_end += ssd1306->x_gap;
  //   y_start += ssd1306->y_gap;
  //   y_end += ssd1306->y_gap;

  //   if (ssd1306->swap_axes) {
  //     int x = x_start;
  //     x_start = y_start;
  //     y_start = x;
  //     x = x_end;
  //     x_end = y_end;
  //     y_end = x;
  //   }

  x_start += OFFSET_X;
  x_end += OFFSET_X;

  // TODO: This might have bugs, test it more fully...
  static const uint8_t stop_masks[8] = {0xFF, 0x01, 0x03, 0x07,
                                        0x0F, 0x1F, 0x3F, 0x7F};
  static const uint8_t start_masks[8] = {0xFF, 0xFE, 0xFC, 0xF8,
                                         0xF0, 0xE0, 0xC0, 0x80};

  // one page contains 8 rows (COMs)
  uint8_t page_start = y_start / 8;
  uint8_t page_end = (y_end - 1) / 8;
  int width = x_end - x_start;

  int input_idx = 0;

  for (uint8_t p = page_start; p <= page_end; p++) {
    uint8_t page_mask = 0xFF;
    page_mask &= (p == page_start) ? start_masks[y_start % 8] : 0xFF;
    page_mask &= (p == page_end) ? stop_masks[y_end % 8] : 0xFF;
    for (int x = x_start; x < x_end; x++) {
      panel->_pages[p]._segs[x] &= ~page_mask;
      panel->_pages[p]._segs[x] |=
          ((uint8_t *)color_data)[input_idx++] & page_mask;
    }

    uint8_t params[3] = {x_start & 0x0F, 0x10 + ((x_start >> 4) & 0x0F),
                         0xB0 | p};
    esp_lcd_panel_io_tx_param(io, OLED_CONTROL_BYTE_CMD_STREAM, params, 3);
    esp_lcd_panel_io_tx_color(io, -1, &panel->_pages[p]._segs[x_start], width);
  }

  return ESP_OK;
}

static esp_err_t ssd1106_invert_color(esp_lcd_panel_t *_panel,
                                      bool invert_color_data) {
  ssd1106_t *panel = __containerof(_panel, ssd1106_t, base);
  return ESP_OK;
}

static esp_err_t ssd1106_mirror(esp_lcd_panel_t *_panel, bool mirror_x,
                                bool mirror_y) {
  ssd1106_t *panel = __containerof(_panel, ssd1106_t, base);
  return ESP_OK;
}

static esp_err_t ssd1106_swap_xy(esp_lcd_panel_t *_panel, bool swap_axes) {
  ssd1106_t *panel = __containerof(_panel, ssd1106_t, base);
  return ESP_OK;
}

static esp_err_t ssd1106_set_gap(esp_lcd_panel_t *_panel, int x_gap,
                                 int y_gap) {
  ssd1106_t *panel = __containerof(_panel, ssd1106_t, base);
  return ESP_OK;
}

static esp_err_t ssd1106_disp_on_off(esp_lcd_panel_t *_panel, bool on) {
  ssd1106_t *panel = __containerof(_panel, ssd1106_t, base);
  ESP_RETURN_ON_ERROR(
      esp_lcd_panel_io_tx_param(
          panel->io, on ? OLED_CMD_DISPLAY_ON : OLED_CMD_DISPLAY_OFF, NULL, 0),
      TAG, "Set panel on/off state failed");
  return ESP_OK;
}
