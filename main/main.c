/******************************************************************************
 * Copyright    Victor All Rights Reserved
 * @file 		main.c
 * @brief		ESP32传感器对接Onenet MQTT
 * @author 		Victor<vic.zkj@gmail.com>
 * @date 		2022/1/11
 * @version 	0.0.2
 * @par Revision History:
 ******************************************************************************/
#include "easyio.h"

#define LED 2
#define KEY 0
#define INCLUDE_vTaskSuspend 1

#define PATTERN_CHR_NUM (3)

#define BIT_0 (1 << 0)
#define BIT_1 (1 << 1)
#define BIT_2 (1 << 2)

#define SYS_TOPIC "$sys/"
#define SUFFIX_ATTR_TOPIC "/thing/property/post"
#define SUFFIX_ATTR_REPLY_TOPIC "/thing/property/post/reply"
#define SUFFIX_SET_TOPIC "/thing/property/set"
#define SUFFIX_SET_REPLY_TOPIC "/thing/property/set/reply"
#define SUFFIX_EVENT_TOPIC "/thing/event/post"
#define SUFFIX_EVENT_REPLY_TOPIC "/thing/event/post/reply"

bool bit_sub_post; // post主题订阅成功标志位。0-未订阅；1-订阅成功。
bool bit_sub_set;  // set主题订阅成功标志位。0-未订阅；1-订阅成功。

char device_key[140] = "version=2018-10-31&res=products%2FMM7lNSfMRo%2Fdevices%2FESP32_6d171554&et=1736922096&method=sha1&sign=qUPcabS80P%2B1iQR8iRh%2F0w%2BDmMo%3D";
char pId[10] = "MM7lNSfMRo";
char esp_mac_name[32] = "";

char property_topic[64] = "";
char property_reply_topic[64] = "";

char set_topic[64] = "";
char set_reply_topic[64] = "";

char event_topic[64] = "";
char event_reply_topic[64] = "";

TaskHandle_t led_task_Handler;
TaskHandle_t mqtt_app_start_Handler;
TaskHandle_t publish_Handler;

QueueHandle_t uart1_queue;
QueueHandle_t sensor_Handle;

EventGroupHandle_t xCreatedEventGroup;
EventBits_t uxBits;

esp_mqtt_client_handle_t mqtt_client;

static const char *TAG = "app";

char cmd_value[8] = {0}; // 获取的命令值

struct sensor_7in1_data
{
    int CO2;
    int CH2O;
    int TVOC;
    int PM25;
    int PM10;
    float Temp;
    float Humi;
} s7in1;

/*************************************************************************
 * @brief MQTT协议数据格式     直接使用printf(cJSON_Print());会造成内存泄漏
 *
 * @param
 *
 * @returns
 *************************************************************************/
char *packet_json(struct sensor_7in1_data *pxRxedMessage)
{
    ESP_LOGI(TAG, "CO2: %d CH2O : %d TVOC : %d PM25 : %d PM10 : %d Temp : %.2f Humi : %.2f", pxRxedMessage->CO2, pxRxedMessage->CH2O, pxRxedMessage->TVOC, pxRxedMessage->PM25, pxRxedMessage->PM10, pxRxedMessage->Temp, pxRxedMessage->Humi);
    char packet_id[18] = {0};
    time_t now;
    time(&now);
    sprintf(packet_id, "%ld", now);

    cJSON *pRoot = cJSON_CreateObject();
    cJSON_AddStringToObject(pRoot, "id", packet_id);
    cJSON_AddStringToObject(pRoot, "version", "1.0");

    cJSON *pParams = cJSON_CreateObject();
    cJSON_AddItemToObject(pRoot, "params", pParams);

    cJSON *pCO2 = cJSON_CreateObject();
    cJSON_AddItemToObject(pParams, "CO2", pCO2);
    cJSON_AddNumberToObject(pCO2, "value", pxRxedMessage->CO2);

    cJSON *pCH2O = cJSON_CreateObject();
    cJSON_AddItemToObject(pParams, "CH2O", pCH2O);
    cJSON_AddNumberToObject(pCH2O, "value", pxRxedMessage->CH2O);

    cJSON *pTVOC = cJSON_CreateObject();
    cJSON_AddItemToObject(pParams, "TVOC", pTVOC);
    cJSON_AddNumberToObject(pTVOC, "value", pxRxedMessage->TVOC);

    cJSON *pPM25 = cJSON_CreateObject();
    cJSON_AddItemToObject(pParams, "PM25", pPM25);
    cJSON_AddNumberToObject(pPM25, "value", pxRxedMessage->PM25);

    cJSON *pPM10 = cJSON_CreateObject();
    cJSON_AddItemToObject(pParams, "PM10", pPM10);
    cJSON_AddNumberToObject(pPM10, "value", pxRxedMessage->PM10);

    cJSON *pTemp = cJSON_CreateObject();
    cJSON_AddItemToObject(pParams, "Temperature", pTemp);
    cJSON_AddNumberToObject(pTemp, "value", (int)(pxRxedMessage->Temp * 100) / 100);

    cJSON *pRH = cJSON_CreateObject();
    cJSON_AddItemToObject(pParams, "RelativeHumidity", pRH);
    cJSON_AddNumberToObject(pRH, "value", (int)(pxRxedMessage->Humi * 100) / 100);

    char *sendData = cJSON_PrintUnformatted(pRoot);
    // memset(packet_id, 0, sizeof(packet_id));
    ESP_LOGI(TAG, "up: --> %s", sendData);
    cJSON_Delete(pRoot); // 释放cJSON_CreateObject ()分配出来的内存空间
    return sendData;
}

/*************************************************************************
 * @brief Json数据解析（使用cJSON）
 *
 * @param rdata MQTT收到的控制json报文
 *
 * @returns int 状态灯的状态。0/1。错误时返回-1。
 *************************************************************************/
void cjson_mqtt_parse(char *rdata)
{
    // // 判断是否为json格式
    cJSON *pJsonRoot = cJSON_Parse(rdata);
    // // 如果是json格式数据，则开始解析
    // if (pJsonRoot != NULL)
    // {
    //     // 解析 params，获得 物模型参数
    //     cJSON *pParams = cJSON_GetObjectItem(pJsonRoot, "params");
    //     if (pParams != NULL)
    //     {
    //         // 解析 params -> StatusLightSwitch，获得 状态灯开关 状态
    //         cJSON *pStatusLightSwitch = cJSON_GetObjectItem(pParams, "StatusLightSwitch");
    //         if (pStatusLightSwitch != NULL)
    //         {
    //             int statusLight = pStatusLightSwitch->valueint;
    //             ESP_LOGW(TAG, "\nStatusLightSwitch: %d \n", statusLight);
    //             cJSON_Delete(pJsonRoot); // 释放cJSON_Parse ()分配的内存
    //             return statusLight;
    //         }
    //     }
    // }
    // cJSON_Delete(pJsonRoot); // 释放cJSON_Parse ()分配的内存
    ESP_LOGI("DATA", "<-- %s", cJSON_PrintUnformatted(pJsonRoot));
}

/*************************************************************************
 * @brief NTP校时
 *
 * @param
 *
 * @returns
 *************************************************************************/
void ntpInit()
{
    time_t now;
    struct tm timeinfo;
    time(&now);                   // 获取当前系统时间，UTC时间Unix格式，long类型
    localtime_r(&now, &timeinfo); // 将Unix时间now，转换为当前时区的年月日时分秒，struct tm存储格式。
    if (timeinfo.tm_year < (2021 - 1900))
    {
        ESP_LOGW(TAG, "Time is not set yet. Starting sync system time.");
        obtain_time();                // SNTP校正时间
        time(&now);                   // 重新获取当前系统时间，UTC时间Unix格式，long类型
        localtime_r(&now, &timeinfo); // 将Unix时间now，转换为当前时区的年月日时分秒，struct tm存储格式。
        if (timeinfo.tm_year < (2021 - 1900))
        {
            ESP_LOGW(TAG, "syncing system time failed. System will be restart after 5 seconds.");
            for (int i = 5; i > 0; i--)
            {
                ESP_LOGW(TAG, "Restarting in %d seconds...", i);
                vTaskDelay(1000 / portTICK_PERIOD_MS);
            }
            ESP_LOGW(TAG, "Restarting now.");
            esp_restart();
        }

        time(&now);
    }

#ifdef CONFIG_SNTP_TIME_SYNC_METHOD_SMOOTH
    else
    {
        // add 500 ms error to the current system time.
        // Only to demonstrate a work of adjusting method!
        {
            ESP_LOGI(TAG, "Add a error for test adjtime");
            struct timeval tv_now;
            gettimeofday(&tv_now, NULL);
            int64_t cpu_time = (int64_t)tv_now.tv_sec * 1000000L + (int64_t)tv_now.tv_usec;
            int64_t error_time = cpu_time + 500 * 1000L;
            struct timeval tv_error = {.tv_sec = error_time / 1000000L, .tv_usec = error_time % 1000000L};
            settimeofday(&tv_error, NULL);
        }

        ESP_LOGI(TAG, "Time was set, now just adjusting it. Use SMOOTH SYNC method.");
        obtain_time();
        // update 'now' variable with current time
        time(&now);
    }
#endif

    char strftime_buf[64];

    // Set timezone to China Standard Time
    setenv("TZ", "CST-8", 1);
    tzset();
    localtime_r(&now, &timeinfo);
    strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
    ESP_LOGI(TAG, "The current date/time in Shanghai is: %s", strftime_buf);

    if (sntp_get_sync_mode() == SNTP_SYNC_MODE_SMOOTH)
    {
        struct timeval outdelta;
        while (sntp_get_sync_status() == SNTP_SYNC_STATUS_IN_PROGRESS)
        {
            adjtime(NULL, &outdelta);
            ESP_LOGI(TAG, "Waiting for adjusting time ... outdelta = %li sec: %li ms: %li us",
                     (long)outdelta.tv_sec,
                     outdelta.tv_usec / 1000,
                     outdelta.tv_usec % 1000);
            vTaskDelay(2000 / portTICK_PERIOD_MS);
        }
    }
}

/*************************************************************************
 * @brief 7合1传感器数据转换
 *
 * @param arr 打包结构体 len 报文长度
 *
 * @returns
 *************************************************************************/
void parse_7in1_str(uint8_t *arr, int len)
{
    // ESP_LOGI(TAG, "CO2: %d", arr[2] * 256 + arr[3]);
    // ESP_LOGI(TAG, "CH2O: %d", arr[4] * 256 + arr[5]);
    // ESP_LOGI(TAG, "TVOC: %d", arr[6] * 256 + arr[7]);
    // ESP_LOGI(TAG, "PM25: %d", arr[8] * 256 + arr[9]);
    // ESP_LOGI(TAG, "PM10: %d", arr[10] * 256 + arr[11]);
    // ESP_LOGI(TAG, "Temp: %.2f", arr[12] + arr[13] * 0.1);
    // ESP_LOGI(TAG, "Humi: %.2f", arr[14] + arr[15] * 0.1);
    s7in1.CO2 = arr[2] * 256 + arr[3];
    s7in1.CH2O = arr[4] * 256 + arr[5];
    s7in1.TVOC = arr[6] * 256 + arr[7];
    s7in1.PM25 = arr[8] * 256 + arr[9];
    s7in1.PM10 = arr[10] * 256 + arr[11];
    s7in1.Temp = arr[12] + arr[13] * 0.1;
    s7in1.Humi = arr[14] + arr[15] * 0.1;
}

/*************************************************************************
 * @brief 串口读取Task
 *
 * @param
 *
 * @returns
 *************************************************************************/
void uart_rx_task(void *pvParameters)
{
    // uxBits = xEventGroupWaitBits(
    //     xCreatedEventGroup, /* 事件句柄 */
    //     BIT_0 | BIT_1,      /* 标志位  */
    //     pdTRUE,             /* 被设置的位退出会被清零 */
    //     pdTRUE,             /* 同时设置BIT才会解除阻塞状态 */
    //     portMAX_DELAY);     /* 超时时间 */

    uart_event_t event;
    int length = 0;
    uint8_t *dtmp = (uint8_t *)malloc(RX_BUF_SIZE + 1);
    uart_init_no_hwfc(UART_NUM_1, 9600, GPIO_NUM_15, GPIO_NUM_13, 10, &uart1_queue);
    for (;;)
    {
        // Waiting for UART event.
        if (xQueueReceive(uart1_queue, (void *)&event, (portTickType)portMAX_DELAY))
        {
            // bzero(dtmp, RX_BUF_SIZE); // 不用清空也可以
            // ESP_LOGI(TAG, "uart[%d] event:", UART_NUM_1); // 使用 ESP_LOGI 和 uart_sendData，单次接收字段长度超过120时，显示效果是不一样的
            // uart_sendData(UART_NUM_1, "\r\nuart0 event::\r\n");
            switch (event.type)
            {
            // Event of UART receving data
            /*We'd better handler data event fast, there would be much more data events than
                other types of events. If we take too much time on data event, the queue might
                be full.*/
            case UART_DATA:
                uart_get_buffered_data_len(UART_NUM_1, (size_t *)&length);
                // uart_sendData(UART_NUM_1, "\r\n+RECV:\r\n");
                int len = uart_read_bytes(UART_NUM_1, dtmp, event.size, portMAX_DELAY);
                // 打印原始16进制数据
                int i = 0;
                dtmp[len] = 0;
                char sensor_hex_str[64] = "";
                while (i < len)
                {
                    char hex_data[4];
                    sprintf(hex_data, "%02X", dtmp[i]);
                    strcat(sensor_hex_str, hex_data);
                    i += 1;
                }
                ESP_LOGI(TAG, "initial data: %s", sensor_hex_str);
                parse_7in1_str(dtmp, len);
                break;
            // Event of HW FIFO overflow detected
            case UART_FIFO_OVF:
                ESP_LOGI(TAG, "hw fifo overflow");
                // If fifo overflow happened, you should consider adding flow control for your application.
                // The ISR has already reset the rx FIFO,
                // As an example, we directly flush the rx buffer here in order to read more data.
                uart_flush_input(UART_NUM_1);
                xQueueReset(uart1_queue);
                break;
            // Event of UART ring buffer full
            case UART_BUFFER_FULL:
                ESP_LOGI(TAG, "ring buffer full");
                // If buffer full happened, you should consider encreasing your buffer size
                // As an example, we directly flush the rx buffer here in order to read more data.
                uart_flush_input(UART_NUM_1);
                xQueueReset(uart1_queue);
                break;
            // Event of UART RX break detected
            case UART_BREAK:
                ESP_LOGI(TAG, "uart rx break");
                break;
            // Event of UART parity check error
            case UART_PARITY_ERR:
                ESP_LOGI(TAG, "uart parity error");
                break;
            // Event of UART frame error
            case UART_FRAME_ERR:
                ESP_LOGI(TAG, "uart frame error");
                break;
            default:
                ESP_LOGI(TAG, "uart event type: %d", event.type);
                break;
            }
        }
    }
    free(dtmp);
    dtmp = NULL;
    vTaskDelete(NULL);
}

/*************************************************************************
 * @brief 获取ESP32的MAC，并根据设备MAC自动合成 clientID、topic_posted、topic_issued
 *
 * @param : pid product id
 *
 * @returns : none
 *************************************************************************/
void get_device_mac_clientid_topic(char *pid)
{
    uint8_t mac[6];
    char mac_str[24] = {0};
    esp_efuse_mac_get_default(mac); // 获取ESP32的MAC，6个字节
    //esp_read_mac(mac, ESP_MAC_WIFI_STA); // 同样的获取MAC
    // printf("mac: ");
    // for (int i = 0; i < 6; i++)
    //     printf("%02x ", mac[i]);
    // printf("\r\n");
    for (int i = 0; i < 6; i++)
    {
        char mac02str[3] = {0};
        sprintf(mac02str, "%02X", mac[i]);
        strcat(mac_str, mac02str);
        if (i <= 4)
        {
            strcat(mac_str, ":");
        }
    }
    ESP_LOGI(TAG, "ESPMAC: %s", mac_str);

    sprintf(esp_mac_name, "ESP32_%.2x%.2x%.2x%.2x", mac[2], mac[3], mac[4], mac[5]);

    strcat(property_topic, SYS_TOPIC);
    strcat(property_topic, pid);
    strcat(property_topic, "/");
    strcat(property_topic, esp_mac_name);
    strcat(property_topic, SUFFIX_ATTR_TOPIC);
    ESP_LOGI(TAG, "设备属性上报主题: %s", property_topic);

    strcat(property_reply_topic, SYS_TOPIC);
    strcat(property_reply_topic, pid);
    strcat(property_reply_topic, "/");
    strcat(property_reply_topic, esp_mac_name);
    strcat(property_reply_topic, SUFFIX_ATTR_REPLY_TOPIC);
    ESP_LOGI(TAG, "设备属性上报应答主题: %s", property_reply_topic);

    strcat(set_topic, SYS_TOPIC);
    strcat(set_topic, pid);
    strcat(set_topic, "/");
    strcat(set_topic, esp_mac_name);
    strcat(set_topic, SUFFIX_SET_TOPIC);
    ESP_LOGI(TAG, "设备属性设置(同步)主题: %s", set_topic);

    strcat(set_reply_topic, SYS_TOPIC);
    strcat(set_reply_topic, pid);
    strcat(set_reply_topic, "/");
    strcat(set_reply_topic, esp_mac_name);
    strcat(set_reply_topic, SUFFIX_SET_REPLY_TOPIC);
    ESP_LOGI(TAG, "设备属性设置(同步)应答主题: %s", set_reply_topic);

    strcat(event_topic, SYS_TOPIC);
    strcat(event_topic, pid);
    strcat(event_topic, "/");
    strcat(event_topic, esp_mac_name);
    strcat(event_topic, SUFFIX_EVENT_TOPIC);
    ESP_LOGI(TAG, "设备事件上报主题: %s", event_topic);

    strcat(event_reply_topic, SYS_TOPIC);
    strcat(event_reply_topic, pid);
    strcat(event_reply_topic, "/");
    strcat(event_reply_topic, esp_mac_name);
    strcat(event_reply_topic, SUFFIX_EVENT_REPLY_TOPIC);
    ESP_LOGI(TAG, "设备事件上报应答主题: %s", event_reply_topic);
}

esp_err_t mqtt_event_handler_cb(esp_mqtt_event_handle_t event)
{
    esp_mqtt_client_handle_t client = event->client;
    // int msg_id = 0;
    static int post_sub_id = 0;  // 订阅post主题的消息ID
    static int set_sub_id = 0;   // 订阅set主题的消息ID
    static int event_sub_id = 0; // 订阅event主题的消息ID
    static char topic_name[100]; // 缓存接收消息的 主题名称，在允许范围内尽量长些
    static char recv_dat[4096];  // 缓存接收到的消息，在允许范围内尽量长些
    // your_context_t *context = event->context;
    switch (event->event_id)
    {
    case MQTT_EVENT_CONNECTED:
        // 1.客户端已成功建立与代理的连接。之后可以开始订阅主题
        ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");

        // 在绝大多数实际应用场景中，一般分别订阅 收/发 主题，以 Qos0 方式订阅
        post_sub_id = esp_mqtt_client_subscribe(client, property_reply_topic, 0); // 订阅
        ESP_LOGI(TAG, "sent subscribe successful, \"%s\", msg_id=%d", property_reply_topic, post_sub_id);

        // set_sub_id = esp_mqtt_client_subscribe(client, set_reply_topic, 0);
        // ESP_LOGI(TAG, "sent subscribe successful, \"%s\", msg_id=%d", set_reply_topic, set_sub_id);

        event_sub_id = esp_mqtt_client_subscribe(client, event_reply_topic, 0);
        ESP_LOGI(TAG, "sent subscribe successful, \"%s\", msg_id=%d", event_reply_topic, event_sub_id);

        // 订阅后，上发一条测试消息
        // msg_id = esp_mqtt_client_publish(client, event_topic, "Online", 0, 0, 0);
        // ESP_LOGI(TAG, "online event sent publish successful, msg_id=%d", msg_id);
        break;

    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "Offline!");

        break;

    case MQTT_EVENT_SUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
        // 2.代理服务器已确认客户端的订阅请求。
        // 这里不能由 event 直接获得主题名称，需要由 msg_id 来判断先前的哪条主题已被订阅
        // 置位订阅成功标志位。主任务检测到订阅成功后，会定时上发消息
        if (event->msg_id == post_sub_id)
        {
            bit_sub_post = 1;
        }
        else if (event->msg_id == set_sub_id)
        {
            bit_sub_set = 1;
        }
        break;

    case MQTT_EVENT_UNSUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_PUBLISHED:
        ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
        break;

    case MQTT_EVENT_DATA:
        // ESP_LOGI(TAG, "MQTT_EVENT_DATA");
        // 3.客户端已收到发布的消息（客户端自己发布的消息，也会因为主题的广播机制而再接收到一次）。调试输出 接收到消息的主题、数据、数据长度
        // ESP_LOGI(TAG, "TOPIC = \"%.*s\"", event->topic_len, event->topic);
        ESP_LOGI(TAG, "down: <-- \"%.*s\", num = %d", event->data_len, event->data, event->data_len);

        // cJSON数据解析
        // cjson_mqtt_parse(event->data);

        // 转移主题名称和数据，用于后续解析
        memcpy(topic_name, event->topic, event->topic_len);
        memcpy(recv_dat, event->data, event->data_len);

        // 解析服务器下发主题 "/topic/set" 的消息，控制led运行状态，并上发回复报文 Done
        // ESP_LOGI(TAG, "cmd_value: %s", cmd_value);
        // if (strstr(topic_name, topic_issued))
        // {
        //     if (strcmp(cmd_value, "0") == 0)
        //     {
        //         // vTaskSuspend(led_task_Handler); // 暂停任务，LED停止闪烁
        //         led_off(LED);
        //     }
        //     else if (strcmp(cmd_value, "1") == 0)
        //     {
        //         // vTaskResume(led_task_Handler); // 继续任务，LED继续闪烁
        //         led_on(LED);
        //     }
        //     msg_id = esp_mqtt_client_publish(client, topic_posted, "Done", 0, 0, 0);
        //     ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);
        // }
        break;

    case MQTT_EVENT_ERROR:
        ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
        break;
    default:
        ESP_LOGI(TAG, "Other event id:%d", event->event_id);
        break;
    }
    return ESP_OK;
}

void mqtt_event_handler(void *handler_pvParameterss, esp_event_base_t base, int32_t event_id, void *event_data)
{
    ESP_LOGD(TAG, "Event dispatched from event loop base=%s, event_id=%d", base, event_id);
    mqtt_event_handler_cb(event_data);
}

void mqtt_app_start_task(void *pvParameters)
{
    // 获取ESP32的MAC，并根据设备MAC自动合成 clientID、topic_posted、topic_issued
    get_device_mac_clientid_topic("MM7lNSfMRo");

    // MQTT客户端配置。如果不额外配置，则默认 port = 1883，keepalive = 120s，开启自动重连，自动重连超时时间为 10s。
    // 不同书写方式的 uri，其默认端口不同，详见：https://docs.espressif.com/projects/esp-idf/zh_CN/latest/esp32/api-reference/protocols/mqtt.html
    esp_mqtt_client_config_t mqtt_cfg = {
        //.uri = CONFIG_BROKER_URL,             // 使用menuconfig配置项的 MQTT代理URL
        .uri = "mqtt://studio-mqtt.heclouds.com", /*!< Complete MQTT broker URI */
        .port = 1883,
        .client_id = esp_mac_name, /*!< default client id is ``ESP32_%CHIPID%`` where %CHIPID% are last 3 bytes of MAC address in hex format */
        .username = pId,           /*!< MQTT username */
        .password = device_key,    /*!< MQTT password */
        //.keepalive = 30,                  /*!< mqtt keepalive, default is 120 seconds */
        // .reconnect_timeout_ms = 2000,     /*!< Reconnect to the broker after this value in miliseconds if auto reconnect is not disabled (defaults to 10s) */
    };

    esp_mqtt_client_handle_t client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, client);
    esp_mqtt_client_start(client);
    while (1)
    {
        if (bit_sub_post)
        {
            char *payload;
            payload = packet_json(&s7in1);                                                  // 合成上报温湿度传感器信息的json字段
            int msg_id = esp_mqtt_client_publish(client, property_topic, payload, 0, 0, 0); // 发布消息
            cJSON_free((void *)payload);                                                    // 释放cJSON_Print ()分配出来的内存空间
        }
        vTaskDelay(2000 / portTICK_PERIOD_MS);
        ESP_LOGW(TAG, "[APP] Free memory: %d bytes", esp_get_free_heap_size()); // 调试输出空闲内存的空间，及时定位因内存溢出而导致的故障
    }
}

void app_main(void)
{

    esp_log_level_set("*", ESP_LOG_INFO);
    esp_log_level_set("MQTT_CLIENT", ESP_LOG_VERBOSE);
    esp_log_level_set("MQTT_EXAMPLE", ESP_LOG_VERBOSE);
    esp_log_level_set("TRANSPORT_TCP", ESP_LOG_VERBOSE);
    esp_log_level_set("TRANSPORT_SSL", ESP_LOG_VERBOSE);
    esp_log_level_set("TRANSPORT", ESP_LOG_VERBOSE);
    esp_log_level_set("OUTBOX", ESP_LOG_VERBOSE);
    esp_log_level_set("ONENET", ESP_LOG_INFO);

    ESP_LOGI(TAG, "Free memory: %d bytes", esp_get_free_heap_size());
    ESP_LOGI(TAG, "IDF version: %s", esp_get_idf_version());

    led_init(LED, 0);
    // nvs_flash_erase();

    smartconfigInit();
    ntpInit();

    // struct addrinfo hint;
    // struct addrinfo *res = NULL;
    // memset(&hint, 0, sizeof(hint));
    // err = getaddrinfo("g.cn", NULL, &hint, &res);

    vTaskSuspendAll();
    xTaskCreate(mqtt_app_start_task, "mqtt_app_start_task", 4096, NULL, 2, NULL);
    xTaskCreate(uart_rx_task, "uart_rx_task", 2048, NULL, 1, NULL);
    xTaskResumeAll();
}
