#include "input.h"

#include <iomanip>
#include <sstream>

namespace ipbuilding::veldbus::payloads {

namespace {

std::string bytes_to_hex(Span<const uint8_t> data)
{
    std::ostringstream out;
    out << std::hex << std::setfill('0');
    for (std::size_t i = 0; i < data.size(); ++i) {
        out << std::setw(2) << static_cast<int>(data[i]);
    }
    return out.str();
}

bool is_input_poll(Span<const uint8_t> data)
{
    return data.size() == 5 && data[0] == 'I' && data[1] == '0' && data[2] == '0' && data[3] == '0' && data[4] == '0';
}

bool is_input_idle_reply(Span<const uint8_t> data)
{
    // Wire format (13 bytes, field-captured 2026-07-11): I\x02 R <3 status> <6×0x00> E
    if (data.size() == 13 && data[0] == 'I' && data[1] == 0x02 && data[2] == 'R' && data[12] == 'E') {
        for (size_t i = 6; i <= 11; ++i) {
            if (data[i] != 0x00) {
                return false;
            }
        }
        return true;
    }

    // Legacy 14-byte variant (unit-test / older captures).
    if (data.size() == 14 && data[0] == 'I' && data[1] == 0x02 && data[13] == 'E') {
        for (size_t i = 9; i <= 12; ++i) {
            if (data[i] != 0x00) {
                return false;
            }
        }
        return true;
    }

    return false;
}

bool is_input_button_event(Span<const uint8_t> data, bool *is_press)
{
    if (data.size() != 13 || data[0] != 'B' || data[1] != '-' || data[11] != 0x00 || data[12] != 'E') {
        return false;
    }
    if (data[10] == 0x01) {
        *is_press = true;
        return true;
    }
    if (data[10] == 0x00) {
        *is_press = false;
        return true;
    }
    return false;
}

}  // namespace

std::vector<uint8_t> encode_input_poll()
{
    return {'I', '0', '0', '0', '0'};
}

std::optional<InputEvent> decode_input_event(Span<const uint8_t> data)
{
    if (is_input_poll(data)) {
        return InputEvent{InputEventType::Poll, {}, {}};
    }

    if (is_input_idle_reply(data)) {
        InputEvent event;
        event.type = InputEventType::IdleStatus;
        event.status_bytes_hex = bytes_to_hex(data.subspan(3, 3));
        return event;
    }

    bool is_press = false;
    if (is_input_button_event(data, &is_press)) {
        InputEvent event;
        event.type = is_press ? InputEventType::Press : InputEventType::Release;
        event.id_hex = bytes_to_hex(data.subspan(2, 7));
        return event;
    }

    return std::nullopt;
}

}  // namespace ipbuilding::veldbus::payloads
