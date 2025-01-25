#include "Style.h"

static Style global_styles{};

const Style &Style::get() {
  if (!global_styles.is_init) {
    global_styles.init();
  }
  return global_styles;
}

void Style::init_button_styles() {
  lv_style_init(&btn_style_base);
  lv_style_set_border_width(&btn_style_base, button_border_width);
  lv_style_set_border_color(&btn_style_base, bg_color);
  lv_style_set_bg_color(&btn_style_base, bg_color);
  lv_style_set_bg_opa(&btn_style_base, LV_OPA_100);
  // TODO: why doesn't this work
  // lv_style_set_radius(&btn_style_base, 2);

  lv_style_init(&btn_style_focus);
  lv_style_set_border_color(&btn_style_focus, fg_color);
  lv_style_set_bg_color(&btn_style_focus, bg_color);

  lv_style_init(&btn_style_click);
  lv_style_set_border_color(&btn_style_click, bg_color);
  lv_style_set_bg_color(&btn_style_click, fg_color);
  lv_style_set_text_color(&btn_style_click, bg_color);
}
