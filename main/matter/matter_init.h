#pragma once

#include "registry/device_registry.h"

#include "esp_err.h"

namespace ipbuilding::matter {

// Starts the Matter stack with one endpoint per active veldbus channel (E2).
// The registry must be loaded before calling; it must outlive the Matter stack.
esp_err_t init(registry::DeviceRegistry &registry);

}  // namespace ipbuilding::matter
