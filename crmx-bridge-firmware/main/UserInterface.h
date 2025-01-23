#pragma once

#include "stddef.h"
#include <array>
#include <memory>
#include <string>
#include <vector>

class ScreenInterface {};

struct UIPos {
  int x;
  int y;
};

struct UISize {
  int width;
  int height;
};

struct UIElement {
  virtual void handle_press() = 0;
  virtual const UISize get_size() = 0;
  virtual void render(ScreenInterface &scrn, const UIPos &pos,
                      const bool focus) = 0;
};

static constexpr int screen_width = 128;

struct BoxMenuItem : public UIElement {
  void handle_press() override {}
  const UISize get_size() override {
    return UISize{
        .width = screen_width,
        .height = 12,
    };
  }
  void render(ScreenInterface &scrn, const UIPos &pos,
              const bool focus) override {}

  std::string text;
};

template <typename ParamT> class UIAction {
  void execute(const ParamT param);
  ParamT param;
};

struct Menu {
  template <size_t _num_children>
  Menu(std::vector<std::unique_ptr<Menu>> _children) {
    for (size_t i = 0; i < num_children; i++) {
      children.push_back(_children[i]);
    }
  }
  std::vector<std::unique_ptr<Menu>> children;
};

class NavigationController {
public:
  NavigationController(ScreenInterface &scrn);
  void dispatchPress();

  void selectNext();
  void selectPrev();

  void goHome();

  void display();

  void tick();

private:
  ScreenInterface &screen;
  Menu *currentMenu;
};
