#include "HomePage.h"
#include "Style.h"
#include "lvgl.h"
#include "ui.h"

#define TAG "UI"

static lv_group_t *g;
static HomePage *home_page;

void ui_init() {
  g = lv_group_create();
  lv_group_set_default(g);

  Style::get();

  lv_indev_t *indev = NULL;
  for (;;) {
    indev = lv_indev_get_next(indev);
    if (!indev) {
      break;
    }

    lv_indev_type_t indev_type = lv_indev_get_type(indev);
    if (indev_type == LV_INDEV_TYPE_KEYPAD) {
      lv_indev_set_group(indev, g);
    }

    if (indev_type == LV_INDEV_TYPE_ENCODER) {
      lv_indev_set_group(indev, g);
    }
  }
  // Create screens
  home_page = new HomePage();

  // Set home screen active and delete previous active screen
  lv_obj_t *prev_active_screen = lv_screen_active();
  lv_screen_load(home_page->obj());
  lv_obj_delete(prev_active_screen);

  // Temporarily - hydrate the home page UI.
  HomePageViewModel model = {
      .input = HomePageIOStackDataModel{.title = "DMX"},
      .output = HomePageIOStackDataModel{.title = "CRMX"},
      .output_en = true,
  };
  home_page->hydrate(model);
}

void ui_deinit() {
  delete home_page;
  home_page = nullptr;
}
