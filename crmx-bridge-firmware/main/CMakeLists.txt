idf_component_register(
    SRCS "main.cc" "SettingsRegistry.cc" "DmxSwitcher.cc" "TimoInterface.cc" "ssd1106.c"
         "ui/ui_main.cc" "ui/HomePage.cc" "ui/Style.cc"
    INCLUDE_DIRS "." "./ui"
    REQUIRES esp_dmx esp32-rotary-encoder esp_lcd
    PRIV_REQUIRES esp_adc
)