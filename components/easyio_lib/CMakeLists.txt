set(easyio_srcs
    "src/led.c"
    "src/gpioX.c"
    "src/key.c"
    "src/touch_pad_button.c"
    "src/ledc_pwm.c"
    "src/led_strip_rmt_ws2812.c"
    "src/uart_config.c"
    "src/sntp_systime.c"
    "src/wifi_scan_print.c"
    "src/wifi_smartconfig.c")

idf_component_register(SRCS "${easyio_srcs}"
                       INCLUDE_DIRS "include"
                       "${IDF_PATH}/components/esp_adc_cal/include"
                       "${IDF_PATH}/components/console"
                       "${IDF_PATH}/components/spi_flash/include"
                       "${IDF_PATH}/components/fatfs/vfs"
                       "${IDF_PATH}/components/fatfs/src"
                       "${IDF_PATH}/components/wear_levelling/include"
                       "${IDF_PATH}/components/sdmmc/include"
                       "${IDF_PATH}/components/nvs_flash/include"
                       "${IDF_PATH}/components/mqtt/esp-mqtt/include"
                       "${IDF_PATH}/components/json/cJSON"
                       "${IDF_PATH}/examples/common_components/protocol_examples_common/include")

