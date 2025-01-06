# This component copied and modified from https://github.com/nopnop2002/esp-idf-ssd1306

set(component_srcs "ssd1306.c" "ssd1306_i2c_new.c")
idf_component_register(SRCS "${component_srcs}" PRIV_REQUIRES driver INCLUDE_DIRS ".")
