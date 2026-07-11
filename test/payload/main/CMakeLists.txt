set(MAIN_DIR "${CMAKE_CURRENT_LIST_DIR}/../../..")

idf_component_register(
    SRCS
        "test_payloads.cpp"
        "${MAIN_DIR}/main/veldbus/payloads/relay.cpp"
        "${MAIN_DIR}/main/veldbus/payloads/dimmer.cpp"
        "${MAIN_DIR}/main/veldbus/payloads/input.cpp"
    INCLUDE_DIRS
        "."
        "${MAIN_DIR}/main"
        "${MAIN_DIR}/main/veldbus"
    REQUIRES
        unity
)
