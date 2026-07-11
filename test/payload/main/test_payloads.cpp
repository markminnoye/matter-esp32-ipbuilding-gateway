#include "unity.h"

#include "span.h"
#include "veldbus/payloads/dimmer.h"
#include "veldbus/payloads/input.h"
#include "veldbus/payloads/relay.h"

#include <vector>

using ipbuilding::Span;
using ipbuilding::veldbus::payloads::DimmerCommand;
using ipbuilding::veldbus::payloads::InputEventType;
using ipbuilding::veldbus::payloads::RelayAction;
using ipbuilding::veldbus::payloads::RelayCommand;

static Span<const uint8_t> as_span(const std::vector<uint8_t> &data)
{
    return Span<const uint8_t>(data.data(), data.size());
}

static bool bytes_equal(const std::vector<uint8_t> &actual, std::initializer_list<uint8_t> expected)
{
    if (actual.size() != expected.size()) {
        return false;
    }
    size_t i = 0;
    for (uint8_t byte : expected) {
        if (actual[i++] != byte) {
            return false;
        }
    }
    return true;
}

TEST_CASE("relay status on", "[payloads]")
{
    const std::vector<uint8_t> payload = {'I', '0', '0', '1', '0', '0', '1', '0', '0'};
    const auto status = ipbuilding::veldbus::payloads::decode_relay_status(as_span(payload));
    TEST_ASSERT_TRUE(status.has_value());
    TEST_ASSERT_EQUAL(10, status->channel);
    TEST_ASSERT_EQUAL_STRING("on", status->state.c_str());
}

TEST_CASE("relay status off", "[payloads]")
{
    const std::vector<uint8_t> payload = {'I', '0', '0', '0', '0', '0', '0', '0', '0'};
    const auto status = ipbuilding::veldbus::payloads::decode_relay_status(as_span(payload));
    TEST_ASSERT_TRUE(status.has_value());
    TEST_ASSERT_EQUAL(0, status->channel);
    TEST_ASSERT_EQUAL_STRING("off", status->state.c_str());
}

TEST_CASE("relay strip j envelope", "[payloads]")
{
    const std::vector<uint8_t> wrapped = {'m', 'J', 'S', '0', '0', '0', '0'};
    const auto stripped = ipbuilding::veldbus::payloads::strip_j_envelope(as_span(wrapped));
    TEST_ASSERT_TRUE(bytes_equal(stripped, {'S', '0', '0', '0', '0'}));
}

TEST_CASE("relay encode on", "[payloads]")
{
    RelayCommand cmd{0, RelayAction::On};
    const auto wire = ipbuilding::veldbus::payloads::encode_relay_command(cmd);
    TEST_ASSERT_TRUE(bytes_equal(wire, {'S', '0', '0', '0', '0'}));
}

TEST_CASE("dimmer status 30 percent", "[payloads]")
{
    const std::vector<uint8_t> payload = {'I', '0', '1', '5', '4', '0', '3', '0'};
    const auto status = ipbuilding::veldbus::payloads::decode_dimmer_status(as_span(payload));
    TEST_ASSERT_TRUE(status.has_value());
    TEST_ASSERT_EQUAL(0, status->channel);
    TEST_ASSERT_EQUAL(30, status->level_percent);
}

TEST_CASE("dimmer channel in code regression", "[payloads]")
{
    const std::vector<uint8_t> payload = {'I', '0', '1', '5', '4', '1', '3', '0'};
    const auto status = ipbuilding::veldbus::payloads::decode_dimmer_status(as_span(payload));
    TEST_ASSERT_TRUE(status.has_value());
    TEST_ASSERT_EQUAL(1, status->channel);
    TEST_ASSERT_EQUAL(30, status->level_percent);
}

TEST_CASE("dimmer idle heartbeat ignored", "[payloads]")
{
    const std::vector<uint8_t> payload = {'I', '0', '1', '5', '4', '9', '9', '9'};
    TEST_ASSERT_TRUE(ipbuilding::veldbus::payloads::is_dimmer_idle_poll(as_span(payload)));
    TEST_ASSERT_FALSE(ipbuilding::veldbus::payloads::decode_dimmer_status(as_span(payload)).has_value());
}

TEST_CASE("dimmer encode command", "[payloads]")
{
    const auto wire = ipbuilding::veldbus::payloads::encode_dim_command(DimmerCommand{0, 30});
    TEST_ASSERT_TRUE(bytes_equal(wire, {'S', '0', '3', '0', '1', '0', '3', '0'}));
}

TEST_CASE("input button press", "[payloads]")
{
    const std::vector<uint8_t> raw = {0x42, 0x2d, 0x2f, 0x81, 0x85, 0x19, 0x00, 0x00, 0xdf, 0x03, 0x01, 0x00, 0x45};
    const auto event = ipbuilding::veldbus::payloads::decode_input_event(as_span(raw));
    TEST_ASSERT_TRUE(event.has_value());
    TEST_ASSERT_EQUAL(static_cast<int>(InputEventType::Press), static_cast<int>(event->type));
    TEST_ASSERT_EQUAL_STRING("2f8185190000df", event->id_hex.c_str());
}

extern "C" void app_main(void)
{
    unity_run_menu();
}
