#pragma once

#include "span.h"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace ipbuilding::veldbus::payloads {

using ipbuilding::Span;

enum class RelayAction { On, Off, Pulse, Toggle };

struct RelayCommand {
    int channel = 0;
    RelayAction action = RelayAction::Toggle;
};

struct RelayStatus {
    int channel = 0;
    std::string state;  // "on", "off", "unknown"
    std::string state_code;
    std::optional<std::string> module;
};

std::vector<uint8_t> strip_j_envelope(Span<const uint8_t> data);
std::vector<uint8_t> encode_relay_command(const RelayCommand &cmd);
std::optional<RelayStatus> decode_relay_status(Span<const uint8_t> data);

}  // namespace ipbuilding::veldbus::payloads
