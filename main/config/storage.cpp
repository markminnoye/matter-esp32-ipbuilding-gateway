#include "storage.h"

#include "esp_littlefs.h"
#include "esp_log.h"

namespace ipbuilding::storage {

namespace {
constexpr const char *TAG = "storage";
constexpr const char *kBasePath = "/littlefs";
constexpr const char *kPartitionLabel = "littlefs";
bool s_mounted = false;
}  // namespace

esp_err_t mount_littlefs()
{
    if (s_mounted) {
        return ESP_OK;
    }

    esp_vfs_littlefs_conf_t conf = {};
    conf.format_if_mount_failed = true;
    conf.partition_label = kPartitionLabel;
    conf.base_path = kBasePath;

    const esp_err_t err = esp_vfs_littlefs_register(&conf);
    if (err == ESP_OK || err == ESP_ERR_INVALID_STATE) {
        // ESP_ERR_INVALID_STATE means an earlier mount_littlefs() call
        // already registered this partition — treat as success.
        s_mounted = true;
        return ESP_OK;
    }

    ESP_LOGW(TAG, "LittleFS mount failed: %s", esp_err_to_name(err));
    return err;
}

}  // namespace ipbuilding::storage
