#pragma once

#include "registry/device_registry.h"

#include "esp_err.h"

namespace ipbuilding::veldbus {

esp_err_t start(registry::DeviceRegistry &registry);
void stop();

}  // namespace ipbuilding::veldbus
