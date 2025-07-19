#pragma once

#include "limits.h"
#include <array>
#include <vector>

template <typename T, size_t Sz>
static constexpr std::vector<T> arr_to_vec(const std::array<T, Sz> &arr) {
  return std::vector<T>(arr.begin(), arr.end());
}

enum class DmxSourceSink : uint32_t {
  none,
  timo,
  onboard,
  artnet,
};
