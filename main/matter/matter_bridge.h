#pragma once

#include "registry/device_registry.h"

#include "esp_err.h"
#include "esp_matter.h"

namespace ipbuilding::matter::bridge {

// Creates one Matter endpoint per active veldbus channel (design spec §8.1):
//   relay semantic_type=light  -> On/Off Light
//   relay semantic_type=fan    -> Fan (FanControl, Off/High)
//   relay semantic_type=switch -> On/Off Plug-in Unit
//   dimmer channel             -> Dimmable Light (On/Off + Level Control)
// Call after node::create() and before esp_matter::start().
esp_err_t create_endpoints(esp_matter::node_t *node, const registry::DeviceRegistry &registry);

// E2.4 command path: Matter attribute write -> veldbus command.
// Wire this into the node attribute callback (PRE_UPDATE).
esp_err_t handle_attribute_update(esp_matter::attribute::callback_type_t type, uint16_t endpoint_id,
                                  uint32_t cluster_id, uint32_t attribute_id, esp_matter_attr_val_t *val);

// E2.3 state sync: registers registry listeners that push veldbus state
// changes into Matter attributes. Call after esp_matter::start().
void start_state_sync(registry::DeviceRegistry &registry);

}  // namespace ipbuilding::matter::bridge
