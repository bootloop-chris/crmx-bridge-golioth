#include "ui_priv.h"
#include "esp_log.h"
#include "view_models.h"

#define TAG "UI_PRIV"

void ui_click_action_handler(lv_event_t *e) {
  if (!e) {
    return;
  }
  Action *action = static_cast<Action *>(lv_event_get_user_data(e));
  if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
    ESP_LOGE(TAG, "click event handler got unexpected event code: %d",
             lv_event_get_code(e));
  } else if (action) {
    Action _action = *action;
    _action();
  }

  if (!action) {
    ESP_LOGE(TAG, "click event occurred with no action");
    return;
  }
}
