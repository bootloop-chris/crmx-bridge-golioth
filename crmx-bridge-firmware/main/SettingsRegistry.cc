
#include "SettingsRegistry.h"

static Settings settings{};

Settings &Settings::get_settings() { return settings; }
