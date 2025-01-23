#pragma once

#include "lvgl.h"
#include "ui.h"
#include "view_models.h"
#include <string>

class HomePageIOStack : public UIComponent<MenuStackViewModel>,
                        MenuStackViewModelDelegate {
  using Base = UIComponent<MenuStackViewModel>;

public:
  HomePageIOStack(lv_obj_t *parent);

  // UIComponent
  void hydrate(const MenuStackViewModel &data) override;

  // Delegate
  void set_data(const MenuStackViewModel &data) override { hydrate(data); };
  void set_actions(Action &onclick_title, Action &onclick_settings) override;

protected:
  lv_obj_t *input_select_button;
  lv_obj_t *input_select_label;
  lv_obj_t *settings_button;
};

class HomePage : public UIComponent<HomePageViewModel> {
  using Base = UIComponent<HomePageViewModel>;

public:
  static constexpr int32_t mid_width = 17;
  static constexpr int32_t footer_height = 14;

  HomePage();
  ~HomePage() {
    delete input_container;
    delete output_container;
  }

  void hydrate(const HomePageViewModel &data) override;

protected:
  HomePageIOStack *input_container;
  HomePageIOStack *output_container;
  lv_obj_t *output_en_label;
};
