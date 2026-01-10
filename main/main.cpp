#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "esp_log.h"
#include "nvs_flash.h"

#include "M5Unified.h"

#include "settings.hpp"
#include "protocol/espnow_protocol.hpp"
#include "ui/ui_controller.hpp"

static const char* TAG_ = "app";

extern "C" void app_main(void)
{
    ESP_LOGI(TAG_, "Booting M5Dial remote controller...");
    
    // Initialize NVS - required for WiFi/ESP-NOW
    esp_err_t nvs_err = nvs_flash_init();
    if (nvs_err == ESP_ERR_NVS_NO_FREE_PAGES || nvs_err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG_, "Erasing NVS flash...");
        ESP_ERROR_CHECK(nvs_flash_erase());
        nvs_err = nvs_flash_init();
    }
    if (nvs_err != ESP_OK) {
        ESP_LOGE(TAG_, "NVS init failed: %s", esp_err_to_name(nvs_err));
        // Continue anyway - WiFi may not work but display will
    }
    
    // Use default settings (skip full settings store for now)
    Settings settings{};

    // Initialize M5Unified with M5Dial board
    auto cfg = M5.config();
    cfg.fallback_board = m5gfx::board_t::board_M5Dial;
    cfg.clear_display = true;
    M5.begin(cfg);
    
    // Apply brightness
    M5.Display.setBrightness(settings.ui.brightness);

    // Create protocol event queue for ESP-NOW
    QueueHandle_t proto_queue = xQueueCreate(10, sizeof(espnow::ProtoEvent));
    if (!proto_queue) {
        ESP_LOGE(TAG_, "Failed to create protocol queue");
        return;
    }

    (void)espnow::Init(proto_queue);

    // Initialize and run UI
    ui::UiController ui(proto_queue, &settings);
    ui.Init();

    while (true) {
        M5.update();
        ui.Tick();
        vTaskDelay(pdMS_TO_TICKS(16));
    }
}
