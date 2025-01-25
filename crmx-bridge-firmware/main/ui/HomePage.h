#pragma once

#include "lvgl.h"
#include "ui.h"
#include "view_models.h"
#include <string>

class HomePageIOStack : public UIComponent, public MenuStackDelegate {

public:
  using ViewModelT = MenuStackViewModel;

  HomePageIOStack(lv_obj_t *parent, lv_group_t *_group);

  // Delegate
  void set_data(const MenuStackData &data) override;
  void bind_actions(const MenuStackActions &actions) override;

protected:
  lv_obj_t *input_select_button;
  lv_obj_t *input_select_label;
  lv_obj_t *settings_button;

  Action onclick_title;
  Action onclick_settings;
};

class HomePage : public UIComponent, public HomePageDelegate {

public:
  using ViewModelT = HomePageViewModel;

  static constexpr int32_t mid_width = 17;
  static constexpr int32_t footer_height = 14;

  HomePage();
  ~HomePage() {
    delete input_container;
    delete output_container;
  }

  // Delegate
  void set_data(const HomePageData &data) override;
  void bind_actions(const HomePageActions &actions) override;

protected:
  HomePageIOStack *input_container;
  HomePageIOStack *output_container;
  lv_obj_t *output_en_label;
  lv_obj_t *arrow_button;
  lv_obj_t *settings_button;

  Action onclick_output_en;
  Action onclick_settings;
};
