#pragma once

#include "esp_err.h"
#include "esp_netif.h"

namespace ipbuilding::network {

esp_err_t init();

esp_netif_t *get_eth_netif();
esp_netif_t *get_wifi_netif();

}  // namespace ipbuilding::network
