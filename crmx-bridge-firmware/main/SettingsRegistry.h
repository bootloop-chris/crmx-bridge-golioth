#pragma once

#include "Color.h"
#include "freertos/FreeRTOS.h"

class Settings {
public:
  static Settings &get_settings();

protected:
  SemaphoreHandle_t settings_smphr;
};
