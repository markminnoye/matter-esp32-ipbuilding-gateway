#include "matter_init.h"

#include "matter_bridge.h"

#include "esp_check.h"
#include "esp_log.h"
#include "esp_matter.h"
#include "esp_matter_attribute_utils.h"
#include "sdkconfig.h"

#include <cstring>
#include <platform/CHIPDeviceLayer.h>

namespace ipbuilding::matter {

static const char *TAG = "matter_init";

using namespace esp_matter;

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

static esp_err_t app_attribute_update_cb(attribute::callback_type_t type, uint16_t endpoint_id, uint32_t cluster_id,
                                         uint32_t attribute_id, esp_matter_attr_val_t *val, void *)
{
    return bridge::handle_attribute_update(type, endpoint_id, cluster_id, attribute_id, val);
}

esp_err_t init(registry::DeviceRegistry &registry)
{
    node::config_t node_config;
    std::strncpy(node_config.root_node.basic_information.node_label, CONFIG_MATTER_BRIDGE_NODE_LABEL,
                 sizeof(node_config.root_node.basic_information.node_label) - 1);
    node_config.root_node.basic_information.node_label[sizeof(node_config.root_node.basic_information.node_label) - 1] =
        '\0';

    node_t *node = node::create(&node_config, app_attribute_update_cb, app_identification_cb);
    ESP_RETURN_ON_FALSE(node != nullptr, ESP_FAIL, TAG, "Failed to create Matter node");

    ESP_RETURN_ON_ERROR(bridge::create_endpoints(node, registry), TAG, "Bridge endpoint creation failed");

    ESP_RETURN_ON_ERROR(esp_matter::start(app_event_cb), TAG, "esp_matter::start failed");

    bridge::start_state_sync(registry);

    ESP_LOGI(TAG, "Matter stack started");
    return ESP_OK;
}

}  // namespace ipbuilding::matter
