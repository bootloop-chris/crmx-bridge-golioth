idf_component_register(SRCS "DmxSwitcher.cc" "TimoInterface.cc" "main.cpp"
                    INCLUDE_DIRS "."
                    REQUIRES esp_dmx
                    PRIV_REQUIRES esp_adc)