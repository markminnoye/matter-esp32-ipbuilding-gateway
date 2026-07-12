#include "device_registry.h"

#include "veldbus/payloads/dimmer.h"
#include "veldbus/payloads/input.h"
#include "veldbus/payloads/relay.h"

#include "esp_log.h"

namespace ipbuilding::registry {

namespace {
constexpr const char *TAG = "device_registry";
}

void DeviceRegistry::load_from_modules(const std::vector<ModuleConfig> &modules)
{
    modules_ = modules;
    active_modules_.clear();
    module_types_.clear();
    module_managed_.clear();
    relay_states_.clear();
    dimmer_states_.clear();
    dimmer_last_channel_.clear();

    for (const auto &module : modules_) {
        module_types_[module.ip] = module.type;
        module_managed_[module.ip] = module.managed;

        ModuleConfig active_module = module;
        active_module.channels.clear();

        for (const auto &channel : module.channels) {
            if (!channel.active) {
                continue;
            }
            active_module.channels.push_back(channel);

            const DeviceKey key{module.ip, module.type, channel.ch};
            if (module.type == ModuleType::Relay) {
                relay_states_.emplace(key, RelayState{});
            } else if (module.type == ModuleType::Dimmer) {
                dimmer_states_.emplace(key, DimmerState{});
                dimmer_last_channel_[module.ip] = channel.ch;
            }
        }

        if (!module.managed) {
            continue;
        }

        // Input modules have no config channels but must still be polled (I0000) when
        // managed so the IP1100 stays in slave mode and sends unsolicited B-…E events.
        if (!active_module.channels.empty() || module.type == ModuleType::Input) {
            active_modules_.push_back(std::move(active_module));
        }
    }

    ESP_LOGI(TAG, "Loaded %zu modules (%zu in poll loop)", modules_.size(), active_modules_.size());
}

void DeviceRegistry::handle_packet(const std::string &src_ip, Span<const uint8_t> data)
{
    const auto it = module_types_.find(src_ip);
    if (it == module_types_.end()) {
        ESP_LOGW(TAG, "RX reply from unconfigured module %s (not in devices.json)", src_ip.c_str());
        return;
    }

    switch (it->second) {
    case ModuleType::Relay:
        handle_relay(src_ip, data);
        break;
    case ModuleType::Dimmer:
        handle_dimmer(src_ip, data);
        break;
    case ModuleType::Input:
        handle_input(src_ip, data);
        break;
    default:
        break;
    }
}

void DeviceRegistry::handle_relay(const std::string &module_ip, Span<const uint8_t> data)
{
    if (veldbus::payloads::is_relay_poll_echo(data)) {
        ESP_LOGD(TAG, "REPLY relay %s poll-ack (P000000000 — no channel state on poll)", module_ip.c_str());
        return;
    }

    const auto status = veldbus::payloads::decode_relay_status(data);
    if (!status) {
        ESP_LOGW(TAG, "RX reply from relay %s: undecoded payload", module_ip.c_str());
        return;
    }

    const DeviceKey key{module_ip, ModuleType::Relay, status->channel};
    const auto old_it = relay_states_.find(key);
    const std::string old_state = old_it != relay_states_.end() ? old_it->second.state : "unknown";
    RelayState next{status->state, status->state_code};
    relay_states_[key] = next;

    ESP_LOGI(TAG, "REPLY relay %s ch%d state=%s (code=%s)", module_ip.c_str(), status->channel, next.state.c_str(),
             next.state_code.c_str());
    if (old_state != next.state) {
        ESP_LOGI(TAG, "REPLY relay %s ch%d changed: %s -> %s", module_ip.c_str(), status->channel, old_state.c_str(),
                 next.state.c_str());
        if (relay_listener_) {
            relay_listener_(key, next);
        }
    }
}

void DeviceRegistry::handle_dimmer(const std::string &module_ip, Span<const uint8_t> data)
{
    if (veldbus::payloads::is_dimmer_idle_poll(data)) {
        ESP_LOGD(TAG, "REPLY dimmer %s idle heartbeat (I0154999 — no level on poll)", module_ip.c_str());
        return;
    }

    const auto status = veldbus::payloads::decode_dimmer_status(data);
    if (!status) {
        ESP_LOGW(TAG, "RX reply from dimmer %s: undecoded payload", module_ip.c_str());
        return;
    }

    int channel = status->channel;
    if (channel < 0) {
        const auto last = dimmer_last_channel_.find(module_ip);
        channel = last != dimmer_last_channel_.end() ? last->second : 0;
    }

    const DeviceKey key{module_ip, ModuleType::Dimmer, channel};
    const auto old_it = dimmer_states_.find(key);
    const int old_level = old_it != dimmer_states_.end() ? old_it->second.level_percent : -1;
    DimmerState next{status->level_percent, status->internal_value_code};
    dimmer_states_[key] = next;

    ESP_LOGI(TAG, "REPLY dimmer %s ch%d level=%d%% (code=%s)", module_ip.c_str(), channel, next.level_percent,
             next.internal_value_code.c_str());
    if (old_level != next.level_percent) {
        ESP_LOGI(TAG, "REPLY dimmer %s ch%d changed: %d -> %d%%", module_ip.c_str(), channel, old_level,
                 next.level_percent);
        if (dimmer_listener_) {
            dimmer_listener_(key, next);
        }
    }
}

void DeviceRegistry::handle_input(const std::string &module_ip, Span<const uint8_t> data)
{
    const auto managed_it = module_managed_.find(module_ip);
    if (managed_it != module_managed_.end() && !managed_it->second) {
        ESP_LOGD(TAG, "RX reply from unmanaged input %s (ignored)", module_ip.c_str());
        return;
    }

    const auto event = veldbus::payloads::decode_input_event(data);
    if (!event) {
        ESP_LOGW(TAG, "RX reply from input %s: undecoded payload", module_ip.c_str());
        return;
    }

    if (event->type == veldbus::payloads::InputEventType::IdleStatus) {
        ESP_LOGD(TAG, "REPLY input %s idle heartbeat (no button activity)", module_ip.c_str());
        return;
    }

    if (event->type == veldbus::payloads::InputEventType::Press) {
        ESP_LOGI(TAG, "REPLY input %s button %s: press", module_ip.c_str(), event->id_hex.c_str());
    } else if (event->type == veldbus::payloads::InputEventType::Release) {
        ESP_LOGI(TAG, "REPLY input %s button %s: release", module_ip.c_str(), event->id_hex.c_str());
    }
}

std::optional<RelayState> DeviceRegistry::get_relay_state(const DeviceKey &key) const
{
    const auto it = relay_states_.find(key);
    if (it == relay_states_.end()) {
        return std::nullopt;
    }
    return it->second;
}

std::optional<DimmerState> DeviceRegistry::get_dimmer_state(const DeviceKey &key) const
{
    const auto it = dimmer_states_.find(key);
    if (it == dimmer_states_.end()) {
        return std::nullopt;
    }
    return it->second;
}

}  // namespace ipbuilding::registry
