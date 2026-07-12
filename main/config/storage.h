#pragma once

#include "esp_err.h"

namespace ipbuilding::storage {

// Idempotent: safe to call from multiple config loaders (devices.json,
// network.json). Returns ESP_OK once the LittleFS partition is mounted,
// whether by this call or an earlier one.
esp_err_t mount_littlefs();

}  // namespace ipbuilding::storage
