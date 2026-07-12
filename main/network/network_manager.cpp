#include "network_manager.h"

#include "board_profile.h"

#include <cstring>

#include "esp_check.h"
#include "esp_eth.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "lwip/inet.h"
#include "sdkconfig.h"

namespace ipbuilding::network {

static const char *TAG = "network_mgr";

static constexpr int kEthLinkBit = BIT0;
static constexpr int kWifiConnectedBit = BIT1;
static constexpr int kEthGotIpBit = BIT2;

// Issue #3: DHCP is attempted for this long before falling back to the
// configured static address (field-bus VLANs often have no DHCP server).
static constexpr int kEthDhcpTimeoutMs = 30000;

static EventGroupHandle_t s_event_group;
static esp_netif_t *s_eth_netif;
static esp_netif_t *s_wifi_netif;
static esp_eth_handle_t s_eth_handle;
static const char *s_eth_ip_source = "none";

static bool parse_ipv4(const char *text, esp_ip4_addr_t *out)
{
    struct in_addr addr = {};
    if (inet_aton(text, &addr) == 0) {
        return false;
    }
    out->addr = addr.s_addr;
    return true;
}

static esp_err_t apply_static_ipv4(esp_netif_t *netif, const char *ip, const char *gw, const char *netmask)
{
    esp_netif_ip_info_t ip_info = {};
    ESP_RETURN_ON_FALSE(parse_ipv4(ip, &ip_info.ip), ESP_ERR_INVALID_ARG, TAG, "invalid IP %s", ip);
    ESP_RETURN_ON_FALSE(parse_ipv4(gw, &ip_info.gw), ESP_ERR_INVALID_ARG, TAG, "invalid gateway %s", gw);
    ESP_RETURN_ON_FALSE(parse_ipv4(netmask, &ip_info.netmask), ESP_ERR_INVALID_ARG, TAG, "invalid netmask %s",
                        netmask);

    ESP_RETURN_ON_ERROR(esp_netif_dhcpc_stop(netif), TAG, "esp_netif_dhcpc_stop failed");
    ESP_RETURN_ON_ERROR(esp_netif_set_ip_info(netif, &ip_info), TAG, "esp_netif_set_ip_info failed");
    return ESP_OK;
}

static void log_ip_info(const char *label, esp_netif_t *netif)
{
    esp_netif_ip_info_t ip_info = {};
    if (esp_netif_get_ip_info(netif, &ip_info) != ESP_OK) {
        ESP_LOGW(TAG, "%s: no IP info", label);
        return;
    }
    ESP_LOGI(TAG, "%s: ip=" IPSTR " gw=" IPSTR " mask=" IPSTR, label, IP2STR(&ip_info.ip), IP2STR(&ip_info.gw),
             IP2STR(&ip_info.netmask));
}

static void eth_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    (void)arg;
    (void)event_base;

    esp_eth_handle_t eth_handle = *static_cast<esp_eth_handle_t *>(event_data);

    switch (event_id) {
    case ETHERNET_EVENT_CONNECTED: {
        uint8_t mac[6] = {};
        esp_eth_ioctl(eth_handle, ETH_CMD_G_MAC_ADDR, mac);
        ESP_LOGI(TAG, "Ethernet link up, MAC %02x:%02x:%02x:%02x:%02x:%02x (field-bus VLAN ready)", mac[0], mac[1],
                 mac[2], mac[3], mac[4], mac[5]);
        xEventGroupSetBits(s_event_group, kEthLinkBit);
        break;
    }
    case ETHERNET_EVENT_DISCONNECTED:
        ESP_LOGW(TAG, "Ethernet link down");
        xEventGroupClearBits(s_event_group, kEthLinkBit);
        break;
    case ETHERNET_EVENT_START:
        ESP_LOGI(TAG, "Ethernet started");
        break;
    default:
        break;
    }
}

static void eth_got_ip_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    (void)arg;
    (void)event_base;
    (void)event_id;
    (void)event_data;

    log_ip_info("ETH", s_eth_netif);
    xEventGroupSetBits(s_event_group, kEthGotIpBit);
}

static void wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    (void)arg;
    (void)event_data;

    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        ESP_LOGI(TAG, "WiFi STA started, connecting to %s", CONFIG_WIFI_SSID);
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGW(TAG, "WiFi disconnected, reconnecting");
        xEventGroupClearBits(s_event_group, kWifiConnectedBit);
        esp_wifi_connect();
    }
}

static void wifi_got_ip_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    (void)arg;
    (void)event_base;
    (void)event_id;
    (void)event_data;

    wifi_ap_record_t ap_info = {};
    if (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK) {
        ESP_LOGI(TAG, "WiFi connected, RSSI=%d dBm (H1 field-cabinet test)", ap_info.rssi);
    }

    log_ip_info("WiFi", s_wifi_netif);
    xEventGroupSetBits(s_event_group, kWifiConnectedBit);
}

static esp_err_t init_ethernet(const config::EthNetworkConfig &cfg)
{
    ESP_RETURN_ON_ERROR(board::init_w5500_ethernet(&s_eth_handle), TAG, "W5500 init failed");

    esp_netif_config_t netif_cfg = ESP_NETIF_DEFAULT_ETH();
    s_eth_netif = esp_netif_new(&netif_cfg);
    ESP_RETURN_ON_FALSE(s_eth_netif != nullptr, ESP_FAIL, TAG, "esp_netif_new ETH failed");

    esp_eth_netif_glue_handle_t glue = esp_eth_new_netif_glue(s_eth_handle);
    ESP_RETURN_ON_ERROR(esp_netif_attach(s_eth_netif, glue), TAG, "esp_netif_attach ETH failed");

    ESP_RETURN_ON_ERROR(esp_event_handler_register(ETH_EVENT, ESP_EVENT_ANY_ID, &eth_event_handler, nullptr), TAG,
                        "ETH event register failed");
    ESP_RETURN_ON_ERROR(esp_event_handler_register(IP_EVENT, IP_EVENT_ETH_GOT_IP, &eth_got_ip_handler, nullptr), TAG,
                        "ETH IP event register failed");

    if (cfg.mode == config::EthNetworkConfig::Mode::Static) {
        ESP_LOGI(TAG, "ETH mode=static (operator override): ip=%s gw=%s mask=%s", cfg.ip.c_str(),
                 cfg.gateway.c_str(), cfg.netmask.c_str());
        ESP_RETURN_ON_ERROR(apply_static_ipv4(s_eth_netif, cfg.ip.c_str(), cfg.gateway.c_str(), cfg.netmask.c_str()),
                            TAG, "ETH static IP failed");
        s_eth_ip_source = "static";
    } else {
        ESP_LOGI(TAG, "ETH mode=dhcp: requesting lease (timeout %d ms, fallback=%s)", kEthDhcpTimeoutMs,
                 cfg.dhcp_fallback_static ? cfg.ip.c_str() : "disabled");
    }

    ESP_RETURN_ON_ERROR(esp_eth_start(s_eth_handle), TAG, "esp_eth_start failed");

    if (cfg.mode == config::EthNetworkConfig::Mode::Dhcp) {
        const EventBits_t bits = xEventGroupWaitBits(s_event_group, kEthGotIpBit, pdFALSE, pdTRUE,
                                                      pdMS_TO_TICKS(kEthDhcpTimeoutMs));
        if ((bits & kEthGotIpBit) != 0) {
            s_eth_ip_source = "dhcp";
        } else if (cfg.dhcp_fallback_static) {
            ESP_LOGW(TAG, "ETH: DHCP timeout, using fallback static %s", cfg.ip.c_str());
            const esp_err_t fallback_err =
                apply_static_ipv4(s_eth_netif, cfg.ip.c_str(), cfg.gateway.c_str(), cfg.netmask.c_str());
            if (fallback_err == ESP_OK) {
                s_eth_ip_source = "fallback";
                xEventGroupSetBits(s_event_group, kEthGotIpBit);
            } else {
                ESP_LOGE(TAG, "ETH fallback static IP failed: %s", esp_err_to_name(fallback_err));
                s_eth_ip_source = "none";
            }
        } else {
            ESP_LOGW(TAG, "ETH: DHCP timeout, no fallback configured (dhcp_fallback_static=false) — veldbus may "
                          "fail to bind");
            s_eth_ip_source = "none";
        }
    }

    log_ip_info("ETH", s_eth_netif);
    ESP_LOGI(TAG, "ETH IP source: %s", s_eth_ip_source);
    return ESP_OK;
}

static esp_err_t init_wifi()
{
    s_wifi_netif = esp_netif_create_default_wifi_sta();
    ESP_RETURN_ON_FALSE(s_wifi_netif != nullptr, ESP_FAIL, TAG, "create WiFi STA netif failed");

    wifi_init_config_t wifi_cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_RETURN_ON_ERROR(esp_wifi_init(&wifi_cfg), TAG, "esp_wifi_init failed");
    ESP_RETURN_ON_ERROR(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, nullptr), TAG,
                        "WiFi event register failed");
    ESP_RETURN_ON_ERROR(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_got_ip_handler, nullptr), TAG,
                        "WiFi IP event register failed");

    wifi_config_t sta_config = {};
    std::strncpy(reinterpret_cast<char *>(sta_config.sta.ssid), CONFIG_WIFI_SSID, sizeof(sta_config.sta.ssid) - 1);
    std::strncpy(reinterpret_cast<char *>(sta_config.sta.password), CONFIG_WIFI_PASSWORD,
                 sizeof(sta_config.sta.password) - 1);
    sta_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;

    ESP_RETURN_ON_ERROR(esp_wifi_set_mode(WIFI_MODE_STA), TAG, "esp_wifi_set_mode failed");
    ESP_RETURN_ON_ERROR(esp_wifi_set_config(WIFI_IF_STA, &sta_config), TAG, "esp_wifi_set_config failed");
    ESP_RETURN_ON_ERROR(esp_wifi_start(), TAG, "esp_wifi_start failed");

    return ESP_OK;
}

esp_err_t init(const config::EthNetworkConfig &eth_config)
{
    s_event_group = xEventGroupCreate();
    ESP_RETURN_ON_FALSE(s_event_group != nullptr, ESP_ERR_NO_MEM, TAG, "EventGroup create failed");

    ESP_RETURN_ON_ERROR(esp_netif_init(), TAG, "esp_netif_init failed");
    ESP_RETURN_ON_ERROR(esp_event_loop_create_default(), TAG, "esp_event_loop_create_default failed");

    // init_ethernet() resolves the ETH address itself (DHCP wait + fallback
    // or static apply) before returning, so veldbus can bind on a real IP
    // as soon as network::init() completes (issue #3: "start veldbus pas
    // na IP_EVENT_ETH_GOT_IP").
    ESP_RETURN_ON_ERROR(init_ethernet(eth_config), TAG, "Ethernet init failed");
    ESP_RETURN_ON_ERROR(init_wifi(), TAG, "WiFi init failed");

    EventBits_t bits = xEventGroupWaitBits(s_event_group, kWifiConnectedBit, pdFALSE, pdTRUE, pdMS_TO_TICKS(30000));
    if ((bits & kWifiConnectedBit) == 0) {
        ESP_LOGW(TAG, "WiFi not connected within timeout — check CONFIG_WIFI_SSID/PASSWORD in menuconfig");
    }

    ESP_LOGI(TAG, "Network init complete (ETH source=%s, dual-netif: ETH field-bus + WiFi home LAN)",
             s_eth_ip_source);
    return ESP_OK;
}

esp_netif_t *get_eth_netif()
{
    return s_eth_netif;
}

esp_netif_t *get_wifi_netif()
{
    return s_wifi_netif;
}

esp_err_t get_eth_ip_info(esp_netif_ip_info_t *out)
{
    if (s_eth_netif == nullptr || out == nullptr) {
        return ESP_ERR_INVALID_STATE;
    }
    if (std::strcmp(s_eth_ip_source, "none") == 0) {
        return ESP_ERR_INVALID_STATE;
    }
    return esp_netif_get_ip_info(s_eth_netif, out);
}

const char *get_eth_ip_source()
{
    return s_eth_ip_source;
}

}  // namespace ipbuilding::network
