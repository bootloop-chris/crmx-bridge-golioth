#include "DmxSwitcher.h"
#include "esp_log.h"

static const char *TAG = "DMX_SWITCH";

static DmxSwitcher dmx_switcher{};

template <typename T, typename U = std::underlying_type_t<T>,
          typename = std::enable_if_t<std::is_enum_v<T>>>
static inline constexpr U underlying(T val) {
  return static_cast<U>(val);
}

DmxSwitcher &get_switcher() { return dmx_switcher; }

extern "C" static void dmx_switcher_task(void *pvParameters) {
  DmxSwitcher *_switcher = static_cast<DmxSwitcher *>(pvParameters);
  if (_switcher == nullptr) {
    return;
  }

  TickType_t xLastWakeTime;

  // Initialise the xLastWakeTime variable with the current time.
  xLastWakeTime = xTaskGetTickCount();
  while (true) {
    _switcher->dispatch();
    // Wait for the next cycle.
    vTaskDelayUntil(&xLastWakeTime, dmx_switcher_period);
  }
}

esp_err_t DmxInterface::init() {
  tx_queue = xQueueCreate(dmx_queue_size, sizeof(DmxPacket));
  if (tx_queue == nullptr) {
    ESP_LOGE(TAG, "Could not create TX queue");
    return ESP_ERR_NO_MEM;
  }
  rx_queue = xQueueCreate(dmx_queue_size, sizeof(DmxPacket));
  if (rx_queue == nullptr) {
    ESP_LOGE(TAG, "Could not create RX queue");
    return ESP_ERR_NO_MEM;
  }
  return ESP_OK;
}

esp_err_t DmxSwitcher::init() {

  esp_err_t ret = ESP_ERROR_CHECK_WITHOUT_ABORT(timo_interface.init());
  if (ret != ESP_OK) {
    return ESP_ERR_NO_MEM;
  }

  esp_err_t ret = ESP_ERROR_CHECK_WITHOUT_ABORT(onboard_interface.init());
  if (ret != ESP_OK) {
    return ESP_ERR_NO_MEM;
  }

  esp_err_t ret = ESP_ERROR_CHECK_WITHOUT_ABORT(artnet_interface.init());
  if (ret != ESP_OK) {
    return ESP_ERR_NO_MEM;
  }

  bool success = xTaskCreate(dmx_switcher_task, "dmx_switcher", 4092, this, 2,
                             &switcher_task);

  if (!success || switcher_task == nullptr) {
    ESP_LOGE(TAG, "Could not create dmx switcher task");
    return ESP_ERR_INVALID_STATE;
  }

  return ESP_OK;
}

void DmxSwitcher::dispatch() {

  uint32_t _src;
  if (xTaskNotifyWaitIndexed(underlying(Notification::set_src), 0, ULONG_MAX,
                             &_src, 0) == pdPASS) {
    active_src = static_cast<DmxSourceSink>(_src);
  }

  uint32_t _sink;
  if (xTaskNotifyWaitIndexed(underlying(Notification::set_sink), 0, ULONG_MAX,
                             &_sink, 0) == pdPASS) {
    active_sink = static_cast<DmxSourceSink>(_sink);
  }

  QueueHandle_t src_queue = get_src_queue();
  QueueHandle_t sink_queue = get_sink_queue();

  if (src_queue == nullptr || sink_queue == nullptr) {
    return;
  }

  DmxPacket packet;
  if (xQueueReceive(src_queue, &packet, 0)) {
    xQueueOverwrite(sink_queue, &packet);
  }
}

esp_err_t DmxSwitcher::set_src(const DmxSourceSink source) {
  if (switcher_task == nullptr) {
    return ESP_ERR_INVALID_STATE;
  }
  xTaskNotifyIndexed(switcher_task, underlying(Notification::set_src),
                     underlying(source), eSetValueWithOverwrite);
  return ESP_OK;
}

esp_err_t DmxSwitcher::set_sink(const DmxSourceSink sink) {
  if (switcher_task == nullptr) {
    return ESP_ERR_INVALID_STATE;
  }
  xTaskNotifyIndexed(switcher_task, underlying(Notification::set_sink),
                     underlying(sink), eSetValueWithOverwrite);
  return ESP_OK;
}
