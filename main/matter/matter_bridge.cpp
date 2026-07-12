#include "matter_bridge.h"

#include "registry/types.h"
#include "veldbus/payloads/relay.h"
#include "veldbus/udp_bus_manager.h"

#include "esp_check.h"
#include "esp_log.h"
#include "esp_matter.h"

#include <app-common/zap-generated/cluster-enums.h>
#include <app-common/zap-generated/ids/Attributes.h>
#include <app-common/zap-generated/ids/Clusters.h>
#include <lib/support/TypeTraits.h>

#include <cstring>
#include <string>
#include <vector>

namespace ipbuilding::matter::bridge {

namespace {

constexpr const char *TAG = "matter_bridge";

using namespace esp_matter;
using namespace chip::app::Clusters;

enum class BridgeKind { RelayLight, RelayFan, RelaySwitch, Dimmer };

struct BridgedDevice {
    uint16_t endpoint_id = 0;
    BridgeKind kind = BridgeKind::RelayLight;
    registry::DeviceKey key;
    std::string name;
    // Last non-zero dimmer level. CurrentLevel stays retained while off (Matter
    // semantics: the slider shows the brightness a bare "on" restores), so the
    // restore value must match what the controller displays.
    int last_level_percent = 0;
};

std::vector<BridgedDevice> s_devices;
registry::DeviceRegistry *s_registry = nullptr;

// True while the bridge itself writes attributes (veldbus -> Matter sync).
// The attribute callback runs synchronously inside attribute::update(), under
// the chip stack lock, so a plain bool is enough to break the echo loop.
bool s_sync_in_progress = false;

// FanControl fan-mode values for FanModeSequence Off/High.
constexpr uint8_t kFanModeOff = chip::to_underlying(FanControl::FanModeEnum::kOff);
constexpr uint8_t kFanModeHigh = chip::to_underlying(FanControl::FanModeEnum::kHigh);
constexpr uint8_t kFanModeSequenceOffHigh = chip::to_underlying(FanControl::FanModeSequenceEnum::kOffHigh);

BridgedDevice *find_by_endpoint(uint16_t endpoint_id)
{
    for (auto &device : s_devices) {
        if (device.endpoint_id == endpoint_id) {
            return &device;
        }
    }
    return nullptr;
}

BridgedDevice *find_by_key(const registry::DeviceKey &key, BridgeKind *kind_out = nullptr)
{
    for (auto &device : s_devices) {
        if (device.key == key) {
            if (kind_out != nullptr) {
                *kind_out = device.kind;
            }
            return &device;
        }
    }
    return nullptr;
}

int matter_level_to_percent(uint8_t level)
{
    // Matter Level Control (lighting): 1..254; veldbus dimmer: 0..100 %.
    int percent = (static_cast<int>(level) * 100 + 127) / 254;
    if (percent < 0) {
        percent = 0;
    }
    if (percent > 100) {
        percent = 100;
    }
    return percent;
}

uint8_t percent_to_matter_level(int percent)
{
    if (percent <= 0) {
        return 1;  // CurrentLevel min for lighting; off is expressed via OnOff.
    }
    if (percent > 100) {
        percent = 100;
    }
    const int level = (percent * 254 + 50) / 100;
    return static_cast<uint8_t>(level < 1 ? 1 : level);
}

// Bridged Node endpoint with NodeLabel (design spec §7.4: `name` as user-visible
// label) and UniqueID (stable identity across endpoint-id shifts). The device
// type clusters are added onto this endpoint afterwards.
endpoint_t *create_bridged_node(node_t *node, endpoint_t *aggregator, const std::string &name,
                                const std::string &entity_id)
{
    endpoint::bridged_node::config_t config;
    endpoint_t *ep = endpoint::bridged_node::create(node, &config, ENDPOINT_FLAG_BRIDGE, nullptr);
    if (ep == nullptr) {
        return nullptr;
    }

    cluster_t *bdbi = cluster::get(ep, BridgedDeviceBasicInformation::Id);
    if (bdbi != nullptr) {
        namespace bdbi_attrs = cluster::bridged_device_basic_information;
        char label[bdbi_attrs::k_max_node_label_length + 1] = {};
        const std::string &source = name.empty() ? entity_id : name;
        strncpy(label, source.c_str(), sizeof(label) - 1);
        bdbi_attrs::attribute::create_node_label(bdbi, label, strlen(label));

        // bridged_node::create already creates UniqueID with a generated value;
        // create() would return that attribute unchanged (plus a warning), so
        // overwrite the value instead.
        char unique_id[bdbi_attrs::k_max_unique_id_length + 1] = {};
        strncpy(unique_id, entity_id.c_str(), sizeof(unique_id) - 1);
        attribute_t *uid_attr = attribute::get(bdbi, BridgedDeviceBasicInformation::Attributes::UniqueID::Id);
        if (uid_attr != nullptr) {
            esp_matter_attr_val_t uid_val = esp_matter_char_str(unique_id, strlen(unique_id));
            attribute::set_val(uid_attr, &uid_val, false);
        } else {
            bdbi_attrs::attribute::create_unique_id(bdbi, unique_id, strlen(unique_id));
        }
    } else {
        ESP_LOGW(TAG, "No BridgedDeviceBasicInformation cluster on %s — no NodeLabel", entity_id.c_str());
    }

    if (endpoint::set_parent_endpoint(ep, aggregator) != ESP_OK) {
        ESP_LOGW(TAG, "set_parent_endpoint failed for %s", entity_id.c_str());
    }
    return ep;
}

esp_err_t create_relay_endpoint(node_t *node, endpoint_t *aggregator, const registry::ModuleConfig &module,
                                const registry::ChannelConfig &channel)
{
    const std::string entity_id = registry::make_entity_id(module.ip, registry::ModuleType::Relay, channel.ch);
    endpoint_t *ep = create_bridged_node(node, aggregator, channel.name, entity_id);
    ESP_RETURN_ON_FALSE(ep != nullptr, ESP_FAIL, TAG, "Failed to create relay endpoint for %s", entity_id.c_str());

    BridgeKind kind = BridgeKind::RelayLight;
    esp_err_t err = ESP_OK;

    switch (channel.semantic_type) {
    case registry::SemanticType::Fan: {
        endpoint::fan::config_t config;
        config.fan_control.fan_mode = kFanModeOff;
        config.fan_control.fan_mode_sequence = kFanModeSequenceOffHigh;
        config.fan_control.percent_setting = nullable<uint8_t>(0);
        config.fan_control.percent_current = 0;
        err = endpoint::fan::add(ep, &config);
        kind = BridgeKind::RelayFan;
        break;
    }
    case registry::SemanticType::Switch: {
        endpoint::on_off_plug_in_unit::config_t config;
        config.on_off.on_off = false;
        err = endpoint::on_off_plug_in_unit::add(ep, &config);
        kind = BridgeKind::RelaySwitch;
        break;
    }
    case registry::SemanticType::Light:
    case registry::SemanticType::Unknown:
    default: {
        endpoint::on_off_light::config_t config;
        config.on_off.on_off = false;
        err = endpoint::on_off_light::add(ep, &config);
        kind = BridgeKind::RelayLight;
        break;
    }
    }

    ESP_RETURN_ON_ERROR(err, TAG, "Failed to add relay device type for %s", entity_id.c_str());

    const registry::DeviceKey key{module.ip, registry::ModuleType::Relay, channel.ch};
    s_devices.push_back(BridgedDevice{endpoint::get_id(ep), kind, key, channel.name});
    return ESP_OK;
}

esp_err_t create_dimmer_endpoint(node_t *node, endpoint_t *aggregator, const registry::ModuleConfig &module,
                                 const registry::ChannelConfig &channel)
{
    const std::string entity_id = registry::make_entity_id(module.ip, registry::ModuleType::Dimmer, channel.ch);
    endpoint_t *ep = create_bridged_node(node, aggregator, channel.name, entity_id);
    ESP_RETURN_ON_FALSE(ep != nullptr, ESP_FAIL, TAG, "Failed to create dimmer endpoint for %s", entity_id.c_str());

    endpoint::dimmable_light::config_t config;
    config.on_off.on_off = false;
    ESP_RETURN_ON_ERROR(endpoint::dimmable_light::add(ep, &config), TAG, "Failed to add dimmer device type for %s",
                        entity_id.c_str());

    const registry::DeviceKey key{module.ip, registry::ModuleType::Dimmer, channel.ch};
    s_devices.push_back(BridgedDevice{endpoint::get_id(ep), BridgeKind::Dimmer, key, channel.name});
    return ESP_OK;
}

void update_attribute_from_veldbus(uint16_t endpoint_id, uint32_t cluster_id, uint32_t attribute_id,
                                   esp_matter_attr_val_t val)
{
    lock::ScopedChipStackLock stack_lock(portMAX_DELAY);
    s_sync_in_progress = true;
    const esp_err_t err = attribute::update(endpoint_id, cluster_id, attribute_id, &val);
    s_sync_in_progress = false;
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "attribute::update ep=%u cluster=0x%04lx attr=0x%04lx failed: %s", endpoint_id,
                 static_cast<unsigned long>(cluster_id), static_cast<unsigned long>(attribute_id),
                 esp_err_to_name(err));
    }
}

void on_relay_state(const registry::DeviceKey &key, const registry::RelayState &state)
{
    BridgeKind kind = BridgeKind::RelayLight;
    const BridgedDevice *device = find_by_key(key, &kind);
    if (device == nullptr) {
        return;
    }
    if (state.state != "on" && state.state != "off") {
        return;  // "unknown" — keep the last Matter value.
    }
    const bool on = state.state == "on";

    ESP_LOGI(TAG, "Sync veldbus -> Matter: %s ep=%u %s", registry::make_entity_id(key.module_ip, key.type, key.channel).c_str(),
             device->endpoint_id, on ? "on" : "off");

    if (kind == BridgeKind::RelayFan) {
        update_attribute_from_veldbus(device->endpoint_id, FanControl::Id, FanControl::Attributes::FanMode::Id,
                                      esp_matter_enum8(on ? kFanModeHigh : kFanModeOff));
        update_attribute_from_veldbus(device->endpoint_id, FanControl::Id, FanControl::Attributes::PercentSetting::Id,
                                      esp_matter_nullable_uint8(nullable<uint8_t>(on ? 100 : 0)));
        update_attribute_from_veldbus(device->endpoint_id, FanControl::Id, FanControl::Attributes::PercentCurrent::Id,
                                      esp_matter_uint8(on ? 100 : 0));
    } else {
        update_attribute_from_veldbus(device->endpoint_id, OnOff::Id, OnOff::Attributes::OnOff::Id,
                                      esp_matter_bool(on));
    }
}

void on_dimmer_state(const registry::DeviceKey &key, const registry::DimmerState &state)
{
    BridgedDevice *device = find_by_key(key);
    if (device == nullptr || state.level_percent < 0) {
        return;
    }
    if (state.level_percent > 0) {
        device->last_level_percent = state.level_percent;
    }

    ESP_LOGI(TAG, "Sync veldbus -> Matter: %s ep=%u level=%d%%",
             registry::make_entity_id(key.module_ip, key.type, key.channel).c_str(), device->endpoint_id,
             state.level_percent);

    update_attribute_from_veldbus(device->endpoint_id, OnOff::Id, OnOff::Attributes::OnOff::Id,
                                  esp_matter_bool(state.level_percent > 0));
    if (state.level_percent > 0) {
        update_attribute_from_veldbus(device->endpoint_id, LevelControl::Id,
                                      LevelControl::Attributes::CurrentLevel::Id,
                                      esp_matter_nullable_uint8(nullable<uint8_t>(percent_to_matter_level(state.level_percent))));
    }
}

esp_err_t command_on_off(const BridgedDevice &device, bool on)
{
    if (device.kind == BridgeKind::Dimmer) {
        if (!on) {
            return veldbus::send_dimmer_off(device.key.module_ip.c_str(), device.key.channel);
        }
        // Bare "on": restore the level the controller's slider shows (retained
        // CurrentLevel = last non-zero level); 100% only when never dimmed yet.
        const int percent = device.last_level_percent > 0 ? device.last_level_percent : 100;
        return veldbus::send_dimmer_command(device.key.module_ip.c_str(), device.key.channel, percent);
    }

    return veldbus::send_relay_command(device.key.module_ip.c_str(), device.key.channel,
                                       on ? veldbus::payloads::RelayAction::On : veldbus::payloads::RelayAction::Off);
}

}  // namespace

esp_err_t create_endpoints(node_t *node, const registry::DeviceRegistry &registry)
{
    ESP_RETURN_ON_FALSE(node != nullptr, ESP_ERR_INVALID_ARG, TAG, "node is null");
    s_devices.clear();

    // Aggregator endpoint: makes this node a Matter bridge, so controllers show
    // every bridged endpoint as a separate accessory named by its NodeLabel.
    endpoint::aggregator::config_t aggregator_config;
    endpoint_t *aggregator = endpoint::aggregator::create(node, &aggregator_config, ENDPOINT_FLAG_NONE, nullptr);
    ESP_RETURN_ON_FALSE(aggregator != nullptr, ESP_FAIL, TAG, "Failed to create aggregator endpoint");
    ESP_LOGI(TAG, "Aggregator endpoint %u (bridge root)", endpoint::get_id(aggregator));

    for (const auto &module : registry.active_modules()) {
        for (const auto &channel : module.channels) {
            esp_err_t err = ESP_OK;
            if (module.type == registry::ModuleType::Relay) {
                err = create_relay_endpoint(node, aggregator, module, channel);
            } else if (module.type == registry::ModuleType::Dimmer) {
                err = create_dimmer_endpoint(node, aggregator, module, channel);
            } else {
                continue;  // Input modules: listener only in Gen 1, no Matter export.
            }
            ESP_RETURN_ON_ERROR(err, TAG, "Endpoint creation failed");

            const auto &device = s_devices.back();
            ESP_LOGI(TAG, "Endpoint %u <- %s \"%s\"", device.endpoint_id,
                     registry::make_entity_id(module.ip, module.type, channel.ch).c_str(), channel.name.c_str());
        }
    }

    if (s_devices.empty()) {
        ESP_LOGW(TAG, "No active relay/dimmer channels in devices.json — node has no bridged endpoints");
    } else {
        ESP_LOGI(TAG, "Created %zu veldbus-mapped endpoints", s_devices.size());
    }
    return ESP_OK;
}

esp_err_t handle_attribute_update(attribute::callback_type_t type, uint16_t endpoint_id, uint32_t cluster_id,
                                  uint32_t attribute_id, esp_matter_attr_val_t *val)
{
    if (type != attribute::PRE_UPDATE || val == nullptr || s_sync_in_progress) {
        return ESP_OK;
    }

    BridgedDevice *device = find_by_endpoint(endpoint_id);
    if (device == nullptr) {
        return ESP_OK;
    }

    esp_err_t err = ESP_OK;

    if (cluster_id == OnOff::Id && attribute_id == OnOff::Attributes::OnOff::Id) {
        ESP_LOGI(TAG, "Matter -> veldbus: ep=%u \"%s\" on_off=%d", endpoint_id, device->name.c_str(), val->val.b);
        err = command_on_off(*device, val->val.b);
    } else if (cluster_id == LevelControl::Id && attribute_id == LevelControl::Attributes::CurrentLevel::Id &&
               device->kind == BridgeKind::Dimmer) {
        // percent 0 (level 1 at cluster init, or slider minimum): nothing to send —
        // off goes via OnOff.
        const int percent = matter_level_to_percent(val->val.u8);
        if (percent > 0) {
            device->last_level_percent = percent;

            // After an Off, CHIP restores CurrentLevel to the remembered
            // brightness while OnOff stays false (bookkeeping, not a command).
            // Forwarding that write re-lit the lamp right after C…-off
            // (field test 2026-07-12) — only send while the light is on.
            esp_matter_attr_val_t on_off_val = esp_matter_invalid(nullptr);
            const bool is_on = attribute::get_val(endpoint_id, OnOff::Id, OnOff::Attributes::OnOff::Id,
                                                  &on_off_val) == ESP_OK &&
                               on_off_val.val.b;
            if (is_on) {
                ESP_LOGI(TAG, "Matter -> veldbus: ep=%u \"%s\" level=%u (%d%%)", endpoint_id, device->name.c_str(),
                         val->val.u8, percent);
                err = veldbus::send_dimmer_command(device->key.module_ip.c_str(), device->key.channel, percent);
            } else {
                ESP_LOGD(TAG, "ep=%u level restore to %d%% while off — remembered, not sent", endpoint_id, percent);
            }
        }
    } else if (cluster_id == FanControl::Id && device->kind == BridgeKind::RelayFan) {
        if (attribute_id == FanControl::Attributes::FanMode::Id) {
            const bool on = val->val.u8 != kFanModeOff;
            ESP_LOGI(TAG, "Matter -> veldbus: ep=%u \"%s\" fan_mode=%u -> %s", endpoint_id, device->name.c_str(),
                     val->val.u8, on ? "on" : "off");
            err = command_on_off(*device, on);
        } else if (attribute_id == FanControl::Attributes::PercentSetting::Id) {
            // Defensive: don't rely on the fan-control server mapping PercentSetting
            // to FanMode (assumption to confirm in the Apple Home field test).
            // Relay commands are absolute, so a duplicate command is harmless.
            nullable<uint8_t> percent(val->val.u8);
            if (!percent.is_null()) {
                const bool on = percent.value() > 0;
                ESP_LOGI(TAG, "Matter -> veldbus: ep=%u \"%s\" fan_percent=%u -> %s", endpoint_id,
                         device->name.c_str(), percent.value(), on ? "on" : "off");
                err = command_on_off(*device, on);
            }
        }
    }

    if (err != ESP_OK) {
        ESP_LOGW(TAG, "veldbus command for ep=%u failed: %s", endpoint_id, esp_err_to_name(err));
    }
    // Always accept the attribute write; veldbus poll replies re-sync the true state.
    return ESP_OK;
}

void start_state_sync(registry::DeviceRegistry &registry)
{
    s_registry = &registry;
    registry.set_relay_state_listener(on_relay_state);
    registry.set_dimmer_state_listener(on_dimmer_state);
    ESP_LOGI(TAG, "State sync active (veldbus -> Matter attributes)");
}

}  // namespace ipbuilding::matter::bridge
