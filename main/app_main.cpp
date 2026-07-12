#include "config/config_store.h"
#include "matter_init.h"
#include "network_manager.h"
#include "registry/device_registry.h"
#include "veldbus/udp_bus_manager.h"

#include "esp_app_desc.h"
#include "esp_chip_info.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"

static const char *TAG = "app_main";

static ipbuilding::config::ConfigStore s_config;
static ipbuilding::registry::DeviceRegistry s_registry;

static void log_boot_banner()
{
    esp_chip_info_t chip_info = {};
    esp_chip_info(&chip_info);

    uint8_t mac[6] = {};
    esp_read_mac(mac, ESP_MAC_WIFI_STA);

    const esp_app_desc_t *app_desc = esp_app_get_description();
    ESP_LOGI(TAG, "IPBuilding embedded gateway — E2 Matter MVP (v%s)", app_desc->version);
    ESP_LOGI(TAG, "Chip: ESP32-S3, cores=%d, revision=%d", chip_info.cores, chip_info.revision);
    ESP_LOGI(TAG, "MAC: %02x:%02x:%02x:%02x:%02x:%02x", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    ESP_LOGI(TAG, "Free heap: %lu bytes (internal), %lu bytes (PSRAM)",
             static_cast<unsigned long>(heap_caps_get_free_size(MALLOC_CAP_INTERNAL)),
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

    err = s_config.init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Config load failed: %s", esp_err_to_name(err));
    } else {
        s_registry.load_from_modules(s_config.modules());
    }

    err = ipbuilding::veldbus::start(s_registry);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Veldbus start failed: %s", esp_err_to_name(err));
    }

    err = ipbuilding::matter::init(s_registry);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Matter init failed: %s — network stack remains active", esp_err_to_name(err));
    }

    while (true) {
        ESP_LOGI(TAG, "Idle — free heap internal=%lu psram=%lu",
                 static_cast<unsigned long>(heap_caps_get_free_size(MALLOC_CAP_INTERNAL)),
                 static_cast<unsigned long>(heap_caps_get_free_size(MALLOC_CAP_SPIRAM)));
        vTaskDelay(pdMS_TO_TICKS(30000));
    }
}
