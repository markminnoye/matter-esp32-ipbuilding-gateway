#pragma once

#include "types.h"
#include "span.h"

#include <functional>
#include <optional>
#include <span>
#include <string>
#include <unordered_map>
#include <vector>

namespace ipbuilding::registry {

using ipbuilding::Span;

struct RelayState {
    std::string state = "unknown";
    std::string state_code;
};

struct DimmerState {
    int level_percent = -1;
    std::string internal_value_code;
};

struct ButtonEvent {
    std::string action;
    std::string id_hex;
};

class DeviceRegistry {
public:
    using RelayStateListener = std::function<void(const DeviceKey &, const RelayState &)>;
    using DimmerStateListener = std::function<void(const DeviceKey &, const DimmerState &)>;

    void load_from_modules(const std::vector<ModuleConfig> &modules);
    void handle_packet(const std::string &src_ip, Span<const uint8_t> data);

    // Listeners fire on state *change* only, from the udp_bus_rx task context.
    void set_relay_state_listener(RelayStateListener listener) { relay_listener_ = std::move(listener); }
    void set_dimmer_state_listener(DimmerStateListener listener) { dimmer_listener_ = std::move(listener); }

    const std::vector<ModuleConfig> &modules() const { return modules_; }
    const std::vector<ModuleConfig> &active_modules() const { return active_modules_; }

    std::optional<RelayState> get_relay_state(const DeviceKey &key) const;
    std::optional<DimmerState> get_dimmer_state(const DeviceKey &key) const;

private:
    void handle_relay(const std::string &module_ip, Span<const uint8_t> data);
    void handle_dimmer(const std::string &module_ip, Span<const uint8_t> data);
    void handle_input(const std::string &module_ip, Span<const uint8_t> data);

    RelayStateListener relay_listener_;
    DimmerStateListener dimmer_listener_;
    std::vector<ModuleConfig> modules_;
    std::vector<ModuleConfig> active_modules_;
    std::unordered_map<std::string, ModuleType> module_types_;
    std::unordered_map<DeviceKey, RelayState, DeviceKeyHash> relay_states_;
    std::unordered_map<DeviceKey, DimmerState, DeviceKeyHash> dimmer_states_;
    std::unordered_map<std::string, int> dimmer_last_channel_;
};

}  // namespace ipbuilding::registry
