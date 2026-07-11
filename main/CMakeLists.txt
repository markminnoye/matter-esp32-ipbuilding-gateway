idf_component_register(
    SRCS
        "app_main.cpp"
        "board/waveshare_s3_eth.cpp"
        "network/network_manager.cpp"
        "matter/matter_init.cpp"
    INCLUDE_DIRS
        "."
        "board"
        "network"
        "matter"
    REQUIRES
        nvs_flash
        esp_netif
        esp_event
        esp_eth
        esp_wifi
        driver
        esp_matter
)

target_compile_options(${COMPONENT_LIB} PRIVATE "-DCHIP_HAVE_CONFIG_H")
