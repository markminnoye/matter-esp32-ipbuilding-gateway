#include "network_config.h"

#include "storage.h"

#include "esp_log.h"
#include "sdkconfig.h"

#include "cJSON.h"

#include <cstdio>
#include <cstring>

namespace ipbuilding::config {

namespace {
constexpr const char *TAG = "network_config";
constexpr const char *kNetworkJsonPath = "/littlefs/network.json";

std::string read_file(const char *path)
{
    FILE *file = fopen(path, "r");
    if (file == nullptr) {
        return {};
    }

    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return {};
    }

    const long size = ftell(file);
    if (size < 0) {
        fclose(file);
        return {};
    }
    rewind(file);

    std::string content(static_cast<size_t>(size), '\0');
    const size_t read = fread(content.data(), 1, content.size(), file);
    fclose(file);
    content.resize(read);
    return content;
}

}  // namespace

esp_err_t NetworkConfigStore::init()
{
    // Factory default: DHCP with fallback to the IPBuilding hub convention.
    // Matches CONFIG_ETH_STATIC_IP/GW/NETMASK so behaviour is unchanged from
    // pre-E3 firmware when network.json is absent.
    eth_ = EthNetworkConfig{};
    eth_.mode = EthNetworkConfig::Mode::Dhcp;
    eth_.dhcp_fallback_static = true;
    eth_.ip = CONFIG_ETH_STATIC_IP;
    eth_.netmask = CONFIG_ETH_STATIC_NETMASK;
    eth_.gateway = CONFIG_ETH_STATIC_GW;

    const esp_err_t mount_err = storage::mount_littlefs();
    if (mount_err != ESP_OK) {
        ESP_LOGW(TAG, "LittleFS mount failed (%s) — factory default eth config (dhcp, fallback %s)",
                 esp_err_to_name(mount_err), eth_.ip.c_str());
        return ESP_OK;
    }

    const std::string json_text = read_file(kNetworkJsonPath);
    if (json_text.empty()) {
        ESP_LOGI(TAG, "No network.json on LittleFS — factory default eth config (dhcp, fallback %s)",
                 eth_.ip.c_str());
        return ESP_OK;
    }

    return parse_json(json_text.c_str());
}

esp_err_t NetworkConfigStore::parse_json(const char *json_text)
{
    cJSON *root = cJSON_Parse(json_text);
    if (root == nullptr) {
        ESP_LOGE(TAG, "Failed to parse network.json — using factory default eth config");
        return ESP_ERR_INVALID_ARG;
    }

    const cJSON *eth = cJSON_GetObjectItemCaseSensitive(root, "eth");
    if (!cJSON_IsObject(eth)) {
        cJSON_Delete(root);
        ESP_LOGW(TAG, "network.json missing \"eth\" object — using factory default eth config");
        return ESP_ERR_INVALID_ARG;
    }

    const cJSON *mode = cJSON_GetObjectItemCaseSensitive(eth, "mode");
    const bool is_static = cJSON_IsString(mode) && std::strcmp(mode->valuestring, "static") == 0;

    if (is_static) {
        const cJSON *ip = cJSON_GetObjectItemCaseSensitive(eth, "ip");
        const cJSON *netmask = cJSON_GetObjectItemCaseSensitive(eth, "netmask");
        const cJSON *gateway = cJSON_GetObjectItemCaseSensitive(eth, "gateway");
        if (!cJSON_IsString(ip) || !cJSON_IsString(netmask) || !cJSON_IsString(gateway)) {
            cJSON_Delete(root);
            ESP_LOGW(TAG, "network.json mode=static missing ip/netmask/gateway — using factory default eth config");
            return ESP_ERR_INVALID_ARG;
        }

        eth_.mode = EthNetworkConfig::Mode::Static;
        eth_.dhcp_fallback_static = false;
        eth_.ip = ip->valuestring;
        eth_.netmask = netmask->valuestring;
        eth_.gateway = gateway->valuestring;
        ESP_LOGI(TAG, "network.json: eth mode=static ip=%s gw=%s mask=%s", eth_.ip.c_str(), eth_.gateway.c_str(),
                 eth_.netmask.c_str());
        cJSON_Delete(root);
        return ESP_OK;
    }

    // mode == "dhcp" (default) or unspecified — factory defaults already
    // set in init(); only override fields explicitly present.
    eth_.mode = EthNetworkConfig::Mode::Dhcp;

    const cJSON *fallback_enabled = cJSON_GetObjectItemCaseSensitive(eth, "dhcp_fallback_static");
    if (cJSON_IsBool(fallback_enabled)) {
        eth_.dhcp_fallback_static = cJSON_IsTrue(fallback_enabled);
    }

    const cJSON *fallback_ip = cJSON_GetObjectItemCaseSensitive(eth, "fallback_ip");
    const cJSON *fallback_netmask = cJSON_GetObjectItemCaseSensitive(eth, "fallback_netmask");
    const cJSON *fallback_gateway = cJSON_GetObjectItemCaseSensitive(eth, "fallback_gateway");
    if (cJSON_IsString(fallback_ip)) {
        eth_.ip = fallback_ip->valuestring;
    }
    if (cJSON_IsString(fallback_netmask)) {
        eth_.netmask = fallback_netmask->valuestring;
    }
    if (cJSON_IsString(fallback_gateway)) {
        eth_.gateway = fallback_gateway->valuestring;
    }

    ESP_LOGI(TAG, "network.json: eth mode=dhcp fallback=%s (%s)", eth_.dhcp_fallback_static ? "on" : "off",
             eth_.ip.c_str());
    cJSON_Delete(root);
    return ESP_OK;
}

}  // namespace ipbuilding::config
