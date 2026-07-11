#pragma once

#include "registry/device_registry.h"
#include "span.h"
#include "veldbus/payloads/relay.h"

#include "esp_err.h"

namespace ipbuilding::veldbus {

using ipbuilding::Span;

esp_err_t start(registry::DeviceRegistry &registry);
void stop();

esp_err_t send_raw_command(const char *module_ip, Span<const uint8_t> payload);
esp_err_t send_relay_command(const char *module_ip, int channel, payloads::RelayAction action);
esp_err_t send_dimmer_command(const char *module_ip, int channel, int level_percent);
esp_err_t send_dimmer_off(const char *module_ip, int channel);

}  // namespace ipbuilding::veldbus
