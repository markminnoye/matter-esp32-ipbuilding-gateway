#include "matter_init.h"
#include "network_manager.h"

#include "esp_chip_info.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"

static const char *TAG = "app_main";

static void log_boot_banner()
{
    esp_chip_info_t chip_info = {};
    esp_chip_info(&chip_info);

    uint8_t mac[6] = {};
    esp_read_mac(mac, ESP_MAC_WIFI_STA);

    ESP_LOGI(TAG, "IPBuilding embedded gateway — E0 bootstrap");
    ESP_LOGI(TAG, "Chip: ESP32-S3, cores=%d, revision=%d", chip_info.cores, chip_info.revision);
    ESP_LOGI(TAG, "MAC: %02x:%02x:%02x:%02x:%02x:%02x", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    ESP_LOGI(TAG, "Free heap: %lu bytes (internal), %lu bytes (PSRAM)", static_cast<unsigned long>(esp_get_free_heap_size()),
             static_cast<unsigned long>(heap_caps_get_free_size(MALLOC_CAP_SPIRAM)));
}

extern "C" void app_main(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    log_boot_banner();

    ESP_ERROR_CHECK(ipbuilding::network::init());

    err = ipbuilding::matter::init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Matter init failed: %s — network stack remains active", esp_err_to_name(err));
    }

    while (true) {
        ESP_LOGI(TAG, "Idle — free heap internal=%lu psram=%lu", static_cast<unsigned long>(esp_get_free_heap_size()),
                 static_cast<unsigned long>(heap_caps_get_free_size(MALLOC_CAP_SPIRAM)));
        vTaskDelay(pdMS_TO_TICKS(30000));
    }
}
