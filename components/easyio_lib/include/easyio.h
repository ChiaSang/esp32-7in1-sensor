#ifndef __EASYIO_H__
#define __EASYIO_H__

#include <stdio.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/event_groups.h"
#include "driver/gpio.h"
#include "sdkconfig.h"
#include <dirent.h>

#include "led.h"
#include "gpioX.h"
#include "key.h"
#include "touch_pad_button.h"
#include "ledc_pwm.h"
#include "esp_log.h"
#include "uart_config.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "sntp_systime.h"
#include "wifi_scan_print.h"
#include "wifi_smartconfig.h"

#include <string.h>
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include <sys/param.h>
#include "esp_netif.h"
#include "protocol_examples_common.h"
#include "addr_from_stdin.h"
#include "lwip/err.h"
#include "lwip/sys.h"
#include "lwip/sockets.h"
#include "lwip/netdb.h"
#include "lwip/dns.h"

#include "cJSON.h"
#include <stdint.h>
#include <stddef.h>
#include "mqtt_client.h"

#endif
