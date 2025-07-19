#pragma once

#include "../DmxSwitcher.h"
#include "../TimoReg.h"
#include "util.h"
#include <optional>
#include <string>

// Fileprivate consts
namespace {
static constexpr char const *unknown_enum_str = "Unknown";
} // namespace

template <typename> struct ui_enum {
  static constexpr bool is_ui_enum = false;
};

template <typename T>
using enable_if_enum_or_string_t =
    std::enable_if_t<ui_enum<T>::is_ui_enum || std::is_same_v<T, std::string>>;

constexpr const char *enum_or_string_to_c_str(const std::string choice) {
  return choice.c_str();
}

template <typename EnumT,
          typename = std::enable_if_t<ui_enum<EnumT>::is_ui_enum>>
constexpr const char *enum_or_string_to_c_str(const EnumT choice) {
  return ui_enum<EnumT>::to_string(choice);
}

// UI enum specializations.

template <> struct ui_enum<TIMO::CONFIG::RADIO_TX_RX_MODE_T> {
  static constexpr bool is_ui_enum = true;

  static constexpr std::array<TIMO::CONFIG::RADIO_TX_RX_MODE_T, 2> as_list() {
    return {TIMO::CONFIG::RADIO_TX_RX_MODE_T::RX,
            TIMO::CONFIG::RADIO_TX_RX_MODE_T::TX};
  }

  static constexpr char const *
  to_string(const TIMO::CONFIG::RADIO_TX_RX_MODE_T mode) {
    switch (mode) {
    case TIMO::CONFIG::RADIO_TX_RX_MODE_T::RX:
      return "RX";
    case TIMO::CONFIG::RADIO_TX_RX_MODE_T::TX:
      return "TX";
    default:
      return unknown_enum_str;
    }
  }

  static constexpr std::optional<TIMO::CONFIG::RADIO_TX_RX_MODE_T>
  from_string(const std::string str) {
    if (str == "RX") {
      return {TIMO::CONFIG::RADIO_TX_RX_MODE_T::RX};
    } else if (str == "TX") {
      return {TIMO::CONFIG::RADIO_TX_RX_MODE_T::TX};
    }
    return {};
  }
};

template <> struct ui_enum<TIMO::RF_PROTOCOL::TX_PROTOCOL_T> {
  static constexpr bool is_ui_enum = true;

  static constexpr std::array<TIMO::RF_PROTOCOL::TX_PROTOCOL_T, 3> as_list() {
    return {TIMO::RF_PROTOCOL::TX_PROTOCOL_T::CRMX,
            TIMO::RF_PROTOCOL::TX_PROTOCOL_T::W_DMX_G3,
            TIMO::RF_PROTOCOL::TX_PROTOCOL_T::W_DMX_G4S};
  }

  static constexpr char const *
  to_string(const TIMO::RF_PROTOCOL::TX_PROTOCOL_T protocol) {
    switch (protocol) {
    case TIMO::RF_PROTOCOL::TX_PROTOCOL_T::CRMX:
      return "CRMX";
    case TIMO::RF_PROTOCOL::TX_PROTOCOL_T::W_DMX_G3:
      return "WDMX G3";
    case TIMO::RF_PROTOCOL::TX_PROTOCOL_T::W_DMX_G4S:
      return "WDMX G4S";
    default:
      return unknown_enum_str;
    }
  }

  static constexpr std::optional<TIMO::RF_PROTOCOL::TX_PROTOCOL_T>
  from_string(const std::string str) {
    if (str == "CRMX") {
      return {TIMO::RF_PROTOCOL::TX_PROTOCOL_T::CRMX};
    } else if (str == "WDMX G3") {
      return {TIMO::RF_PROTOCOL::TX_PROTOCOL_T::W_DMX_G3};
    } else if (str == "WDMX G4S") {
      return {TIMO::RF_PROTOCOL::TX_PROTOCOL_T::W_DMX_G4S};
    }
    return {};
  }
};

template <> struct ui_enum<TIMO::RF_POWER::OUTPUT_POWER_T> {
  static constexpr bool is_ui_enum = true;

  static constexpr std::array<TIMO::RF_POWER::OUTPUT_POWER_T, 4> as_list() {
    return {TIMO::RF_POWER::OUTPUT_POWER_T::PWR_100_MW,
            TIMO::RF_POWER::OUTPUT_POWER_T::PWR_40_MW,
            TIMO::RF_POWER::OUTPUT_POWER_T::PWR_13_MW,
            TIMO::RF_POWER::OUTPUT_POWER_T::PWR_3_MW};
  }

  static constexpr char const *
  to_string(const TIMO::RF_POWER::OUTPUT_POWER_T pwr) {
    switch (pwr) {
    case TIMO::RF_POWER::OUTPUT_POWER_T::PWR_100_MW:
      return "100mW";
    case TIMO::RF_POWER::OUTPUT_POWER_T::PWR_40_MW:
      return "40mW";
    case TIMO::RF_POWER::OUTPUT_POWER_T::PWR_13_MW:
      return "13mW";
    case TIMO::RF_POWER::OUTPUT_POWER_T::PWR_3_MW:
      return "3mW";
    default:
      return unknown_enum_str;
    }
  }

  static constexpr std::optional<TIMO::RF_POWER::OUTPUT_POWER_T>
  from_string(const std::string str) {
    if (str == "100mW") {
      return {TIMO::RF_POWER::OUTPUT_POWER_T::PWR_100_MW};
    } else if (str == "40mW") {
      return {TIMO::RF_POWER::OUTPUT_POWER_T::PWR_40_MW};
    } else if (str == "13mW") {
      return {TIMO::RF_POWER::OUTPUT_POWER_T::PWR_13_MW};
    } else if (str == "3mW") {
      return {TIMO::RF_POWER::OUTPUT_POWER_T::PWR_3_MW};
    }
    return {};
  }
};

template <> struct ui_enum<TIMO::LINKING_KEY_RX::MODE_T> {
  static constexpr bool is_ui_enum = true;

  static constexpr std::array<TIMO::LINKING_KEY_RX::MODE_T, 2> as_list() {
    return {TIMO::LINKING_KEY_RX::MODE_T::CRMX_CLASSIC,
            TIMO::LINKING_KEY_RX::MODE_T::CRMX_2};
  }

  static constexpr char const *
  to_string(const TIMO::LINKING_KEY_RX::MODE_T mode) {
    switch (mode) {
    case TIMO::LINKING_KEY_RX::MODE_T::CRMX_CLASSIC:
      return "CRMX Classic";
    case TIMO::LINKING_KEY_RX::MODE_T::CRMX_2:
      return "CRMX 2";
    default:
      return unknown_enum_str;
    }
  }

  static constexpr std::optional<TIMO::LINKING_KEY_RX::MODE_T>
  from_string(std::string str) {
    if (str == "CRMX Classic") {
      return {TIMO::LINKING_KEY_RX::MODE_T::CRMX_CLASSIC};
    } else if (str == "CRMX 2") {
      return {TIMO::LINKING_KEY_RX::MODE_T::CRMX_2};
    }
    return {};
  }
};

template <> struct ui_enum<DmxSourceSink> {
  static constexpr bool is_ui_enum = true;

  static constexpr std::array<DmxSourceSink, 4> as_list() {
    return {DmxSourceSink::none, DmxSourceSink::timo, DmxSourceSink::onboard,
            DmxSourceSink::artnet};
  }

  static constexpr char const *to_string(const DmxSourceSink src_sink) {
    switch (src_sink) {
    case DmxSourceSink::none:
      return "None";
    case DmxSourceSink::timo:
      return "CRMX";
    case DmxSourceSink::onboard:
      return "DMX";
    case DmxSourceSink::artnet:
      return "ARTNET";
    default:
      return unknown_enum_str;
    }
  }

  static constexpr std::optional<DmxSourceSink>
  from_string(const std::string str) {
    if (str == "None") {
      return {DmxSourceSink::none};
    } else if (str == "CRMX") {
      return {DmxSourceSink::timo};
    } else if (str == "DMX") {
      return {DmxSourceSink::onboard};
    } else if (str == "ARTNET") {
      return {DmxSourceSink::artnet};
    }
    return {};
  }
};
