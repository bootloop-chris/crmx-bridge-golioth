#pragma once

#include "stddef.h"
#include "stdint.h"
#include <cmath>

template <typename T> static constexpr T clamp(T val, T min, T max) {
  if (val < min) {
    return min;
  } else if (val > max) {
    return max;
  }
  return val;
}

template <typename T> static constexpr T abs(T val) {
  if (val < T{0}) {
    return T{-1} * val;
  }
  return val;
}

struct RGBColor {
  uint8_t red;
  uint8_t green;
  uint8_t blue;

  bool operator==(const RGBColor &c) const {
    return red == c.red && green == c.green && blue == c.blue;
  }

  bool operator!=(const RGBColor &c) const { return !(*this == c); }

  static constexpr RGBColor Green() {
    return RGBColor{
        .red = 0,
        .green = 0xFF,
        .blue = 0,
    };
  }

  static constexpr RGBColor Red() {
    return RGBColor{
        .red = 0xFF,
        .green = 0,
        .blue = 0,
    };
  }

  static constexpr RGBColor Blue() {
    return RGBColor{
        .red = 0,
        .green = 0,
        .blue = 0xFF,
    };
  }
};

struct HSLColor {
  float hue;
  float sat;
  float lightness;

public:
  RGBColor to_rgb() const {
    float h = clamp(hue / 360, 0.f, 1.f);
    float s = clamp(sat, 0.f, 1.f);
    float v = clamp(lightness, 0.f, 1.f);

    int i = floor(h * 6);
    float f = h * 6 - i;
    float p = v * (1 - s);
    float q = v * (1 - f * s);
    float t = v * (1 - (1 - f) * s);

    RGBColor color;

    switch (i % 6) {
    case 0:
      color.red = v * 255, color.green = t * 255, color.blue = p * 255;
      break;
    case 1:
      color.red = q * 255, color.green = v * 255, color.blue = p * 255;
      break;
    case 2:
      color.red = p * 255, color.green = v * 255, color.blue = t * 255;
      break;
    case 3:
      color.red = p * 255, color.green = q * 255, color.blue = v * 255;
      break;
    case 4:
      color.red = t * 255, color.green = p * 255, color.blue = v * 255;
      break;
    case 5:
      color.red = v * 255, color.green = p * 255, color.blue = q * 255;
      break;
    }

    return color;
  }
};
