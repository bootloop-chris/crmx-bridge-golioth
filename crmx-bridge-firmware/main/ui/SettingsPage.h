#pragma once

#include "lvgl.h"
#include "ui.h"
#include "view_models.h"
#include <string>

enum SettingDisplayType {
  on_off,
  list_select,
  int_select,
};

struct SettingOnOffSelectData {
  bool value;
  std::function<void(bool)> on_select;
};

struct SettingListSelectData {
  std::string value;
  std::vector<std::string> options;
  std::function<void(std::string)> on_select;
};

struct SettingIntSelectData {
  int value;
  const int min;
  const int max;
  const bool wrap;
  std::function<void(int)> on_select;
};

struct SettingListEntry {
  SettingDisplayType type;
  union {
    SettingOnOffSelectData on_off;
    SettingListSelectData list;
    SettingIntSelectData num;
  } data;
};

class SettingsPage : public UIComponent, public SettingsPageDelegate {

public:
  using ViewModelT = SettingsPageViewModel;

  static constexpr int32_t header_height = 16;
  static constexpr size_t max_items = 32;

  SettingsPage();

  // Delegate
  void set_data(const SettingsPageData &data) override;
  void bind_actions(const SettingsPageActions &actions) override;

  void populate_from_settings_list(std::vector<SettingListEntry> &list);

protected:
  lv_obj_t *title_label;
  lv_obj_t *back_button;
  lv_obj_t *list_container;
  std::array<lv_obj_t *, max_items> items = {};

  Action onclick_back_button;
  std::array<Action, max_items> item_actions = {};
  size_t num_items = 0;
};