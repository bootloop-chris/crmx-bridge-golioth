#pragma once

#include "Enums.h"
#include "lvgl.h"
#include <functional>
#include <string>

using Action = std::function<void(void)>;

class ViewModel {
  virtual void view_model_did_update() = 0;
};

template <typename DataT, typename ActionsT, typename DelegateT>
class ViewModel_impl : public ViewModel {
public:
  ViewModel_impl() : data(), actions(), delegate(nullptr) {}

  virtual void bind_actions(const ActionsT &_actions) {
    actions = _actions;
    view_model_did_update();
  }

  virtual void set_data(const DataT &_data) {
    data = _data;
    view_model_did_update();
  }

  virtual void set_delegate(DelegateT *_delegate) {
    delegate = _delegate;
    view_model_did_update();
  }

  virtual void view_model_did_update() {
    if (delegate) {
      delegate->set_data(data);
      delegate->bind_actions(actions);
    }
  }

  virtual const DataT &get_data() const { return data; }
  virtual const ActionsT &get_actions() const { return actions; }

protected:
  DataT data;
  ActionsT actions;
  DelegateT *delegate;
};

// ====================== Home Page ======================

struct MenuStackData {
  DmxSourceSink io_type;
};

struct MenuStackActions {
  Action onclick_title;
  Action onclick_settings;
};

struct MenuStackDelegate {
  virtual void set_data(const MenuStackData &data) = 0;
  virtual void bind_actions(const MenuStackActions &actions) = 0;
};

using MenuStackViewModel =
    ViewModel_impl<MenuStackData, MenuStackActions, MenuStackDelegate>;

struct HomePageData {
  MenuStackData input;
  MenuStackData output;
  bool output_en;
};

struct HomePageActions {
  MenuStackActions input_actions;
  MenuStackActions output_actions;
  Action onclick_output_en;
  Action onclick_settings;
};

struct HomePageDelegate {
  virtual void set_data(const HomePageData &data) = 0;
  virtual void bind_actions(const HomePageActions &actions) = 0;
};

using HomePageViewModel =
    ViewModel_impl<HomePageData, HomePageActions, HomePageDelegate>;

// ====================== Settings Page ======================

struct SettingsPageData {
  std::string title;
  std::vector<std::string> items;
};

struct SettingsPageActions {
  Action onclick_back_button;
  std::vector<Action> item_actions;
};

struct SettingsPageDelegate {
  virtual void set_data(const SettingsPageData &data) = 0;
  virtual void bind_actions(const SettingsPageActions &actions) = 0;
};

using SettingsPageViewModel =
    ViewModel_impl<SettingsPageData, SettingsPageActions, SettingsPageDelegate>;

struct UIViewModels {
  HomePageViewModel home;
  SettingsPageViewModel settings;
};
