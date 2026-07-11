#pragma once

#include "span.h"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace ipbuilding::veldbus::payloads {

using ipbuilding::Span;

enum class InputEventType { Poll, IdleStatus, Press, Release };

struct InputEvent {
    InputEventType type = InputEventType::Poll;
    std::string id_hex;
    std::string status_bytes_hex;
};

std::vector<uint8_t> encode_input_poll();
std::optional<InputEvent> decode_input_event(Span<const uint8_t> data);

}  // namespace ipbuilding::veldbus::payloads
