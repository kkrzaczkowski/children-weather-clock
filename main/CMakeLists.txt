set(CLOCK_SRCS
    "main.c"
    "app_state.c"
    "board_io.c"
    "config_store.c"
    "http_panel.c"
    "image_decode.c"
    "log_store.c"
    "sdcard.c"
    "templates.c"
    "time_sync.c"
    "ui.c"
    "ui_config.c"
    "weather.c"
    "wifi_mgr.c"
    "logic/clock_logic.c"
    "../components/waveshare_rgb_lcd_port.c"
    "fonts/font_clock_56.c"
    "fonts/font_clock_72.c"
    "fonts/font_clock_88.c"
    "fonts/font_temp_32.c"
    "fonts/font_temp_40.c"
    "fonts/font_temp_48.c"
    "fonts/font_label_20.c"
    "fonts/font_label_28.c"
    "fonts/font_label_36.c")

idf_component_register(
    SRCS ${CLOCK_SRCS}
    INCLUDE_DIRS "." "logic" "../components"
    REQUIRES
        nvs_flash
        esp_wifi
        esp_event
        esp_netif
        esp_http_client
        esp_http_server
        esp_timer
        esp_lcd
        json
        fatfs
        sdmmc
        esp_driver_sdspi
        esp_driver_spi
        esp_driver_gpio
        sd_cs
        esp_driver_i2c
        mbedtls
        espressif__libpng
        lvgl__lvgl
        espressif__esp_lvgl_adapter
        espressif__esp_lcd_touch_gt911
    EMBED_FILES
        "assets/fallback/1.png"
        "assets/fallback/2.png"
        "assets/fallback/3.png"
        "assets/fallback/4.png"
        "assets/fallback/5.png")

get_filename_component(PROJECT_ROOT "${COMPONENT_DIR}/.." ABSOLUTE)
set(SECRETS_FILE "${PROJECT_ROOT}/secrets.env")
set(SECRETS_HEADER "${CMAKE_BINARY_DIR}/generated/secrets.h")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${SECRETS_FILE}")
if(NOT DEFINED PYTHON)
    set(PYTHON python)
endif()
execute_process(
    COMMAND "${PYTHON}" "${PROJECT_ROOT}/tools/gen_secrets.py" "${SECRETS_FILE}" "${SECRETS_HEADER}"
    RESULT_VARIABLE secrets_rc
)
if(NOT secrets_rc EQUAL 0)
    message(FATAL_ERROR "Brak poprawnego pliku secrets.env. Skopiuj secrets.env.example i uzupełnij WIFI_SSID oraz PANEL_PASSWORD.")
endif()
target_include_directories(${COMPONENT_LIB} PUBLIC "${CMAKE_BINARY_DIR}/generated")

idf_build_get_property(build_components BUILD_COMPONENTS)
if("lvgl" IN_LIST build_components)
    idf_component_get_property(lvgl_lib lvgl COMPONENT_LIB)
elseif("lvgl__lvgl" IN_LIST build_components)
    idf_component_get_property(lvgl_lib lvgl__lvgl COMPONENT_LIB)
endif()

if(lvgl_lib)
    target_compile_options(${lvgl_lib} PRIVATE -Wno-format)
endif()

# Rename chip-select calls inside the SDSPI driver only. GPIO6 is a token;
# sd_cs mirrors the level to EXIO4 and does not drive the pad.
idf_component_get_property(sdspi esp_driver_sdspi COMPONENT_LIB)
target_compile_definitions(${sdspi} PRIVATE
    "gpio_set_level=sd_cs_gpio_set_level"
    "gpio_config=sd_cs_gpio_config")
idf_component_get_property(sd_cs_lib sd_cs COMPONENT_LIB)
target_link_libraries(${sdspi} PUBLIC ${sd_cs_lib})
