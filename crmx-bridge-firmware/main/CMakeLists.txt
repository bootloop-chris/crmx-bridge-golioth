idf_component_register(
    SRCS "main.cc" "SettingsHandler.cc" "DmxSwitcher.cc" "TimoInterface.cc" "ssd1106.c" "wifi_manager.cc" "wifi_task.cc"
         "ui/ui_main.cc" "ui/HomePage.cc" "ui/Style.cc" "ui/ui_priv.cc" "ui/SettingsPage.cc" "ui/NavigationController.cc"
    INCLUDE_DIRS "." "./ui"
    REQUIRES esp_dmx esp32-rotary-encoder esp_lcd
    PRIV_REQUIRES esp_adc esp_wifi nvs_flash json console spi_flash esp_hw_support driver esp_timer
)