#include "SettingsPage.h"
#include "Style.h"
#include "esp_log.h"
#include "ui_priv.h"
#include <algorithm>
#include <type_traits>

#define TAG "UI_SETTINGS"

SettingsPage::SettingsPage() : UIComponent(lv_obj_create(NULL)) {
  lv_obj_set_width(root, LV_PCT(100));
  lv_obj_set_height(root, LV_PCT(100));
  lv_obj_set_scrollbar_mode(root, LV_SCROLLBAR_MODE_AUTO);
  lv_obj_set_style_bg_color(root, Style::fg_color, LV_PART_SCROLLBAR);

  title_label = lv_label_create(root);
  lv_label_set_text(title_label, "Settings");
  lv_obj_set_width(title_label, LV_PCT(100));
  lv_obj_set_style_text_align(title_label, LV_TEXT_ALIGN_CENTER,
                              LV_STATE_DEFAULT);
  lv_obj_update_layout(title_label);
  lv_obj_set_y(title_label,
               (header_height - lv_obj_get_height(title_label)) / 2);
  lv_obj_remove_flag(title_label, LV_OBJ_FLAG_CLICKABLE);

  back_button = lv_button_create(root);
  lv_group_add_obj(group, back_button);
  lv_obj_add_event_cb(back_button, ui_click_action_handler, LV_EVENT_CLICKED,
                      &onclick_back_button);
  lv_obj_set_size(back_button, 12, header_height);
  Style::get().style_button(back_button);
  lv_obj_t *back_button_label = lv_label_create(back_button);
  lv_label_set_text(back_button_label, LV_SYMBOL_LEFT);
  lv_obj_set_style_text_font(back_button_label, &lv_font_montserrat_12,
                             LV_STATE_DEFAULT);

  list_container = lv_list_create(root);
  lv_obj_set_size(list_container, LV_PCT(100),
                  lv_obj_get_height(root) - header_height);
  lv_obj_set_pos(list_container, 0, header_height);
}

void SettingsPage::set_data(const SettingsPageData &data) {
  if (data.items.size() > max_items) {
    ESP_LOGE(TAG, "Attempting to add %d items to a settings list (max %d)",
             data.items.size(), max_items);
  }

  for (size_t i = 0; i < num_items; i++) {
    lv_obj_delete(items[i]);
    item_actions[i] = Action();
  }
  num_items = std::min(data.items.size(), max_items);

  for (size_t i = 0; i < num_items; i++) {
    items[i] = lv_list_add_button(list_container, NULL, data.items[i].c_str());
    lv_group_add_obj(group, items[i]);
    lv_obj_add_event_cb(items[i], ui_click_action_handler, LV_EVENT_CLICKED,
                        &(item_actions[i]));
    Style::get().style_button(items[i]);
  }

  lv_group_focus_obj(back_button);
}

void SettingsPage::bind_actions(const SettingsPageActions &actions) {
  onclick_back_button = actions.onclick_back_button;
  if (std::min(actions.item_actions.size(), max_items) != num_items) {
    ESP_LOGE(TAG, "Number of actions does not equal number of items (%d != %d)",
             actions.item_actions.size(), num_items);
    return;
  }
  for (size_t i = 0; i < num_items; i++) {
    item_actions[i] = actions.item_actions[i];
  }
}

void populate_from_settings_list(std::vector<SettingListEntry> &list) {}
