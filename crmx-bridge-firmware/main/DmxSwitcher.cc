#include "DmxSwitcher.h"
#include "esp_log.h"

static const char *TAG = "DMX_SWITCH";

static DmxSwitcher dmx_switcher{};

template <typename T, typename U = std::underlying_type_t<T>,
          typename = std::enable_if_t<std::is_enum_v<T>>>
static inline constexpr U underlying(T val) {
  return static_cast<U>(val);
}

DmxSwitcher &DmxSwitcher::get_switcher() { return dmx_switcher; }

extern "C" {
static void dmx_switcher_task(void *pvParameters) {
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
    vTaskDelayUntil(&xLastWakeTime, dmx_switcher_period_min);
  }
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

void DmxInterface::deinit() {
  vQueueDelete(tx_queue);
  vQueueDelete(rx_queue);
}

esp_err_t DmxSwitcher::init() {

  esp_err_t ret = ESP_ERROR_CHECK_WITHOUT_ABORT(timo_interface.init());
  if (ret != ESP_OK) {
    return ESP_ERR_NO_MEM;
  }

  ret = ESP_ERROR_CHECK_WITHOUT_ABORT(onboard_interface.init());
  if (ret != ESP_OK) {
    return ESP_ERR_NO_MEM;
  }

  ret = ESP_ERROR_CHECK_WITHOUT_ABORT(artnet_interface.init());
  if (ret != ESP_OK) {
    return ESP_ERR_NO_MEM;
  }

  inout_mutex = xSemaphoreCreateMutex();
  if (inout_mutex == nullptr) {
    ESP_LOGE(TAG, "Could not create mutex!");
    return ESP_ERR_NO_MEM;
  }

  bool success = xTaskCreate(dmx_switcher_task, "dmx_switcher", 4092, this, 2,
                             &switcher_task);

  if (!success || switcher_task == nullptr) {
    ESP_LOGE(TAG, "Could not create dmx switcher task");
    return ESP_ERR_INVALID_STATE;
  }

  SettingsHandler::shared().add_delegate(this);

  return ESP_OK;
}

void DmxSwitcher::deinit() {
  vTaskDelete(switcher_task);
  timo_interface.deinit();
  onboard_interface.deinit();
  artnet_interface.deinit();
  SettingsHandler::shared().remove_delegate(this);
}

void DmxSwitcher::dispatch() {

  xSemaphoreTake(inout_mutex, dmx_switcher_period_max);
  QueueHandle_t src_queue = get_src_queue();
  QueueHandle_t sink_queue = get_sink_queue();
  bool _output_en = output_en;
  xSemaphoreGive(inout_mutex);

  if (src_queue == nullptr || sink_queue == nullptr) {
    return;
  }

  DmxPacket packet;
  if (xQueueReceive(src_queue, &packet, dmx_switcher_period_max) &&
      _output_en) {
    xQueueOverwrite(sink_queue, &packet);
  }
}

esp_err_t DmxSwitcher::set_src_sink(const DmxSourceSink src,
                                    const DmxSourceSink sink) {
  bool taken = xSemaphoreTake(inout_mutex, pdMS_TO_TICKS(2));
  if (taken) {
    active_src = src;
    active_sink = sink;
    if (xSemaphoreGive(inout_mutex) != pdPASS) {
      return ESP_FAIL;
    }
  } else {
    return ESP_ERR_TIMEOUT;
  }

  return ESP_OK;
}

esp_err_t DmxSwitcher::set_output_en(const bool en) {
  bool taken = xSemaphoreTake(inout_mutex, pdMS_TO_TICKS(2));
  if (taken) {
    output_en = en;
    if (xSemaphoreGive(inout_mutex) != pdPASS) {
      return ESP_FAIL;
    }
  } else {
    return ESP_ERR_TIMEOUT;
  }

  return ESP_OK;
}

void DmxSwitcher::on_settings_update(const SettingsHandler &settings) {
  esp_err_t err = set_src_sink(settings.input, settings.output);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to set source/sink on settings update.");
  }
  err = set_output_en(settings.output_en);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to set output enable on settings update.");
  }
}
