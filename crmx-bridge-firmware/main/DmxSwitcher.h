#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include <array>

void dmx_switcher_task();

enum class DmxSourceSink : uint32_t {
  none,
  timo,
  onboard,
  artnet,
};

static constexpr size_t dmx_packet_size = 512;
static constexpr TickType_t dmx_switcher_period_min = pdMS_TO_TICKS(1);
static constexpr TickType_t dmx_switcher_period_max = pdMS_TO_TICKS(5);

struct DmxPacket {
  DmxSourceSink source;
  struct PACKED_ATTR {
    uint8_t start_code;
    std::array<uint8_t, dmx_packet_size> data;
    static constexpr size_t size() { return dmx_packet_size + 1; }
  } full_packet;
};
static_assert(sizeof(decltype(DmxPacket::full_packet)) == dmx_packet_size + 1);

class DmxSwitcher;

class DmxInterface {
public:
  static constexpr size_t dmx_queue_size = 1;

  void send(const DmxPacket &packet) {
    if (tx_queue == nullptr) {
      return;
    }
    xQueueOverwrite(tx_queue, &packet);
  }
  bool recieve(DmxPacket &packet, const TickType_t timeout) {
    if (rx_queue == nullptr) {
      return false;
    }
    return xQueueReceive(rx_queue, &packet, timeout) == pdTRUE;
  }

  esp_err_t init();
  void deinit();

protected:
  QueueHandle_t tx_queue;
  QueueHandle_t rx_queue;

  friend class DmxSwitcher;
};

class DmxSwitcher {

public:
  esp_err_t init();
  void deinit();
  void dispatch();

  // These functions should be thread-safe.
  esp_err_t set_src_sink(const DmxSourceSink src, const DmxSourceSink sink);

  static DmxSwitcher &get_switcher();

  DmxInterface &get_timo_interface() { return timo_interface; }
  DmxInterface &get_onboard_interface() { return onboard_interface; }
  DmxInterface &get_artnet_interface() { return artnet_interface; }

protected:
  /* Custom notifications start at 1. */
  //   enum class Notification : uint32_t {
  //     set_src = 1,
  //     set_sink = 2,
  //   };

  QueueHandle_t get_src_queue() {
    switch (active_src) {
    case DmxSourceSink::timo:
      return timo_interface.tx_queue;
    case DmxSourceSink::onboard:
      return onboard_interface.tx_queue;
    case DmxSourceSink::artnet:
      return artnet_interface.tx_queue;
    case DmxSourceSink::none:
    default:
      return nullptr;
    }
  }

  QueueHandle_t get_sink_queue() {
    switch (active_sink) {
    case DmxSourceSink::timo:
      return timo_interface.rx_queue;
    case DmxSourceSink::onboard:
      return onboard_interface.rx_queue;
    case DmxSourceSink::artnet:
      return artnet_interface.rx_queue;
    case DmxSourceSink::none:
    default:
      return nullptr;
    }
  }

  TaskHandle_t switcher_task;

  SemaphoreHandle_t src_sink_mutex_handle;
  DmxSourceSink active_src;
  DmxSourceSink active_sink;

  DmxInterface timo_interface;
  DmxInterface onboard_interface;
  DmxInterface artnet_interface;
};
