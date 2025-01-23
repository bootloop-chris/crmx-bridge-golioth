#pragma once

#include "lvgl.h"

struct Style {
  // Foreground black
  static constexpr lv_color_t fg_color = {
      .blue = 0,
      .green = 0,
      .red = 0,
  };

  // Background white
  static constexpr lv_color_t bg_color = {
      .blue = 255,
      .green = 255,
      .red = 255,
  };

  static const Style &get();

  void init() {
    init_button_styles();
    is_init = true;
  }

  void style_button(lv_obj_t *button) const {
    lv_obj_add_style(button, &btn_style_base, LV_STATE_DEFAULT);
    lv_obj_add_style(button, &btn_style_click, LV_STATE_PRESSED);
    lv_obj_add_style(button, &btn_style_focus, LV_STATE_FOCUSED);
  }

protected:
  void init_button_styles();

  bool is_init = false;

public:
  // TODO: made these const
  lv_style_t btn_style_base;
  lv_style_t btn_style_focus;
  lv_style_t btn_style_click;
};
