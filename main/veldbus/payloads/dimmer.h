#pragma once

#include "span.h"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace ipbuilding::veldbus::payloads {

using ipbuilding::Span;

struct DimmerCommand {
    int channel = 0;
    int level = 0;
};

struct DimmerStatus {
    int channel = 0;
    int level_percent = 0;
    std::string internal_value_code;
    std::string device_type = "01";
    std::string family_constant = "54";
};

std::vector<uint8_t> encode_dim_command(const DimmerCommand &cmd);
std::vector<uint8_t> encode_dim_off(int channel);
std::vector<uint8_t> encode_dim_toggle(int channel);
std::vector<uint8_t> encode_dim_start(int channel);
std::vector<uint8_t> encode_dim_stop(int channel);
std::optional<DimmerStatus> decode_dimmer_status(Span<const uint8_t> data);
bool is_dimmer_idle_poll(Span<const uint8_t> data);

}  // namespace ipbuilding::veldbus::payloads
