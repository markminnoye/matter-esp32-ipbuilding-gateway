#include "board_profile.h"

#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_check.h"
#include "esp_eth.h"
#include "esp_eth_mac_spi.h"
#include "esp_eth_phy.h"
#include "esp_log.h"

namespace ipbuilding::board {

static const char *TAG = "waveshare_s3_eth";

const BoardProfile &get_board_profile()
{
    static const BoardProfile kProfile = {
        .spi_host = SPI2_HOST,
        .spi_sclk_gpio = 13,
        .spi_mosi_gpio = 11,
        .spi_miso_gpio = 12,
        .spi_cs_gpio = 14,
        .spi_int_gpio = 10,
        .spi_rst_gpio = 9,
        .spi_clock_mhz = 25,
    };
    return kProfile;
}

esp_err_t init_w5500_ethernet(esp_eth_handle_t *eth_handle_out)
{
    ESP_RETURN_ON_FALSE(eth_handle_out != nullptr, ESP_ERR_INVALID_ARG, TAG, "eth_handle_out is null");

    const BoardProfile &board = get_board_profile();

    ESP_RETURN_ON_ERROR(gpio_install_isr_service(0), TAG, "gpio_install_isr_service failed");

    spi_bus_config_t buscfg = {};
    buscfg.miso_io_num = board.spi_miso_gpio;
    buscfg.mosi_io_num = board.spi_mosi_gpio;
    buscfg.sclk_io_num = board.spi_sclk_gpio;
    buscfg.quadwp_io_num = -1;
    buscfg.quadhd_io_num = -1;

    ESP_RETURN_ON_ERROR(spi_bus_initialize(static_cast<spi_host_device_t>(board.spi_host), &buscfg, SPI_DMA_CH_AUTO),
                        TAG, "spi_bus_initialize failed");

    spi_device_interface_config_t spi_devcfg = {};
    spi_devcfg.mode = 0;
    spi_devcfg.clock_speed_hz = board.spi_clock_mhz * 1000 * 1000;
    spi_devcfg.spics_io_num = board.spi_cs_gpio;
    spi_devcfg.queue_size = 20;

    eth_w5500_config_t w5500_config =
        ETH_W5500_DEFAULT_CONFIG(static_cast<spi_host_device_t>(board.spi_host), &spi_devcfg);
    w5500_config.int_gpio_num = board.spi_int_gpio;

    eth_mac_config_t mac_config = ETH_MAC_DEFAULT_CONFIG();
    mac_config.rx_task_stack_size = 4096;
    esp_eth_mac_t *mac = esp_eth_mac_new_w5500(&w5500_config, &mac_config);
    ESP_RETURN_ON_FALSE(mac != nullptr, ESP_FAIL, TAG, "esp_eth_mac_new_w5500 failed");

    eth_phy_config_t phy_config = ETH_PHY_DEFAULT_CONFIG();
    phy_config.reset_gpio_num = board.spi_rst_gpio;
    esp_eth_phy_t *phy = esp_eth_phy_new_w5500(&phy_config);
    if (phy == nullptr) {
        mac->del(mac);
        ESP_LOGE(TAG, "esp_eth_phy_new_w5500 failed");
        return ESP_FAIL;
    }

    esp_eth_config_t eth_config = ETH_DEFAULT_CONFIG(mac, phy);
    esp_eth_handle_t eth_handle = nullptr;
    esp_err_t err = esp_eth_driver_install(&eth_config, &eth_handle);
    if (err != ESP_OK) {
        phy->del(phy);
        mac->del(mac);
        ESP_LOGE(TAG, "esp_eth_driver_install failed: %s", esp_err_to_name(err));
        return err;
    }

    *eth_handle_out = eth_handle;
    ESP_LOGI(TAG, "W5500 Ethernet driver installed (SPI host %d)", board.spi_host);
    return ESP_OK;
}

}  // namespace ipbuilding::board
