#pragma once

#include "lvgl.h"
#include <functional>
#include <string>

using Action = std::function<void(void)>;

template <typename DelegateT> struct ViewModelInterface {

  virtual void set_delegate(DelegateT *_delegate) { delegate = _delegate; }

  virtual void view_model_did_update() {
    if (delegate) {
      delegate->hydrate(*this);
    }
  }

protected:
  DelegateT *delegate;
};

struct MenuStackViewModel;

struct MenuStackViewModelDelegate {
  virtual void set_data(const MenuStackViewModel &data) = 0;
  virtual void set_actions(Action &onclick_title, Action &onclick_settings) = 0;
};

struct MenuStackViewModel
    : public ViewModelInterface<MenuStackViewModelDelegate> {
  using Base = ViewModelInterface<MenuStackViewModelDelegate>;

  void set_delegate(MenuStackViewModelDelegate *_delegate) override {
    Base::set_delegate(_delegate);
    if (delegate) {
      delegate->set_actions(onclick_title, onclick_settings);
    }
  }

  std::string title;

  Action onclick_title;
  Action onclick_settings;
};

struct HomePageViewModel;

struct HomePageViewModelDelegate {
  virtual void set_data(const HomePageViewModel &data) = 0;
  virtual void set_actions(Action &onclick_output_en) = 0;
};

struct HomePageViewModel
    : public ViewModelInterface<HomePageViewModelDelegate> {
  using Base = ViewModelInterface<HomePageViewModelDelegate>;

  void set_delegate(HomePageViewModelDelegate *_delegate) override {
    Base::set_delegate(_delegate);
    if (delegate) {
      delegate->set_actions(onclick_output_en);
    }
  }

  MenuStackViewModel input;
  MenuStackViewModel output;
  bool output_en;

  Action onclick_output_en;
};
