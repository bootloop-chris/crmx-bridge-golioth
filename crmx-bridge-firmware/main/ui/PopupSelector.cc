#include "PopupSelector.h"
#include "Style.h"
#include "esp_log.h"
#include "ui_priv.h"
#include <algorithm>
#include <type_traits>

#define TAG "UI_POPUP"

void popup_select_action_handler(lv_event_t *e) {
  if (!e) {
    return;
  }
  PopupSelector::UserData *action =
      static_cast<PopupSelector::UserData *>(lv_event_get_user_data(e));
  if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
    ESP_LOGE(TAG, "popup select event handler got unexpected event code: %d",
             lv_event_get_code(e));
  } else if (action) {
    ESP_LOGI(TAG, "Popup select event, %s", action->option.c_str());
    action->on_select(action->option);
  }

  if (!action) {
    ESP_LOGE(TAG, "popup select event occurred with no action");
    return;
  }
}

PopupSelector::PopupSelector(lv_obj_t *parent)
    : UIComponent(lv_list_create(parent)) {
  lv_obj_set_size(root, LV_PCT(90), LV_PCT(90));
  lv_obj_center(root);
  lv_obj_set_style_border_width(root, 1, LV_STATE_DEFAULT);
  lv_obj_set_style_bg_color(root, Style::fg_color, LV_PART_SCROLLBAR);
  lv_obj_set_style_width(root, 2, LV_PART_SCROLLBAR);
}

void PopupSelector::set_data(const PopupSelectorData &data) {
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
    items[i] = lv_list_add_button(root, NULL, data.choices[i].c_str());
    lv_group_add_obj(group, items[i]);
    item_actions[i] = UserData{
        .option = data.choices[i],
        .on_select = on_select,
    };
    lv_obj_add_event_cb(items[i], popup_select_action_handler, LV_EVENT_CLICKED,
                        &(item_actions[i]));
    lv_obj_set_width(items[i],
                     lv_obj_get_width(root) - Style::button_border_width * 2);
    Style::get().style_button(items[i]);
    lv_obj_set_style_text_align(items[i], LV_TEXT_ALIGN_CENTER,
                                LV_STATE_DEFAULT);
  }

  // If the list height is less than the screen height, reduce the popup height
  // to the list height
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

void PopupSelector::bind_actions(const PopupSelectorActions &actions) {
  on_select = actions.on_select;

  for (UserData &data : item_actions) {
    data.on_select = on_select;
  }
}
