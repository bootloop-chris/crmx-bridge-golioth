#pragma once

#include "Style.h"
#include "esp_log.h"
#include "lvgl.h"
#include "ui.h"
#include "ui_priv.h"
#include "view_models.h"
#include <algorithm>
#include <string>
#include <type_traits>

// Fileprivate functions and types
namespace {
template <typename T> struct UserData {
  T option;
  std::function<void(T)> on_select;
};

static constexpr char const *TAG = "UI_POPUP";

template <typename T> void popup_select_action_handler(lv_event_t *e) {
  if (!e) {
    return;
  }
  UserData<T> *action = static_cast<UserData<T> *>(lv_event_get_user_data(e));
  if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
    ESP_LOGE(TAG, "popup select event handler got unexpected event code: %d",
             lv_event_get_code(e));
  } else if (action) {
    ESP_LOGI(TAG, "Popup select event, %s",
             enum_or_string_to_c_str(action->option));
    action->on_select(action->option);
  }

  if (!action) {
    ESP_LOGE(TAG, "popup select event occurred with no action");
    return;
  }
}
} // namespace

template <typename T, typename = enable_if_enum_or_string_t<T>>
struct PopupSelectorData {
  std::vector<T> choices;
};

template <typename _SelectorType> class PopupSelector : public UIComponent {
public:
  using SelectorType = _SelectorType;
  using DataT = PopupSelectorData<SelectorType>;

  static constexpr size_t max_items = 32;

  PopupSelector(lv_obj_t *parent, const PopupSelectorData<SelectorType> &_data,
                std::function<void(SelectorType)> _on_select)
      : UIComponent(lv_list_create(parent)) {
    lv_obj_set_size(root, LV_PCT(90), LV_PCT(90));
    lv_obj_center(root);
    lv_obj_set_style_border_width(root, 1, LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(root, Style::fg_color, LV_PART_SCROLLBAR);
    lv_obj_set_style_width(root, 2, LV_PART_SCROLLBAR);

    set_data(_data);
    bind_actions(_on_select);
  }

  void set_data(const PopupSelectorData<SelectorType> &data) {
    // TODO: common base class with settings?
    if (data.choices.size() > max_items) {
      ESP_LOGE(TAG, "Attempting to add %d items to a settings list (max %d)",
               data.choices.size(), max_items);
    }

    for (size_t i = 0; i < num_items; i++) {
      lv_obj_delete(items[i]);
    }
    num_items = std::min(data.choices.size(), max_items);

    for (size_t i = 0; i < num_items; i++) {
      items[i] = lv_list_add_button(
          root, NULL, enum_or_string_to_c_str<SelectorType>(data.choices[i]));
      lv_group_add_obj(group, items[i]);
      item_actions[i] = UserData<SelectorType>{
          .option = data.choices[i],
          .on_select = on_select,
      };
      lv_obj_add_event_cb(items[i], popup_select_action_handler<SelectorType>,
                          LV_EVENT_CLICKED, &(item_actions[i]));
      lv_obj_set_width(items[i],
                       lv_obj_get_width(root) - Style::button_border_width * 2);
      Style::get().style_button(items[i]);
      lv_obj_set_style_text_align(items[i], LV_TEXT_ALIGN_CENTER,
                                  LV_STATE_DEFAULT);
    }

    // If the list height is less than the screen height, reduce the popup
    // height to the list height
    lv_obj_set_size(root, LV_PCT(90), LV_PCT(90));
    lv_obj_update_layout(root);
    const auto end_y = lv_obj_get_y(items[num_items - 1]) +
                       lv_obj_get_height(items[num_items - 1]) +
                       Style::button_border_width * 2;
    if (end_y < lv_obj_get_height(root)) {
      lv_obj_set_height(root, end_y);
      lv_obj_set_scrollbar_mode(root, LV_SCROLLBAR_MODE_OFF);
    } else {
      lv_obj_set_scrollbar_mode(root, LV_SCROLLBAR_MODE_ON);
    }

    lv_group_focus_obj(items[0]);
  }

  void bind_actions(std::function<void(SelectorType)> _on_select) {
    on_select = _on_select;

    for (UserData<SelectorType> &data : item_actions) {
      data.on_select = on_select;
    }
  }

protected:
  std::array<lv_obj_t *, max_items> items = {};
  std::array<UserData<SelectorType>, max_items> item_actions = {};
  size_t num_items = 0;
  std::function<void(SelectorType)> on_select;
};