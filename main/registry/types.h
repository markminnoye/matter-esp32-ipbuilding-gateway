#pragma once

#include <string>
#include <vector>

namespace ipbuilding::registry {

enum class ModuleType { Relay, Dimmer, Input };

enum class SemanticType { Light, Fan, Switch, Unknown };

struct ChannelConfig {
    int ch = 0;
    std::string name;
    std::string room;
    SemanticType semantic_type = SemanticType::Unknown;
    bool active = true;
};

struct ModuleConfig {
    std::string ip;
    ModuleType type = ModuleType::Relay;
    std::string name;
    std::vector<ChannelConfig> channels;
};

struct DeviceKey {
    std::string module_ip;
    ModuleType type = ModuleType::Relay;
    int channel = 0;

    bool operator==(const DeviceKey &other) const;
};

struct DeviceKeyHash {
    std::size_t operator()(const DeviceKey &key) const;
};

std::string make_entity_id(const std::string &module_ip, ModuleType type, int channel);
const char *module_type_to_string(ModuleType type);
ModuleType module_type_from_string(const char *value);

}  // namespace ipbuilding::registry
