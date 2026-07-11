#include "matter_init.h"

#include "esp_check.h"
#include "esp_log.h"
#include "esp_matter.h"
#include "esp_matter_attribute_utils.h"

#include <platform/CHIPDeviceLayer.h>

namespace ipbuilding::matter {

static const char *TAG = "matter_init";

using namespace esp_matter;
using namespace esp_matter::endpoint;

static void app_event_cb(const chip::DeviceLayer::ChipDeviceEvent *event, intptr_t)
{
    switch (event->Type) {
    case chip::DeviceLayer::DeviceEventType::kCommissioningComplete:
        ESP_LOGI(TAG, "Commissioning complete");
        break;
    case chip::DeviceLayer::DeviceEventType::kCommissioningWindowOpened:
        ESP_LOGI(TAG, "Commissioning window opened");
        break;
    case chip::DeviceLayer::DeviceEventType::kInterfaceIpAddressChanged:
        ESP_LOGI(TAG, "Interface IP address changed");
        break;
    default:
        break;
    }
}

static esp_err_t app_identification_cb(identification::callback_type_t, uint16_t, uint8_t, uint8_t, void *)
{
    return ESP_OK;
}

static esp_err_t app_attribute_update_cb(attribute::callback_type_t, uint16_t, uint32_t, uint32_t,
                                         esp_matter_attr_val_t *, void *)
{
    return ESP_OK;
}

esp_err_t init()
{
    node::config_t node_config;
    node_t *node = node::create(&node_config, app_attribute_update_cb, app_identification_cb);
    ESP_RETURN_ON_FALSE(node != nullptr, ESP_FAIL, TAG, "Failed to create Matter node");

    on_off_light::config_t light_config;
    light_config.on_off.on_off = false;

    endpoint_t *endpoint = on_off_light::create(node, &light_config, ENDPOINT_FLAG_NONE, nullptr);
    ESP_RETURN_ON_FALSE(endpoint != nullptr, ESP_FAIL, TAG, "Failed to create On/Off Light endpoint");

    const uint16_t endpoint_id = endpoint::get_id(endpoint);
    ESP_LOGI(TAG, "Placeholder On/Off Light endpoint_id=%u (E2 will add veldbus-mapped endpoints)", endpoint_id);

    ESP_RETURN_ON_ERROR(esp_matter::start(app_event_cb), TAG, "esp_matter::start failed");

    ESP_LOGI(TAG, "Matter stack started");
    return ESP_OK;
}

}  // namespace ipbuilding::matter
