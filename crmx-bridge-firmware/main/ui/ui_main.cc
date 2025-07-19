#include "Enums.h"
#include "HomePage.h"
#include "NavigationController.h"
#include "PopupSelector.h"
#include "SettingsHandler.h"
#include "SettingsPage.h"
#include "Style.h"
#include "esp_log.h"
#include "lvgl.h"
#include "ui.h"
#include "util.h"

#define TAG "UI"

HomePageData home_page_data_from_settings() {
  auto &settings = SettingsHandler::shared();
  return HomePageData{
      .input = MenuStackData{.io_type = settings.input},
      .output = MenuStackData{.io_type = settings.output},
      .output_en = settings.output_en,
  };
}

// Actions
static void on_select_input(DmxSourceSink selection) {
  SettingsHandler::shared().input.write(selection);

  NavigationController::get().get_view_models().home.set_data(
      home_page_data_from_settings());
  NavigationController::get().dismiss_popup();
}

static void on_select_output(DmxSourceSink selection) {
  SettingsHandler::shared().output.write(selection);

  NavigationController::get().get_view_models().home.set_data(
      home_page_data_from_settings());
  NavigationController::get().dismiss_popup();
}

const PopupSelectorData<DmxSourceSink> io_selector_data = {
    .choices = arr_to_vec(ui_enum<DmxSourceSink>::as_list())};

HomePageActions home_actions = {
    .input_actions =
        MenuStackActions{
            .onclick_title =
                []() {
                  NavigationController::get().show_popup<DmxSourceSink>(
                      io_selector_data, on_select_input);
                },
            .onclick_settings =
                []() { ESP_LOGI(TAG, "Input settings clicked"); },
        },
    .output_actions =
        MenuStackActions{
            .onclick_title =
                []() {
                  NavigationController::get().show_popup<DmxSourceSink>(
                      io_selector_data, on_select_output);
                },
            .onclick_settings =
                []() { ESP_LOGI(TAG, "Output settings clicked"); },
        },
    .onclick_output_en =
        []() {
          auto &settings = SettingsHandler::shared();
          settings.output_en.write(!settings.output_en);

          NavigationController::get().get_view_models().home.set_data(
              home_page_data_from_settings());
        },
    .onclick_settings =
        []() {
          NavigationController::get().push_screen<SettingsPage>(
              NavigationController::get().get_view_models().settings);
        },
};

SettingsPageActions settings_actions = {
    .onclick_back_button = []() { NavigationController::get().pop_screen(); },
    .item_actions =
        {
            []() {
              using ProtocolT = TIMO::RF_PROTOCOL::TX_PROTOCOL_T;
              using DataT = PopupSelectorData<ProtocolT>;
              static const DataT data = DataT{
                  .choices = arr_to_vec(ui_enum<ProtocolT>::as_list()),
              };
              NavigationController::get().show_popup<ProtocolT>(
                  data, [](ProtocolT selected) {
                    SettingsHandler::shared().rf_protocol.write(selected);
                    NavigationController::get().dismiss_popup();
                  });
            },
        },
};

void ui_init() {
  // First call to styles get calls init.
  Style::get();
  UIViewModels &models = NavigationController::get().get_view_models();

  models.home.set_data(home_page_data_from_settings());
  models.settings.set_data(SettingsPageData{
      .title = "Settings",
      .items = {"TX: " +
                std::string(ui_enum<SettingsHandler::RfProtocolT>::to_string(
                    SettingsHandler::shared().rf_protocol))},
  });

  models.home.bind_actions(home_actions);
  models.settings.bind_actions(settings_actions);
}

void ui_tick() {}

void ui_deinit() {
  // TODO
}
