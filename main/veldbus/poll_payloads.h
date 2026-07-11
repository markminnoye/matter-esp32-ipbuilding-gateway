#pragma once

#include "span.h"

#include <cstdint>
#include <string_view>

namespace ipbuilding::veldbus {

using ipbuilding::Span;

inline constexpr uint8_t kPollRelay[] = {'P', '0', '0', '0', '0'};
inline constexpr uint8_t kPollDimmer[] = {'I', '9', '9', '0', '0'};
inline constexpr uint8_t kPollInput[] = {'I', '0', '0', '0', '0'};

inline Span<const uint8_t> poll_payload_for_type(std::string_view module_type)
{
    if (module_type == "relay") {
        return kPollRelay;
    }
    if (module_type == "dimmer") {
        return kPollDimmer;
    }
    if (module_type == "input") {
        return kPollInput;
    }
    return {};
}

}  // namespace ipbuilding::veldbus
