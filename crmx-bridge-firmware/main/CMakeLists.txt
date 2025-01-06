idf_component_register(SRCS "SettingsRegistry.cc" "DmxSwitcher.cc" "TimoInterface.cc" "main.cpp"
                    INCLUDE_DIRS "."
                    REQUIRES esp_dmx ssd1306 esp32-rotary-encoder
                    PRIV_REQUIRES esp_adc)