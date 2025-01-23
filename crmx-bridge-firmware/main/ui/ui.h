#pragma once

#include "lvgl.h"

template <typename DataModel> class UIComponent {
protected:
  UIComponent(lv_obj_t *_root) : root(_root) {}
  virtual ~UIComponent() { lv_obj_delete(root); }

public:
  virtual void hydrate(const DataModel &data){};

  lv_obj_t *obj() { return root; }

protected:
  lv_obj_t *root;
};

void ui_init();
void ui_deinit();
