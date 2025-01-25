#include "NavigationController.h"
#include "HomePage.h"
#include "esp_log.h"

static NavigationController nav_controller;

NavigationController &NavigationController::get() {
  if (!nav_controller.is_init) {
    nav_controller.init();
  }
  return nav_controller;
}

bool NavigationController::pop_screen() {
  // TODO: pop popup if present.

  if (nav_stack.size() < 1) {
    return false;
  }
  UIComponent *popped = nav_stack.back();
  nav_stack.pop_back();
  present_screen(nav_stack.back());
  attach_indevs_to_group(nav_stack.back()->get_group());
  if (popped) {
    delete popped;
  }

  return true;
}

void NavigationController::attach_indevs_to_group(lv_group_t *group) {
  // Attach all input devices to the group
  lv_indev_t *indev = NULL;
  while (indev = lv_indev_get_next(indev), indev) {
    // This will only apply for keypad and encoder indev types
    lv_indev_set_group(indev, group);
  }
}

void NavigationController::present_screen(UIComponent *screen) {
  lv_screen_load(screen->obj());
}

void NavigationController::init() {
  // Set home screen active and delete previous active screen
  lv_obj_t *prev_active_screen = lv_screen_active();
  push_screen<HomePage>(view_models.home);
  lv_obj_delete(prev_active_screen);

  is_init = true;
}
