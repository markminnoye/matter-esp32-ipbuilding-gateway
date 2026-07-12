#pragma once

#include "config/network_config.h"

#include "esp_err.h"
#include "esp_netif.h"

namespace ipbuilding::network {

esp_err_t init(const config::EthNetworkConfig &eth_config);

esp_netif_t *get_eth_netif();
esp_netif_t *get_wifi_netif();

// Effective ETH IPv4 info, valid once init() has resolved an address
// (DHCP lease, fallback, or static). Returns ESP_ERR_INVALID_STATE if ETH
// has no usable IP (DHCP timed out with no fallback configured).
esp_err_t get_eth_ip_info(esp_netif_ip_info_t *out);

// "dhcp" | "fallback" | "static" | "none" — for boot-log / diagnostics.
const char *get_eth_ip_source();

}  // namespace ipbuilding::network
