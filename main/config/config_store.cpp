#include "config_store.h"

#include "storage.h"

#include "esp_log.h"

#include "cJSON.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace ipbuilding::config {

namespace {
constexpr const char *TAG = "config_store";
constexpr const char *kDevicesJsonPath = "/littlefs/devices.json";

const char *kEmbeddedDefaultJson = R"({
  "modules": [
    {
      "ip": "10.10.1.30",
      "type": "relay",
      "name": "IP0200PoE",
      "channels": [
        { "ch": 0, "name": "Keuken LED", "room": "Keuken", "semantic_type": "light", "active": true }
      ]
    },
    {
      "ip": "10.10.1.40",
      "type": "dimmer",
      "name": "IP0300PoE",
      "channels": [
        { "ch": 0, "name": "Woonkamer dimmer", "room": "Woonkamer", "active": true }
      ]
    }
  ]
})";

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

esp_err_t ConfigStore::init()
{
    const esp_err_t mount_err = storage::mount_littlefs();
    if (mount_err != ESP_OK) {
        ESP_LOGW(TAG, "LittleFS mount failed (%s) — using embedded default config", esp_err_to_name(mount_err));
        return parse_json(kEmbeddedDefaultJson);
    }

    mounted_ = true;

    std::string json_text = read_file(kDevicesJsonPath);
    if (json_text.empty()) {
        ESP_LOGW(TAG, "No devices.json on LittleFS — using embedded default config");
        return parse_json(kEmbeddedDefaultJson);
    }

    return parse_json(json_text.c_str());
}

registry::SemanticType ConfigStore::parse_semantic_type(const char *value) const
{
    if (value == nullptr) {
        return registry::SemanticType::Unknown;
    }
    if (std::strcmp(value, "light") == 0) {
        return registry::SemanticType::Light;
    }
    if (std::strcmp(value, "fan") == 0) {
        return registry::SemanticType::Fan;
    }
    if (std::strcmp(value, "switch") == 0) {
        return registry::SemanticType::Switch;
    }
    return registry::SemanticType::Unknown;
}

esp_err_t ConfigStore::parse_json(const char *json_text)
{
    modules_.clear();

    cJSON *root = cJSON_Parse(json_text);
    if (root == nullptr) {
        ESP_LOGE(TAG, "Failed to parse devices.json");
        return ESP_ERR_INVALID_ARG;
    }

    const cJSON *modules = cJSON_GetObjectItemCaseSensitive(root, "modules");
    if (!cJSON_IsArray(modules)) {
        cJSON_Delete(root);
        ESP_LOGE(TAG, "devices.json missing modules array");
        return ESP_ERR_INVALID_ARG;
    }

    cJSON *module_item = nullptr;
    cJSON_ArrayForEach(module_item, modules)
    {
        registry::ModuleConfig module;
        const cJSON *ip = cJSON_GetObjectItemCaseSensitive(module_item, "ip");
        const cJSON *type = cJSON_GetObjectItemCaseSensitive(module_item, "type");
        const cJSON *name = cJSON_GetObjectItemCaseSensitive(module_item, "name");
        const cJSON *channels = cJSON_GetObjectItemCaseSensitive(module_item, "channels");
        const cJSON *managed = cJSON_GetObjectItemCaseSensitive(module_item, "managed");

        if (!cJSON_IsString(ip) || !cJSON_IsString(type) || !cJSON_IsArray(channels)) {
            continue;
        }

        module.ip = ip->valuestring;
        module.type = registry::module_type_from_string(type->valuestring);
        if (cJSON_IsString(name)) {
            module.name = name->valuestring;
        }
        module.managed = !cJSON_IsBool(managed) || cJSON_IsTrue(managed);

        cJSON *channel_item = nullptr;
        cJSON_ArrayForEach(channel_item, channels)
        {
            registry::ChannelConfig channel;
            const cJSON *ch = cJSON_GetObjectItemCaseSensitive(channel_item, "ch");
            const cJSON *channel_name = cJSON_GetObjectItemCaseSensitive(channel_item, "name");
            const cJSON *room = cJSON_GetObjectItemCaseSensitive(channel_item, "room");
            const cJSON *semantic_type = cJSON_GetObjectItemCaseSensitive(channel_item, "semantic_type");
            const cJSON *active = cJSON_GetObjectItemCaseSensitive(channel_item, "active");

            if (!cJSON_IsNumber(ch)) {
                continue;
            }

            channel.ch = ch->valueint;
            if (cJSON_IsString(channel_name)) {
                channel.name = channel_name->valuestring;
            }
            if (cJSON_IsString(room)) {
                channel.room = room->valuestring;
            }
            if (cJSON_IsString(semantic_type)) {
                channel.semantic_type = parse_semantic_type(semantic_type->valuestring);
            }
            channel.active = !cJSON_IsBool(active) || cJSON_IsTrue(active);
            module.channels.push_back(std::move(channel));
        }

        modules_.push_back(std::move(module));
    }

    cJSON_Delete(root);
    ESP_LOGI(TAG, "Parsed %zu modules from config", modules_.size());
    return ESP_OK;
}

}  // namespace ipbuilding::config
