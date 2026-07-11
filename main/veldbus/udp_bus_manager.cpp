#include "udp_bus_manager.h"

#include "network_manager.h"
#include "poll_payloads.h"
#include "registry/types.h"
#include "span.h"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "lwip/inet.h"
#include "lwip/sockets.h"
#include "sdkconfig.h"

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
constexpr int kCommandQueueDepth = 8;

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

void poll_task(void *arg)
{
    (void)arg;
    while (s_running) {
        if (s_registry != nullptr) {
            for (const auto &module : s_registry->active_modules()) {
                const auto payload = poll_payload_for_type(registry::module_type_to_string(module.type));
                if (payload.empty()) {
                    continue;
                }
                const esp_err_t err = send_datagram(module.ip.c_str(), payload);
                if (err == ESP_OK) {
                    ESP_LOGI(TAG, "Poll %s (%s)", module.ip.c_str(), registry::module_type_to_string(module.type));
                }
            }
        }

        OutboundCommand queued = {};
        while (xQueueReceive(s_command_queue, &queued, 0) == pdTRUE) {
            send_datagram(queued.ip, Span<const uint8_t>(queued.payload, queued.length));
        }

        vTaskDelay(pdMS_TO_TICKS(kPollIntervalMs));
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
        if (s_registry != nullptr) {
            s_registry->handle_packet(ip_str, Span<const uint8_t>(buffer.data(), static_cast<std::size_t>(received)));
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

}  // namespace ipbuilding::veldbus
