#include "HomePage.h"
#include "NavigationController.h"
#include "PopupSelector.h"
#include "SettingsPage.h"
#include "Style.h"
#include "esp_log.h"
#include "lvgl.h"
#include "ui.h"

#define TAG "UI"

PopupSelectorActions input_selector_actions = {
    .on_select =
        [](std::string selection) {
          // TODO: round-trip this
          auto temp =
              NavigationController::get().get_view_models().home.get_data();
          temp.input.title = selection;
          NavigationController::get().get_view_models().home.set_data(temp);

          NavigationController::get().dismiss_popup();
        },
};
PopupSelectorActions output_selector_actions = {
    .on_select = [](std::string selection) {
      // TODO: round-trip this
      auto temp = NavigationController::get().get_view_models().home.get_data();
      temp.output.title = selection;
      NavigationController::get().get_view_models().home.set_data(temp);

      NavigationController::get().dismiss_popup();
    }};

HomePageActions home_actions = {
    .input_actions =
        MenuStackActions{
            .onclick_title =
                []() {
                  NavigationController::get().show_popup<PopupSelector>(
                      NavigationController::get()
                          .get_view_models()
                          .input_selector);
                },
            .onclick_settings =
                []() { ESP_LOGI(TAG, "Input settings clicked"); },
        },
    .output_actions =
        MenuStackActions{
            .onclick_title =
                []() {
                  NavigationController::get().show_popup<PopupSelector>(
                      NavigationController::get()
                          .get_view_models()
                          .output_selector);
                },
            .onclick_settings =
                []() { ESP_LOGI(TAG, "Output settings clicked"); },
        },
    .onclick_output_en =
        []() {
          // TODO: roundtrip this
          auto temp =
              NavigationController::get().get_view_models().home.get_data();
          temp.output_en = !temp.output_en;
          NavigationController::get().get_view_models().home.set_data(temp);
        },
    .onclick_settings =
        []() {
          NavigationController::get().push_screen<SettingsPage>(
              NavigationController::get().get_view_models().settings);
        },
};

SettingsPageActions settings_actions = {
    .onclick_back_button = []() { NavigationController::get().pop_screen(); },
    .item_actions = {
        []() { ESP_LOGI(TAG, "Setting 1 clicked"); },
        []() { ESP_LOGI(TAG, "Setting 2 clicked"); },
        []() { ESP_LOGI(TAG, "Setting 3 clicked"); },
        []() { ESP_LOGI(TAG, "Setting 4 clicked"); },
        []() { ESP_LOGI(TAG, "Setting 5 clicked"); },
        []() { ESP_LOGI(TAG, "Setting 6 clicked"); },
        []() { ESP_LOGI(TAG, "Setting 7 clicked"); },
    },
};

void ui_init() {
  Style::get();
  UIViewModels &models = NavigationController::get().get_view_models();
  models.home.set_data(HomePageData{
      .input = MenuStackData{.title = "DMX"},
      .output = MenuStackData{.title = "CRMX"},
      .output_en = true,
  });
  models.settings.set_data(SettingsPageData{
      .title = "Settings",
      .items = {"Setting 1", "Setting 2", "Setting 3", "4", "5", "6", "7"},
  });
  PopupSelectorData io_selector_data = {.choices = {"DMX", "CRMX", "ARTNET"}};
  models.input_selector.set_data(io_selector_data);
  models.output_selector.set_data(io_selector_data);

  models.home.bind_actions(home_actions);
  models.settings.bind_actions(settings_actions);
  models.input_selector.bind_actions(input_selector_actions);
  models.output_selector.bind_actions(output_selector_actions);
}

void ui_tick() {}

void ui_deinit() {
  // TODO
}
