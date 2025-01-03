#pragma once
#include "stddef.h"
#include "stdint.h"
#include <array>
#include <type_traits>

/**
 * @brief Mask making utility
 *
 * @tparam T Type of the mask
 * @tparam start Start index of the set bits of the mask
 * @tparam len Length of the set bits of the mask
 * @return constexpr T The mask
 */
template <
    typename T, size_t start, size_t len,
    typename = std::enable_if_t<std::is_integral_v<T> && std::is_unsigned_v<T>>>
constexpr T make_mask() {
  static_assert(start <= sizeof(T) * 8);
  static_assert(start + len <= sizeof(T) * 8);
  T retval = T{0};
  for (size_t i = start; i < start + len; i++) {
    retval |= (T{1} << i);
  }
  return retval;
}

template <typename T, size_t _start, size_t _len, size_t _offset = 0>
struct Field {

  static_assert(sizeof(T) <= 64, "");

  static constexpr size_t start = _start;
  static constexpr size_t len = _len;
  static constexpr size_t offset = _offset;
  using Type = T;

  static constexpr T mask = make_mask<T, start, len>();
  static constexpr T invert_mask = ~make_mask<T, start, len>();
};

template <typename F>
constexpr typename F::Type set_field(const typename F::Type reg_val_in,
                                     const typename F::Type val) {
  typename F::Type temp = reg_val_in;
  temp &= F::invert_mask;
  temp |= (val << F::start) & F::mask;
  return temp;
}

template <typename F>
constexpr typename F::Type get_field(const typename F::Type data) {
  return (data & F::mask) >> F::start;
}

// Address is 6 bits
static constexpr uint8_t TIMO_REG_ADDR_MAX = 0b00111111;

template <uint8_t _address, typename T, size_t _reg_len> struct Register {

  static constexpr uint8_t address = _address;
  static constexpr size_t reg_len = _reg_len;
  static_assert(address <= TIMO_REG_ADDR_MAX, "Max address is 6 bits.");

  const uint8_t *const raw() const {
    return reinterpret_cast<uint8_t const *>(data.data());
  }
  uint8_t *raw() { return reinterpret_cast<uint8_t *>(data.data()); }
  constexpr size_t raw_len() const { return reg_len * sizeof(T); }

  std::array<T, reg_len> data = std::array<T, reg_len>{};

  template <typename F, typename = std::enable_if_t<!std::is_enum_v<T>>>
  inline Register<address, T, reg_len> &set(F field, T val) {
    static_assert(F::offset < reg_len);
    data[F::offset] = set_field<F>(data[F::offset], val);
    return *this;
  }

  template <typename F, typename Enum_T,
            typename = std::enable_if_t<std::is_enum_v<Enum_T>>,
            typename Underlying_T = std::underlying_type_t<Enum_T>>
  inline Register<address, Underlying_T, reg_len> &set(F field, Enum_T val) {
    static_assert(F::offset < reg_len);
    static_assert(std::is_same_v<Underlying_T, T>);
    data[F::offset] =
        set_field<F>(data[F::offset], static_cast<Underlying_T>(val));
    return *this;
  }

  template <typename F> inline T get(F field) {
    static_assert(F::offset < reg_len);
    return get_field<F>(data[F::offset]);
  }
};

// Tests
namespace {
static_assert(make_mask<uint64_t, 0, 1>() == uint64_t{0x1});
static_assert(make_mask<uint64_t, 6, 2>() == uint64_t{0xC0});
static_assert(make_mask<uint8_t, 6, 2>() == uint8_t{0xC0});
static_assert(make_mask<uint64_t, 32, 32>() == uint64_t{0xFFFFFFFF00000000});

Field<uint8_t, 0, 1> test_field_1;
Field<uint8_t, 4, 3> test_field_2;

static_assert(set_field<decltype(test_field_1)>(0x0, 0) == 0);
static_assert(set_field<decltype(test_field_1)>(0x0, 1) == 1);
static_assert(set_field<decltype(test_field_1)>(0x1, 0) == 0);
static_assert(set_field<decltype(test_field_1)>(0x1, 1) == 1);

static_assert(set_field<decltype(test_field_2)>(0x0, 1) == 0x10);
static_assert(set_field<decltype(test_field_2)>(0xFF, 0) == 0x8F);
static_assert(set_field<decltype(test_field_2)>(0xFF, 0xFFFF) == 0xFF);
static_assert(set_field<decltype(test_field_2)>(0x0, 0xFFFF) == 0x70);
} // namespace

// https://docs.lumenrad.io/timotwo/spi-interface/
namespace TIMO {
struct CONFIG : public Register<0x00, uint8_t, 1> {
  static constexpr Field<uint8_t, 7, 1> RADIO_ENABLE = {};
  static constexpr Field<uint8_t, 3, 1> SPI_RDM = {};
  static constexpr Field<uint8_t, 1, 1> RADIO_TX_RX_MODE = {};
  static constexpr Field<uint8_t, 0, 1> UART_EN = {};

  enum class RADIO_TX_RX_MODE_T : uint8_t {
    RX = 0,
    TX = 1,
  };
};

struct STATUS : public Register<0x01, uint8_t, 1> {
  static constexpr Field<uint8_t, 7, 1> UPDATE_MODE = {};
  static constexpr Field<uint8_t, 3, 1> DMX = {};
  static constexpr Field<uint8_t, 2, 1> IDENTIFY = {};
  static constexpr Field<uint8_t, 1, 1> RF_LINK = {};
  static constexpr Field<uint8_t, 0, 1> LINKED = {};
};

struct IRQ_MASK : public Register<0x02, uint8_t, 1> {
  static constexpr Field<uint8_t, 6, 1> EXTENDED_IRQ_EN = {};
  static constexpr Field<uint8_t, 5, 1> IDENTIFY_IRQ_EN = {};
  static constexpr Field<uint8_t, 4, 1> ASC_IRQ_EN = {};
  static constexpr Field<uint8_t, 3, 1> RF_LINK_IRQ_EN = {};
  static constexpr Field<uint8_t, 2, 1> DMX_CHANGED_IRQ_EN = {};
  static constexpr Field<uint8_t, 1, 1> LOST_DMX_IRQ_EN = {};
  static constexpr Field<uint8_t, 0, 1> RX_DMX_IRQ_EN = {};
};

struct IRQ_FLAGS : public Register<0x03, uint8_t, 1> {
  static constexpr Field<uint8_t, 7, 1> SPI_DEVICE_BUSY = {};
  static constexpr Field<uint8_t, 6, 1> EXTENDED_IRQ = {};
  static constexpr Field<uint8_t, 5, 1> IDENTIFY_IRQ = {};
  static constexpr Field<uint8_t, 4, 1> ASC_IRQ = {};
  static constexpr Field<uint8_t, 3, 1> RF_LINK_IRQ = {};
  static constexpr Field<uint8_t, 2, 1> DMX_CHANGED_IRQ = {};
  static constexpr Field<uint8_t, 1, 1> LOST_DMX_IRQ = {};
  static constexpr Field<uint8_t, 0, 1> RX_DMX_IRQ = {};
};

struct DMX_WINDOW : public Register<0x04, uint8_t, 4> {
  static constexpr Field<uint8_t, 0, 8, 0> WINDOW_SIZE_MSB = {};
  static constexpr Field<uint8_t, 0, 8, 1> WINDOW_SIZE_LSB = {};
  static constexpr Field<uint8_t, 0, 8, 2> START_ADDRESS_MSB = {};
  static constexpr Field<uint8_t, 0, 8, 3> START_ADDRESS_LSB = {};
};

static_assert(DMX_WINDOW::reg_len == 4, "");

struct ASC_FRAME : public Register<0x05, uint8_t, 3> {
  static constexpr Field<uint8_t, 0, 8, 0> ASC_FRAME_LENGTH_MSB = {};
  static constexpr Field<uint8_t, 0, 8, 1> ASC_FRAME_LENGTH_LSB = {};
  static constexpr Field<uint8_t, 0, 8, 2> START_CODE = {};
};

struct LINK_QUALITY : public Register<0x06, uint8_t, 1> {
  static constexpr Field<uint8_t, 0, 8> PDR = {};
};

struct DMX_SPEC : public Register<0x08, uint8_t, 8> {
  static constexpr Field<uint8_t, 0, 8, 0> N_CHANNELS_MSB = {};
  static constexpr Field<uint8_t, 0, 8, 1> N_CHANNELS_LSB = {};
  static constexpr Field<uint8_t, 0, 8, 2> INTERSLOT_TIME_MSB = {};
  static constexpr Field<uint8_t, 0, 8, 3> INTERSLOT_TIME_LSB = {};
  static constexpr Field<uint8_t, 0, 8, 4> REFRESH_PERIOD_MSB = {};
  static constexpr Field<uint8_t, 0, 8, 5> REFRESH_PERIOD_B2 = {};
  static constexpr Field<uint8_t, 0, 8, 6> REFRESH_PERIOD_B1 = {};
  static constexpr Field<uint8_t, 0, 8, 7> REFRESH_PERIOD_LSB = {};
};

struct DMX_CONTROL : public Register<0x09, uint8_t, 1> {
  static constexpr Field<uint8_t, 0, 1> ENABLE = {};
};

struct EXTENDED_IRQ_MASK : public Register<0x0A, uint8_t, 4> {
  // First 3 bytes reserved
  static constexpr Field<uint8_t, 7, 1, 3> NODE_QUERY_EN = {};
  static constexpr Field<uint8_t, 6, 1, 3> UNIV_META_CHANGED_EN = {};
  static constexpr Field<uint8_t, 5, 1, 3> RDM_DISCOVERY_EN = {};
  static constexpr Field<uint8_t, 4, 1, 3> RADIO_MUTE_EN = {};
  static constexpr Field<uint8_t, 3, 1, 3> RADIO_DISCOVERY_EN = {};
  static constexpr Field<uint8_t, 2, 1, 3> RXTX_CTS_EN = {};
  static constexpr Field<uint8_t, 1, 1, 3> RXTX_DA_EN = {};
  static constexpr Field<uint8_t, 0, 1, 3> RDM_IRQ_EN = {};
};

struct EXTENDED_IRQ_FLAGS : public Register<0x0B, uint8_t, 4> {
  // First 3 bytes reserved
  static constexpr Field<uint8_t, 7, 1, 3> NODE_QUERY = {};
  static constexpr Field<uint8_t, 6, 1, 3> UNIV_META_CHANGED = {};
  static constexpr Field<uint8_t, 5, 1, 3> RDM_DISCOVERY = {};
  static constexpr Field<uint8_t, 4, 1, 3> RADIO_MUTE = {};
  static constexpr Field<uint8_t, 3, 1, 3> RADIO_DISCOVERY = {};
  static constexpr Field<uint8_t, 2, 1, 3> RXTX_CTS = {};
  static constexpr Field<uint8_t, 1, 1, 3> RXTX_DA = {};
  static constexpr Field<uint8_t, 0, 1, 3> RDM_IRQ = {};
};

struct RF_PROTOCOL : public Register<0x0C, uint8_t, 1> {
  static constexpr Field<uint8_t, 0, 8> TX_PROTOCOL = {};

  enum class TX_PROTOCOL_T : uint8_t {
    CRMX = 0,
    W_DMX_G3 = 1,
    W_DMX_G4S = 2,
  };
};

struct DMX_SOURCE : public Register<0x0D, uint8_t, 1> {
  static constexpr Field<uint8_t, 0, 8> DATA_SOURCE = {};

  enum class DATA_SOURCE_T : uint8_t {
    NO_DATA = 0,
    UART_DMX = 1,
    WIRELESS_DMX = 2,
    SPI = 3,
    BLE = 4,
  };
};

struct LOLLIPOP : public Register<0x0E, uint8_t, 1> {
  static constexpr Field<uint8_t, 0, 8> COUNTER = {};
};

struct VERSION : public Register<0x10, uint8_t, 8> {
  static constexpr Field<uint8_t, 0, 8, 0> HW_VERSION_B4 = {};
  static constexpr Field<uint8_t, 0, 8, 1> HW_VERSION_B3 = {};
  static constexpr Field<uint8_t, 0, 8, 2> HW_VERSION_B2 = {};
  static constexpr Field<uint8_t, 0, 8, 3> HW_VERSION_B1 = {};
  static constexpr Field<uint8_t, 0, 8, 4> SW_VERSION_B4 = {};
  static constexpr Field<uint8_t, 0, 8, 5> SW_VERSION_B3 = {};
  static constexpr Field<uint8_t, 0, 8, 6> SW_VERSION_B2 = {};
  static constexpr Field<uint8_t, 0, 8, 7> SW_VERSION_B1 = {};
};

struct RF_POWER : public Register<0x11, uint8_t, 1> {
  static constexpr Field<uint8_t, 0, 8> OUTPUT_POWER = {};

  enum class OUTPUT_POWER_T : uint8_t {
    PWR_100_MW = 2,
    PWR_40_MW = 3,
    PWR_13_MW = 4,
    PWR_3_MW = 5,
  };
};

struct BLOCKED_CHANNELS : public Register<0x12, uint8_t, 11> {
  static constexpr Field<uint8_t, 0, 8, 0> BLOCK_7_0 = {};
  static constexpr Field<uint8_t, 0, 8, 1> BLOCK_15_8 = {};
  static constexpr Field<uint8_t, 0, 8, 2> BLOCK_23_16 = {};
  static constexpr Field<uint8_t, 0, 8, 3> BLOCK_31_24 = {};
  static constexpr Field<uint8_t, 0, 8, 4> BLOCK_39_32 = {};
  static constexpr Field<uint8_t, 0, 8, 5> BLOCK_47_40 = {};
  static constexpr Field<uint8_t, 0, 8, 6> BLOCK_55_48 = {};
  static constexpr Field<uint8_t, 0, 8, 7> BLOCK_63_56 = {};
  static constexpr Field<uint8_t, 0, 8, 8> BLOCK_71_64 = {};
  static constexpr Field<uint8_t, 0, 8, 9> BLOCK_79_72 = {};
  static constexpr Field<uint8_t, 0, 8, 10> BLOCK_87_80 = {};
};

struct BINDING_UID : public Register<0x20, uint8_t, 6> {
  static constexpr Field<uint8_t, 0, 8, 0> UID_MSB = {};
  static constexpr Field<uint8_t, 0, 8, 1> UID_B4 = {};
  static constexpr Field<uint8_t, 0, 8, 2> UID_B3 = {};
  static constexpr Field<uint8_t, 0, 8, 3> UID_B2 = {};
  static constexpr Field<uint8_t, 0, 8, 4> UID_B1 = {};
  static constexpr Field<uint8_t, 0, 8, 5> UID_LSB = {};
};

struct LINKING_KEY_RX : public Register<0x21, uint8_t, 10> {
  static constexpr Field<uint8_t, 0, 8, 0> KEY1 = {};
  static constexpr Field<uint8_t, 0, 8, 1> KEY2 = {};
  static constexpr Field<uint8_t, 0, 8, 2> KEY3 = {};
  static constexpr Field<uint8_t, 0, 8, 3> KEY4 = {};
  static constexpr Field<uint8_t, 0, 8, 4> KEY5 = {};
  static constexpr Field<uint8_t, 0, 8, 5> KEY6 = {};
  static constexpr Field<uint8_t, 0, 8, 6> KEY7 = {};
  static constexpr Field<uint8_t, 0, 8, 7> KEY8 = {};
  static constexpr Field<uint8_t, 0, 8, 8> MODE = {};
  static constexpr Field<uint8_t, 0, 8, 9> OUTPUT = {};

  enum class MODE_T : uint8_t {
    CRMX_CLASSIC = 0,
    CRMX_2 = 1,
  };
};

struct LINKING_KEY_TX : public Register<0x21, uint8_t, 8> {
  static constexpr Field<uint8_t, 0, 8, 0> KEY1 = {};
  static constexpr Field<uint8_t, 0, 8, 1> KEY2 = {};
  static constexpr Field<uint8_t, 0, 8, 2> KEY3 = {};
  static constexpr Field<uint8_t, 0, 8, 3> KEY4 = {};
  static constexpr Field<uint8_t, 0, 8, 4> KEY5 = {};
  static constexpr Field<uint8_t, 0, 8, 5> KEY6 = {};
  static constexpr Field<uint8_t, 0, 8, 6> KEY7 = {};
  static constexpr Field<uint8_t, 0, 8, 7> KEY8 = {};
};

struct BLE_STATUS : public Register<0x30, uint8_t, 1> {
  static constexpr Field<uint8_t, 1, 1> PIN_ACTIVE = {};
  static constexpr Field<uint8_t, 0, 1> BLE_ENABLE = {};

  enum class PIN_ACTIVE_T : uint8_t {
    ACTIVE = 0,
    INCACTIVE = 1,
  };
};

struct BLE_PIN : public Register<0x31, uint8_t, 6> {
  static constexpr Field<uint8_t, 0, 8, 0> PIN1 = {};
  static constexpr Field<uint8_t, 0, 8, 1> PIN2 = {};
  static constexpr Field<uint8_t, 0, 8, 2> PIN3 = {};
  static constexpr Field<uint8_t, 0, 8, 3> PIN4 = {};
  static constexpr Field<uint8_t, 0, 8, 4> PIN5 = {};
  static constexpr Field<uint8_t, 0, 8, 5> PIN6 = {};
};

struct BATTERY : public Register<0x32, uint8_t, 1> {
  static constexpr Field<uint8_t, 0, 8> BATTERY_LEVEL = {};
};

struct UNIVERSE_COLOR : public Register<0x33, uint8_t, 3> {
  static constexpr Field<uint8_t, 0, 8, 0> RED = {};
  static constexpr Field<uint8_t, 0, 8, 1> GREEN = {};
  static constexpr Field<uint8_t, 0, 8, 2> BLUE = {};
};

struct OEM_INFO : public Register<0x34, uint8_t, 4> {
  static constexpr Field<uint8_t, 0, 8, 0> DEVICE_MODEL_ID_MSB = {};
  static constexpr Field<uint8_t, 0, 8, 1> DEVICE_MODEL_ID_LSB = {};
  static constexpr Field<uint8_t, 0, 8, 2> MANUFACTURER_ID_MSB = {};
  static constexpr Field<uint8_t, 0, 8, 3> MANUFACTURER_ID_LSB = {};
};

struct RXTX_STATUS : public Register<0x35, uint8_t, 1> {
  static constexpr Field<uint8_t, 1, 1> CLEAR_TO_SEND = {};
  static constexpr Field<uint8_t, 0, 1> DATA_AVAILABLE = {};
};

struct DEVICE_NAME : public Register<0x36, uint8_t, 32> {
  /* ASCII string. Use data buffer directly. */
};

struct UNIVERSE_NAME : public Register<0x37, uint8_t, 16> {
  /* ASCII string. Use data buffer directly. */
};

struct INSTALLED_OPTIONS : public Register<0x3D, uint8_t, 13> {
  static constexpr Field<uint8_t, 0, 8, 0> N_OPTIONS = {};
  static constexpr Field<uint8_t, 0, 8, 1> OPTION1_MSB = {};
  static constexpr Field<uint8_t, 0, 8, 2> OPTION1_LSB = {};
  static constexpr Field<uint8_t, 0, 8, 3> OPTION2_MSB = {};
  static constexpr Field<uint8_t, 0, 8, 4> OPTION2_LSB = {};
  static constexpr Field<uint8_t, 0, 8, 5> OPTION3_MSB = {};
  static constexpr Field<uint8_t, 0, 8, 6> OPTION3_LSB = {};
  static constexpr Field<uint8_t, 0, 8, 7> OPTION4_MSB = {};
  static constexpr Field<uint8_t, 0, 8, 8> OPTION4_LSB = {};
  static constexpr Field<uint8_t, 0, 8, 9> OPTION5_MSB = {};
  static constexpr Field<uint8_t, 0, 8, 10> OPTION5_LSB = {};
  static constexpr Field<uint8_t, 0, 8, 11> OPTION6_MSB = {};
  static constexpr Field<uint8_t, 0, 8, 12> OPTION6_LSB = {};

  enum class OptionId : uint16_t {
    RDM_TX_SPI = 0x2001,
    RDM_TX_PROXY = 0x2001,
  };
};

struct PRODUCT_ID : public Register<0x3F, uint8_t, 4> {
  static constexpr Field<uint8_t, 0, 8, 0> ID1 = {};
  static constexpr Field<uint8_t, 0, 8, 1> ID2 = {};
  static constexpr Field<uint8_t, 0, 8, 2> ID3 = {};
  static constexpr Field<uint8_t, 0, 8, 3> ID4 = {};
};
} // namespace TIMO
