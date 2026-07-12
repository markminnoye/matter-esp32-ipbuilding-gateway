#include "udp_bus_manager.h"

#include "network_manager.h"
#include "poll_payloads.h"
#include "registry/types.h"
#include "span.h"
#include "veldbus/payloads/dimmer.h"
#include "veldbus/payloads/relay.h"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "lwip/inet.h"
#include "lwip/sockets.h"
#include "sdkconfig.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace ipbuilding::veldbus {

using ipbuilding::Span;

namespace {
constexpr const char *TAG = "udp_bus";
constexpr int kHubPort = 1001;
constexpr int kMaxDatagram = 256;
constexpr int kPollIntervalMs = 2000;
// Deep enough for a Matter level-transition burst (one command per step).
constexpr int kCommandQueueDepth = 16;

struct OutboundCommand {
    char ip[16];
    uint8_t payload[kMaxDatagram];
    size_t length = 0;
};

registry::DeviceRegistry *s_registry = nullptr;
int s_socket = -1;
TaskHandle_t s_poll_task = nullptr;
TaskHandle_t s_rx_task = nullptr;
QueueHandle_t s_command_queue = nullptr;
bool s_running = false;

bool parse_ipv4(const char *text, in_addr_t *out)
{
    in_addr addr = {};
    if (inet_aton(text, &addr) == 0) {
        return false;
    }
    *out = addr.s_addr;
    return true;
}

esp_err_t send_datagram(const char *module_ip, Span<const uint8_t> payload)
{
    if (s_socket < 0 || payload.empty()) {
        return ESP_ERR_INVALID_STATE;
    }

    sockaddr_in dest = {};
    dest.sin_family = AF_INET;
    dest.sin_port = htons(kHubPort);
    in_addr_t addr = 0;
    if (!parse_ipv4(module_ip, &addr)) {
        ESP_LOGW(TAG, "Invalid module IP: %s", module_ip);
        return ESP_ERR_INVALID_ARG;
    }
    dest.sin_addr.s_addr = addr;

    const int sent = sendto(s_socket, payload.data(), payload.size(), 0, reinterpret_cast<sockaddr *>(&dest),
                            sizeof(dest));
    if (sent < 0) {
        ESP_LOGW(TAG, "sendto %s failed: errno=%d", module_ip, errno);
        return ESP_FAIL;
    }

    ESP_LOGD(TAG, "UDP send %s (%d bytes)", module_ip, static_cast<int>(payload.size()));
    return ESP_OK;
}

std::string format_payload_preview(Span<const uint8_t> data, std::size_t max_len = 48)
{
    if (data.empty()) {
        return "(empty)";
    }

    const std::size_t len = data.size() > max_len ? max_len : data.size();
    bool printable = true;
    for (std::size_t i = 0; i < len; ++i) {
        const uint8_t byte = data[i];
        if (byte < 0x20 || byte > 0x7E) {
            printable = false;
            break;
        }
    }

    if (printable) {
        std::string text(reinterpret_cast<const char *>(data.data()), len);
        if (data.size() > max_len) {
            text += "…";
        }
        return text;
    }

    std::string hex;
    hex.reserve(len * 2 + 4);
    for (std::size_t i = 0; i < len; ++i) {
        char buf[4] = {};
        snprintf(buf, sizeof(buf), "%02x", data[i]);
        hex += buf;
    }
    if (data.size() > max_len) {
        hex += "…";
    }
    return hex;
}

std::string format_poll_payload(Span<const uint8_t> payload)
{
    return format_payload_preview(payload, 16);
}

void drain_command_queue()
{
    if (s_command_queue == nullptr) {
        return;
    }

    OutboundCommand queued = {};
    while (xQueueReceive(s_command_queue, &queued, 0) == pdTRUE) {
        const auto payload = Span<const uint8_t>(queued.payload, queued.length);
        const esp_err_t err = send_datagram(queued.ip, payload);
        if (err == ESP_OK) {
            ESP_LOGI(TAG, "TX command -> %s payload=%s", queued.ip, format_poll_payload(payload).c_str());
        }
    }
}

void send_poll_cycle()
{
    if (s_registry == nullptr) {
        return;
    }
    for (const auto &module : s_registry->active_modules()) {
        const auto payload = poll_payload_for_type(registry::module_type_to_string(module.type));
        if (payload.empty()) {
            continue;
        }
        const esp_err_t err = send_datagram(module.ip.c_str(), payload);
        if (err == ESP_OK) {
            ESP_LOGI(TAG, "TX poll -> %s (%s) payload=%s", module.ip.c_str(),
                     registry::module_type_to_string(module.type), format_poll_payload(payload).c_str());
        }
    }
}

void poll_task(void *arg)
{
    (void)arg;
    // Commands drain as soon as send_raw_command() notifies this task; polls
    // keep their fixed cadence. Without the notify a queued command sat until
    // the next poll tick (up to ~2 s tap-to-light latency, field test 2026-07-12).
    TickType_t next_poll = xTaskGetTickCount();
    while (s_running) {
        drain_command_queue();

        if (static_cast<int32_t>(xTaskGetTickCount() - next_poll) >= 0) {
            send_poll_cycle();
            next_poll = xTaskGetTickCount() + pdMS_TO_TICKS(kPollIntervalMs);
        }

        const TickType_t now = xTaskGetTickCount();
        const TickType_t wait = static_cast<int32_t>(next_poll - now) > 0 ? next_poll - now : 1;
        ulTaskNotifyTake(pdTRUE, wait);
    }
    vTaskDelete(nullptr);
}

void rx_task(void *arg)
{
    (void)arg;
    std::vector<uint8_t> buffer(kMaxDatagram);

    while (s_running) {
        sockaddr_in source = {};
        socklen_t source_len = sizeof(source);
        const int received = recvfrom(s_socket, buffer.data(), buffer.size(), 0,
                                      reinterpret_cast<sockaddr *>(&source), &source_len);
        if (received < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                vTaskDelay(pdMS_TO_TICKS(20));
                continue;
            }
            ESP_LOGW(TAG, "recvfrom failed: errno=%d", errno);
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }

        char ip_str[INET_ADDRSTRLEN] = {};
        inet_ntoa_r(source.sin_addr, ip_str, sizeof(ip_str));
        const int src_port = ntohs(source.sin_port);
        const auto packet = Span<const uint8_t>(buffer.data(), static_cast<std::size_t>(received));

        ESP_LOGI(TAG, "RX reply <- %s:%d (%d bytes) data=%s", ip_str, src_port, received,
                 format_payload_preview(packet).c_str());

        if (s_registry != nullptr) {
            s_registry->handle_packet(ip_str, packet);
        }
    }
    vTaskDelete(nullptr);
}

}  // namespace

esp_err_t start(registry::DeviceRegistry &registry)
{
    if (s_running) {
        return ESP_OK;
    }

    s_registry = &registry;

    in_addr_t bind_addr = 0;
    if (!parse_ipv4(CONFIG_ETH_STATIC_IP, &bind_addr)) {
        ESP_LOGE(TAG, "Invalid CONFIG_ETH_STATIC_IP");
        return ESP_ERR_INVALID_ARG;
    }

    s_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (s_socket < 0) {
        ESP_LOGE(TAG, "socket() failed: errno=%d", errno);
        return ESP_FAIL;
    }

    int reuse = 1;
    setsockopt(s_socket, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    timeval timeout = {};
    timeout.tv_sec = 0;
    timeout.tv_usec = 200000;
    setsockopt(s_socket, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

    sockaddr_in local = {};
    local.sin_family = AF_INET;
    local.sin_port = htons(kHubPort);
    local.sin_addr.s_addr = bind_addr;

    if (bind(s_socket, reinterpret_cast<sockaddr *>(&local), sizeof(local)) != 0) {
        ESP_LOGE(TAG, "bind(%s:%d) failed: errno=%d", CONFIG_ETH_STATIC_IP, kHubPort, errno);
        close(s_socket);
        s_socket = -1;
        return ESP_FAIL;
    }

    s_command_queue = xQueueCreate(kCommandQueueDepth, sizeof(OutboundCommand));
    if (s_command_queue == nullptr) {
        close(s_socket);
        s_socket = -1;
        return ESP_ERR_NO_MEM;
    }

    s_running = true;
    if (xTaskCreate(poll_task, "udp_bus_poll", 4096, nullptr, configMAX_PRIORITIES - 3, &s_poll_task) != pdPASS) {
        s_running = false;
        vQueueDelete(s_command_queue);
        s_command_queue = nullptr;
        close(s_socket);
        s_socket = -1;
        return ESP_ERR_NO_MEM;
    }

    if (xTaskCreate(rx_task, "udp_bus_rx", 4096, nullptr, configMAX_PRIORITIES - 3, &s_rx_task) != pdPASS) {
        s_running = false;
        vTaskDelete(s_poll_task);
        s_poll_task = nullptr;
        vQueueDelete(s_command_queue);
        s_command_queue = nullptr;
        close(s_socket);
        s_socket = -1;
        return ESP_ERR_NO_MEM;
    }

    ESP_LOGI(TAG, "UdpBusManager started on %s:%d", CONFIG_ETH_STATIC_IP, kHubPort);
    return ESP_OK;
}

void stop()
{
    if (!s_running) {
        return;
    }

    s_running = false;
    if (s_poll_task != nullptr) {
        vTaskDelay(pdMS_TO_TICKS(kPollIntervalMs + 100));
        s_poll_task = nullptr;
    }
    if (s_rx_task != nullptr) {
        vTaskDelay(pdMS_TO_TICKS(100));
        s_rx_task = nullptr;
    }
    if (s_command_queue != nullptr) {
        vQueueDelete(s_command_queue);
        s_command_queue = nullptr;
    }
    if (s_socket >= 0) {
        close(s_socket);
        s_socket = -1;
    }
    s_registry = nullptr;
}

esp_err_t send_raw_command(const char *module_ip, Span<const uint8_t> payload)
{
    if (!s_running || s_command_queue == nullptr) {
        return ESP_ERR_INVALID_STATE;
    }
    if (module_ip == nullptr || module_ip[0] == '\0' || payload.empty()) {
        return ESP_ERR_INVALID_ARG;
    }
    if (payload.size() > kMaxDatagram) {
        return ESP_ERR_INVALID_SIZE;
    }

    OutboundCommand queued = {};
    strncpy(queued.ip, module_ip, sizeof(queued.ip) - 1);
    queued.length = payload.size();
    memcpy(queued.payload, payload.data(), payload.size());

    if (xQueueSend(s_command_queue, &queued, 0) != pdTRUE) {
        ESP_LOGW(TAG, "Command queue full for %s", module_ip);
        return ESP_ERR_NO_MEM;
    }

    if (s_poll_task != nullptr) {
        xTaskNotifyGive(s_poll_task);  // Drain immediately instead of at the next poll tick.
    }

    return ESP_OK;
}

esp_err_t send_relay_command(const char *module_ip, int channel, payloads::RelayAction action)
{
    const auto wire = payloads::encode_relay_command(payloads::RelayCommand{channel, action});
    return send_raw_command(module_ip, Span<const uint8_t>(wire.data(), wire.size()));
}

esp_err_t send_dimmer_command(const char *module_ip, int channel, int level_percent)
{
    const auto wire = payloads::encode_dim_command(payloads::DimmerCommand{channel, level_percent});
    return send_raw_command(module_ip, Span<const uint8_t>(wire.data(), wire.size()));
}

esp_err_t send_dimmer_off(const char *module_ip, int channel)
{
    const auto wire = payloads::encode_dim_off(channel);
    return send_raw_command(module_ip, Span<const uint8_t>(wire.data(), wire.size()));
}

esp_err_t send_dimmer_toggle(const char *module_ip, int channel)
{
    const auto wire = payloads::encode_dim_toggle(channel);
    return send_raw_command(module_ip, Span<const uint8_t>(wire.data(), wire.size()));
}

}  // namespace ipbuilding::veldbus
