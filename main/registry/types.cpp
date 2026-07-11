#include "types.h"

#include <functional>
#include <string_view>

namespace ipbuilding::registry {

bool DeviceKey::operator==(const DeviceKey &other) const
{
    return module_ip == other.module_ip && type == other.type && channel == other.channel;
}

std::size_t DeviceKeyHash::operator()(const DeviceKey &key) const
{
    return std::hash<std::string>{}(key.module_ip) ^ (static_cast<std::size_t>(key.type) << 8) ^
           static_cast<std::size_t>(key.channel);
}

std::string make_entity_id(const std::string &module_ip, ModuleType type, int channel)
{
    return module_ip + ":" + module_type_to_string(type) + ":" + std::to_string(channel);
}

const char *module_type_to_string(ModuleType type)
{
    switch (type) {
    case ModuleType::Relay:
        return "relay";
    case ModuleType::Dimmer:
        return "dimmer";
    case ModuleType::Input:
        return "input";
    }
    return "unknown";
}

ModuleType module_type_from_string(const char *value)
{
    if (value == nullptr) {
        return ModuleType::Relay;
    }
    const std::string_view type(value);
    if (type == "relay") {
        return ModuleType::Relay;
    }
    if (type == "dimmer") {
        return ModuleType::Dimmer;
    }
    if (type == "input") {
        return ModuleType::Input;
    }
    return ModuleType::Relay;
}

}  // namespace ipbuilding::registry
