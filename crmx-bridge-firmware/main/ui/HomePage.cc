#include "HomePage.h"
#include "Style.h"
#include "esp_log.h"

#define TAG "UI_HOME"

struct UserData {
  Action action;
};

static void home_page_io_stack_action_handler(lv_event_t *e) {
  if (!e) {
    return;
  }
  if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
    ESP_LOGE(TAG, "HomePageIOStack got unexpected event code: %d",
             lv_event_get_code(e));
    void *ud = lv_event_get_user_data(e);
    if (ud) {
      delete ud;
    }
    return;
  }
  UserData *ud = static_cast<UserData *>(lv_event_get_user_data(e));
  if (!ud) {
    ESP_LOGE(TAG, "No action supplied!");
    return;
  }
  ud->action();
  delete ud;
}

HomePageIOStack::HomePageIOStack(lv_obj_t *parent)
    : Base(lv_obj_create(parent)) {
  lv_obj_set_layout(root, LV_LAYOUT_FLEX);
  lv_obj_set_flex_flow(root, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_flex_main_place(root, LV_FLEX_ALIGN_CENTER,
                                   LV_STATE_DEFAULT);
  lv_obj_set_style_flex_cross_place(root, LV_FLEX_ALIGN_CENTER,
                                    LV_STATE_DEFAULT);
  lv_obj_set_style_flex_track_place(root, LV_FLEX_ALIGN_CENTER,
                                    LV_STATE_DEFAULT);

  input_select_button = lv_button_create(root);
  Style::get().style_button(input_select_button);

  input_select_label = lv_label_create(input_select_button);
  lv_obj_center(input_select_button);

  settings_button = lv_button_create(root);
  Style::get().style_button(settings_button);
  lv_obj_set_style_text_font(settings_button, &lv_font_montserrat_16,
                             LV_STATE_DEFAULT);

  lv_obj_t *input_settings_label = lv_label_create(settings_button);
  lv_label_set_text(input_settings_label, LV_SYMBOL_SETTINGS);
  lv_obj_center(input_settings_label);
}

void HomePageIOStack::hydrate(const MenuStackViewModel &data) {
  if (input_select_label) {
    lv_label_set_text(input_select_label, data.title.c_str());
  }
}

void HomePageIOStack::set_actions(Action &onclick_title,
                                  Action &onclick_settings) {
  // Event handler will handle failure to allocate user data.
  lv_obj_add_event_cb(input_select_button, home_page_io_stack_action_handler,
                      LV_EVENT_CLICKED, new UserData{.action = onclick_title});
  lv_obj_add_event_cb(settings_button, home_page_io_stack_action_handler,
                      LV_EVENT_CLICKED,
                      new UserData{.action = onclick_settings});
}

HomePage::HomePage() : Base(lv_obj_create(NULL)) {
  // Create a grid container the width and height of the screen
  lv_obj_set_width(root, LV_PCT(100));
  lv_obj_set_height(root, LV_PCT(100));

  const int32_t main_row_height = lv_obj_get_height(root) - footer_height;
  const int32_t input_output_width = (lv_obj_get_width(root) - mid_width) / 2;

  // Set up the top-level grid (one row, 3 columns)
  static int32_t _cols[] = {LV_GRID_FR(1), mid_width, LV_GRID_FR(1),
                            LV_GRID_TEMPLATE_LAST};
  static int32_t _rows[] = {LV_GRID_FR(1), footer_height,
                            LV_GRID_TEMPLATE_LAST};
  lv_obj_set_grid_dsc_array(root, _cols, _rows);
  lv_obj_set_layout(root, LV_LAYOUT_GRID);
  lv_obj_set_style_pad_row(root, 0, 0);
  lv_obj_set_style_pad_column(root, 0, 0);

  // Input side
  //   auto input_actions = home_screen_menu_stack_actions_t{
  //       .onclick_title = _onclick_input_title,
  //       .onclick_settings = _onclick_input_settings,
  //   };
  input_container = new HomePageIOStack(root);
  if (!input_container) {
    ESP_LOGE(TAG, "Failed to create input IO stack!");
    return;
  }
  lv_obj_set_grid_cell(input_container->obj(), LV_GRID_ALIGN_CENTER, 0, 1,
                       LV_GRID_ALIGN_CENTER, 0, 1);
  lv_obj_set_size(input_container->obj(), input_output_width, main_row_height);

  // Center arrow
  lv_obj_t *arrow_button = lv_button_create(root);
  lv_obj_set_grid_cell(arrow_button, LV_GRID_ALIGN_CENTER, 1, 1,
                       LV_GRID_ALIGN_CENTER, 0, 1);
  //   lv_obj_add_event_cb(arrow_button, _onclick_output_en, LV_EVENT_CLICKED,
  //   NULL);
  lv_obj_set_width(arrow_button, mid_width);
  Style::get().style_button(arrow_button);
  output_en_label = lv_label_create(arrow_button);
  lv_obj_set_style_text_font(output_en_label, &lv_font_montserrat_16,
                             LV_STATE_DEFAULT);
  lv_obj_center(output_en_label);

  // Output side
  //   auto output_actions = home_screen_menu_stack_actions_t{
  //       .onclick_title = _onclick_output_title,
  //       .onclick_settings = _onclick_output_settings,
  //   };
  output_container = new HomePageIOStack(root);
  if (!output_container) {
    ESP_LOGE(TAG, "Failed to create output IO stack!");
    return;
  }
  lv_obj_set_grid_cell(output_container->obj(), LV_GRID_ALIGN_CENTER, 2, 1,
                       LV_GRID_ALIGN_CENTER, 0, 1);
  lv_obj_set_size(output_container->obj(), input_output_width, main_row_height);
}

void HomePage::hydrate(const HomePageViewModel &data) {
  if (input_container) {
    input_container->hydrate(data.input);
  }
  if (output_container) {
    output_container->hydrate(data.output);
  }
  if (output_en_label) {
    lv_label_set_text(output_en_label,
                      data.output_en ? LV_SYMBOL_RIGHT : LV_SYMBOL_CLOSE);
  }
}
