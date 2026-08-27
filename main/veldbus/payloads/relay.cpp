#include "relay.h"

#include <cctype>
#include <string_view>

namespace ipbuilding::veldbus::payloads {

namespace {

bool is_ascii_text(Span<const uint8_t> data)
{
    for (std::size_t i = 0; i < data.size(); ++i) {
        if (data[i] > 0x7F) {
            return false;
        }
    }
    return true;
}

std::string_view as_ascii_view(Span<const uint8_t> data)
{
    return std::string_view(reinterpret_cast<const char *>(data.data()), data.size());
}

std::optional<int> parse_two_digit_channel(std::string_view text)
{
    if (text.size() != 2 || !std::isdigit(static_cast<unsigned char>(text[0])) ||
        !std::isdigit(static_cast<unsigned char>(text[1]))) {
        return std::nullopt;
    }
    return (text[0] - '0') * 10 + (text[1] - '0');
}

// Prefix rule (keep in sync with gateway/payloads/relay.py):
// 01xx = on, 00xx = off. Lab firmware uses 0100/0000; older Nolf IP0200
// modules also report 0115/0015 on status-poll. Command replies still use
// 0100/0000. Unknown prefixes stay "unknown".
std::string relay_state_from_code(std::string_view state_code)
{
    if (state_code.size() == 4) {
        bool digits = true;
        for (char ch : state_code) {
            if (!std::isdigit(static_cast<unsigned char>(ch))) {
                digits = false;
                break;
            }
        }
        if (digits) {
            if (state_code.substr(0, 2) == "01") {
                return "on";
            }
            if (state_code.substr(0, 2) == "00") {
                return "off";
            }
        }
    }
    return "unknown";
}

std::optional<int> parse_four_digit_channel(std::string_view text)
{
    if (text.size() != 4) {
        return std::nullopt;
    }
    int value = 0;
    for (char ch : text) {
        if (!std::isdigit(static_cast<unsigned char>(ch))) {
            return std::nullopt;
        }
        value = value * 10 + (ch - '0');
    }
    return value;
}

}  // namespace

std::vector<uint8_t> strip_j_envelope(Span<const uint8_t> data)
{
    if (!is_ascii_text(data)) {
        return std::vector<uint8_t>(data.data(), data.data() + data.size());
    }

    auto text = as_ascii_view(data);
    if (text.size() >= 3 && text[1] == 'J') {
        text.remove_prefix(2);
        return std::vector<uint8_t>(text.begin(), text.end());
    }

    if (text.size() >= 2) {
        const char prefix = text[0];
        if (prefix != 'S' && prefix != 'C' && prefix != 'P' && prefix != 'T') {
            const char cmd = text[1];
            if ((cmd == 'S' || cmd == 'C' || cmd == 'P' || cmd == 'T') && text.size() >= 5) {
                text.remove_prefix(1);
                return std::vector<uint8_t>(text.begin(), text.end());
            }
        }
    }

    return std::vector<uint8_t>(data.data(), data.data() + data.size());
}

std::vector<uint8_t> encode_relay_command(const RelayCommand &cmd)
{
    char letter = 'T';
    switch (cmd.action) {
    case RelayAction::On:
        letter = 'S';
        break;
    case RelayAction::Off:
        letter = 'C';
        break;
    case RelayAction::Pulse:
        letter = 'P';
        break;
    case RelayAction::Toggle:
        letter = 'T';
        break;
    }

    if (cmd.action == RelayAction::Pulse && cmd.channel == 0) {
        const std::string_view core = "P0000";
        return std::vector<uint8_t>(core.begin(), core.end());
    }

    char buffer[8] = {};
    const int ch = cmd.channel;
    buffer[0] = letter;
    buffer[1] = static_cast<char>('0' + (ch / 10));
    buffer[2] = static_cast<char>('0' + (ch % 10));
    buffer[3] = '0';
    buffer[4] = '0';
    return std::vector<uint8_t>(buffer, buffer + 5);
}

std::vector<uint8_t> encode_relay_status_query(int channel)
{
    // Cold-boot status read: I<CH>00 -> reply I000<CH><state> (native IPBox
    // behaviour, RE evidence 2026-06-12; corrects the 2026-06-02 negative
    // result which used the wrong channel field position).
    char buffer[8] = {};
    buffer[0] = 'I';
    buffer[1] = static_cast<char>('0' + (channel / 10));
    buffer[2] = static_cast<char>('0' + (channel % 10));
    buffer[3] = '0';
    buffer[4] = '0';
    return std::vector<uint8_t>(buffer, buffer + 5);
}

bool is_relay_poll_echo(Span<const uint8_t> data)
{
    if (!is_ascii_text(data) || data.size() != 10 || data[0] != 'P') {
        return false;
    }
    for (std::size_t i = 1; i < 10; ++i) {
        if (data[i] < '0' || data[i] > '9') {
            return false;
        }
    }
    return true;
}

std::optional<RelayStatus> decode_relay_status(Span<const uint8_t> data)
{
    const auto core_bytes = strip_j_envelope(data);
    if (!is_ascii_text(Span<const uint8_t>(core_bytes.data(), core_bytes.size()))) {
        return std::nullopt;
    }

    auto text = as_ascii_view(Span<const uint8_t>(core_bytes.data(), core_bytes.size()));
    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.back()))) {
        text.remove_suffix(1);
    }

    if (text.size() == 9 && text[0] == 'I') {
        const auto channel = parse_four_digit_channel(text.substr(1, 4));
        if (!channel) {
            return std::nullopt;
        }
        RelayStatus status;
        status.channel = *channel;
        status.state_code = std::string(text.substr(5, 4));
        status.state = relay_state_from_code(status.state_code);
        return status;
    }

    // Module status: I<module:3><channel:2><state:4> — e.g. I000160100 = ch16 on
    // (Python _RELAY_STATUS_RE; field test 2026-07-12).
    if (text.size() == 10 && text[0] == 'I') {
        const auto channel = parse_two_digit_channel(text.substr(4, 2));
        if (!channel) {
            return std::nullopt;
        }
        RelayStatus status;
        status.module = std::string(text.substr(1, 3));
        status.channel = *channel;
        status.state_code = std::string(text.substr(6, 4));
        status.state = relay_state_from_code(status.state_code);
        return status;
    }

    return std::nullopt;
}

}  // namespace ipbuilding::veldbus::payloads
