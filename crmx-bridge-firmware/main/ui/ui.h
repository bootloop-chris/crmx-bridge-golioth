#pragma once

#include "esp_lvgl_port.h"
#include "lvgl.h"

class UIComponent {
protected:
  UIComponent(lv_obj_t *_root, lv_group_t *_group = nullptr)
      : root(_root), group(_group == nullptr ? lv_group_create() : _group) {}

public:
  virtual ~UIComponent() {
    if (group) {
      lv_group_delete(group);
    }
    if (root) {
      lv_obj_delete(root);
    }
  }
  lv_obj_t *obj() { return root; }
  lv_group_t *get_group() { return group; }

protected:
  lv_obj_t *root;
  lv_group_t *group;
};

void ui_init();
void ui_deinit();
void ui_tick();
