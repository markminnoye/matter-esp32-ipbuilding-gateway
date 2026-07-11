#pragma once

#include "esp_eth.h"
#include "esp_err.h"

namespace ipbuilding::board {

struct BoardProfile {
    int spi_host;
    int spi_sclk_gpio;
    int spi_mosi_gpio;
    int spi_miso_gpio;
    int spi_cs_gpio;
    int spi_int_gpio;
    int spi_rst_gpio;
    int spi_clock_mhz;
};

const BoardProfile &get_board_profile();

esp_err_t init_w5500_ethernet(esp_eth_handle_t *eth_handle_out);

}  // namespace ipbuilding::board
