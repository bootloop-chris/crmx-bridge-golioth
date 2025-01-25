#pragma once

#include "esp_log.h"
#include "lvgl.h"
#include "ui.h"
#include "view_models.h"
#include <string>
#include <vector>

class NavigationController {
  static constexpr char const *TAG = "NAV_CTL";

public:
  static NavigationController &get();

  template <typename ScreenT>
  void push_screen(ScreenT::ViewModelT &view_model) {
    ScreenT *screen = new ScreenT();
    if (!screen) {
      ESP_LOGE(TAG, "Screen cannot be null");
    }
    view_model.set_delegate(screen);

    nav_stack.push_back(screen);
    present_screen(nav_stack.back());
    attach_indevs_to_group(nav_stack.back()->get_group());
  }

  bool pop_screen();

  template <typename PopupT> void show_popup(PopupT::ViewModelT &view_model) {
    if (popup) {
      delete popup;
      popup = nullptr;
    }
    PopupT *new_popup = new PopupT(lv_layer_top());
    if (!new_popup) {
      ESP_LOGE(TAG, "Failed to create popup");
      return;
    }
    view_model.set_delegate(new_popup);

    popup = new_popup;
    attach_indevs_to_group(popup->get_group());
  }

  void dismiss_popup() {
    attach_indevs_to_group(nav_stack.back()->get_group());
    if (popup) {
      delete popup;
      popup = nullptr;
    }
  }

  UIViewModels &get_view_models() { return view_models; }

protected:
  void attach_indevs_to_group(lv_group_t *group);
  void present_screen(UIComponent *screen);
  void init();

  bool is_init = false;

  std::vector<UIComponent *> nav_stack;
  UIComponent *popup;

  lv_group_t *lv_input_group;

  UIViewModels view_models;
};
