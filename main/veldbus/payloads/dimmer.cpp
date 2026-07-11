#include "dimmer.h"

#include <charconv>
#include <string_view>

namespace ipbuilding::veldbus::payloads {

namespace {

constexpr const char *kDimmerIdleCode = "999";

std::string percent_to_value_code(int level)
{
    if (level <= 0) {
        return "00";
    }
    if (level >= 100) {
        return "99";
    }
    char buffer[3] = {};
    buffer[0] = static_cast<char>('0' + (level / 10));
    buffer[1] = static_cast<char>('0' + (level % 10));
    return std::string(buffer, 2);
}

std::optional<int> value_code_to_percent(std::string_view code)
{
    int value = 0;
    const auto parsed = std::from_chars(code.data(), code.data() + code.size(), value);
    if (parsed.ec != std::errc()) {
        return std::nullopt;
    }
    if (value <= 0) {
        return 0;
    }
    if (value >= 99) {
        return 100;
    }
    return value;
}

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

}  // namespace

std::vector<uint8_t> encode_dim_command(const DimmerCommand &cmd)
{
    const std::string code = percent_to_value_code(cmd.level);
    const std::string wire = "S" + std::to_string(cmd.channel) + code + "1030";
    return std::vector<uint8_t>(wire.begin(), wire.end());
}

std::vector<uint8_t> encode_dim_off(int channel)
{
    const std::string wire = "C" + std::to_string(channel) + "991030";
    return std::vector<uint8_t>(wire.begin(), wire.end());
}

std::vector<uint8_t> encode_dim_toggle(int channel)
{
    const std::string wire = "T" + std::to_string(channel) + "991000";
    return std::vector<uint8_t>(wire.begin(), wire.end());
}

std::vector<uint8_t> encode_dim_start(int channel)
{
    const std::string wire = "D" + std::to_string(channel) + "001003";
    return std::vector<uint8_t>(wire.begin(), wire.end());
}

std::vector<uint8_t> encode_dim_stop(int channel)
{
    const std::string wire = "D" + std::to_string(channel) + "001000";
    return std::vector<uint8_t>(wire.begin(), wire.end());
}

bool is_dimmer_idle_poll(Span<const uint8_t> data)
{
    if (!is_ascii_text(data)) {
        return false;
    }
    const auto text = as_ascii_view(data);
    if (text == "I9900") {
        return true;
    }
    return text.size() == 8 && text.rfind("I0154", 0) == 0 && text.substr(4) == kDimmerIdleCode;
}

std::optional<DimmerStatus> decode_dimmer_status(Span<const uint8_t> data)
{
    if (!is_ascii_text(data)) {
        return std::nullopt;
    }

    const auto text = as_ascii_view(data);
    if (text.size() != 8 || text.rfind("I01", 0) != 0 || text.substr(3, 2) != "54") {
        return std::nullopt;
    }

    const std::string code = std::string(text.substr(4));
    if (code == kDimmerIdleCode || code.size() != 3) {
        return std::nullopt;
    }

    DimmerStatus status;
    status.internal_value_code = code;
    status.channel = code[0] - '0';
    const auto level = value_code_to_percent(code.substr(1));
    if (!level) {
        return std::nullopt;
    }
    status.level_percent = *level;
    return status;
}

}  // namespace ipbuilding::veldbus::payloads
