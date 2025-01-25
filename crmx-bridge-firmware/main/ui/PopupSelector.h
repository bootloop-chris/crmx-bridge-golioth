#pragma once

#include "lvgl.h"
#include "ui.h"
#include "view_models.h"
#include <string>

class PopupSelector : public UIComponent, public PopupSelectorDelegate {
public:
  using ViewModelT = PopupSelectorViewModel;

  static constexpr size_t max_items = 32;

  struct UserData {
    std::string option;
    std::function<void(std::string)> on_select;
  };

  PopupSelector(lv_obj_t *parent);

  // Delegate
  void set_data(const PopupSelectorData &data) override;
  void bind_actions(const PopupSelectorActions &actions) override;

protected:
  std::array<lv_obj_t *, max_items> items = {};
  std::array<UserData, max_items> item_actions = {};
  size_t num_items = 0;
  std::function<void(std::string)> on_select;
};