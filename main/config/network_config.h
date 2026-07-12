#pragma once

#include "esp_err.h"

#include <string>

namespace ipbuilding::config {

struct EthNetworkConfig {
    enum class Mode { Dhcp, Static };

    Mode mode = Mode::Dhcp;

    // Only meaningful in Dhcp mode: fall back to ip/netmask/gateway below if
    // no lease arrives within the DHCP timeout.
    bool dhcp_fallback_static = true;

    // Static mode: applied directly on boot.
    // Dhcp mode: the fallback address (IPBuilding hub convention).
    std::string ip;
    std::string netmask;
    std::string gateway;
};

// Loads `network.json` from LittleFS (mounts it if not already mounted).
// Factory default (file absent or unreadable): DHCP with fallback to the
// CONFIG_ETH_STATIC_IP/GW/NETMASK menuconfig values (IPBuilding hub
// convention, unchanged from pre-E3 behaviour).
class NetworkConfigStore {
public:
    esp_err_t init();
    const EthNetworkConfig &eth() const { return eth_; }

private:
    esp_err_t parse_json(const char *json_text);

    EthNetworkConfig eth_;
};

}  // namespace ipbuilding::config
