#include "e1_command_test.h"

#include "veldbus/payloads/relay.h"
#include "veldbus/udp_bus_manager.h"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace ipbuilding::lab {

namespace {
constexpr const char *TAG = "e1_cmd_test";
constexpr int kGangRelayCh = 10;
constexpr int kBureauDimmerCh = 1;
constexpr int kStepDelayMs = 3000;
constexpr int kStartupDelayMs = 10000;
}  // namespace

void run_e1_command_test()
{
    ESP_LOGI(TAG, "E1.3 command test starting in %d ms (gang ch%d, bureau ch%d)", kStartupDelayMs, kGangRelayCh,
             kBureauDimmerCh);
    vTaskDelay(pdMS_TO_TICKS(kStartupDelayMs));

    using ipbuilding::veldbus::payloads::RelayAction;

    esp_err_t err = ipbuilding::veldbus::send_relay_command("10.10.1.30", kGangRelayCh, RelayAction::On);
    ESP_LOGI(TAG, "gang relay ON: %s", esp_err_to_name(err));
    vTaskDelay(pdMS_TO_TICKS(kStepDelayMs));

    err = ipbuilding::veldbus::send_relay_command("10.10.1.30", kGangRelayCh, RelayAction::Off);
    ESP_LOGI(TAG, "gang relay OFF: %s", esp_err_to_name(err));
    vTaskDelay(pdMS_TO_TICKS(kStepDelayMs));

    err = ipbuilding::veldbus::send_dimmer_command("10.10.1.40", kBureauDimmerCh, 30);
    ESP_LOGI(TAG, "bureau dimmer 30%%: %s", esp_err_to_name(err));
    vTaskDelay(pdMS_TO_TICKS(kStepDelayMs));

    err = ipbuilding::veldbus::send_dimmer_command("10.10.1.40", kBureauDimmerCh, 0);
    ESP_LOGI(TAG, "bureau dimmer OFF: %s", esp_err_to_name(err));

    ESP_LOGI(TAG, "E1.3 command test sequence complete");
}

}  // namespace ipbuilding::lab
