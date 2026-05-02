#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_netif.h"

#include "lwip/sockets.h"
#include "lwip/inet.h"

#define WIFI_SSID       "WE_C7354E"
#define WIFI_PASS       "20144497500"

#define LAPTOP_IP       "192.168.8.245"
#define LAPTOP_PORT     5005

#define DEVICE_ID       "rx3"

static const char *TAG = "CSI_UDP";

static EventGroupHandle_t wifi_event_group;
#define WIFI_CONNECTED_BIT BIT0

static int udp_sock = -1;
static struct sockaddr_in dest_addr;

#pragma pack(push, 1)
typedef struct {
    char device_id[8];
    uint32_t esp_timestamp;
    int8_t rssi;
    int8_t channel;
    uint16_t csi_len;
} csi_packet_header_t;
#pragma pack(pop)

static void udp_init(void)
{
    if (udp_sock >= 0) {
        return;
    }

    udp_sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);

    if (udp_sock < 0) {
        ESP_LOGE(TAG, "Failed to create UDP socket");
        return;
    }

    dest_addr.sin_addr.s_addr = inet_addr(LAPTOP_IP);
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(LAPTOP_PORT);

    ESP_LOGI(TAG, "UDP ready. Sending to %s:%d", LAPTOP_IP, LAPTOP_PORT);
}

static void wifi_csi_rx_cb(void *ctx, wifi_csi_info_t *info)
{
    if (!info || !info->buf || udp_sock < 0) {
        return;
    }

    uint8_t packet[2048];

    csi_packet_header_t header = {0};

    strncpy(header.device_id, DEVICE_ID, sizeof(header.device_id) - 1);
    header.esp_timestamp = info->rx_ctrl.timestamp;
    header.rssi = info->rx_ctrl.rssi;
    header.channel = info->rx_ctrl.channel;
    header.csi_len = info->len;

    int header_size = sizeof(csi_packet_header_t);

    if (header_size + info->len > sizeof(packet)) {
        return;
    }

    memcpy(packet, &header, header_size);
    memcpy(packet + header_size, info->buf, info->len);

    sendto(
        udp_sock,
        packet,
        header_size + info->len,
        0,
        (struct sockaddr *)&dest_addr,
        sizeof(dest_addr)
    );
}

static void csi_init(void)
{
    wifi_csi_config_t csi_config = {
        .lltf_en = true,
        .htltf_en = true,
        .stbc_htltf2_en = true,
        .ltf_merge_en = true,
        .channel_filter_en = false,
        .manu_scale = false,
        .shift = false,
    };

    ESP_ERROR_CHECK(esp_wifi_set_csi_rx_cb(wifi_csi_rx_cb, NULL));
    ESP_ERROR_CHECK(esp_wifi_set_csi_config(&csi_config));

    esp_err_t ret = esp_wifi_set_csi(true);

    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "CSI enabled");
    } else {
        ESP_LOGE(TAG, "CSI enable failed: %s", esp_err_to_name(ret));
    }
}

static void event_handler(
    void *arg,
    esp_event_base_t event_base,
    int32_t event_id,
    void *event_data
)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    }

    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGW(TAG, "WiFi disconnected. Reconnecting...");
        esp_wifi_connect();
        xEventGroupClearBits(wifi_event_group, WIFI_CONNECTED_BIT);
    }

    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;

        ESP_LOGI(TAG, "ESP32 IP: " IPSTR, IP2STR(&event->ip_info.ip));

        xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

static void wifi_init_sta(void)
{
    wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();

    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(
        esp_event_handler_instance_register(
            WIFI_EVENT,
            ESP_EVENT_ANY_ID,
            &event_handler,
            NULL,
            NULL
        )
    );

    ESP_ERROR_CHECK(
        esp_event_handler_instance_register(
            IP_EVENT,
            IP_EVENT_STA_GOT_IP,
            &event_handler,
            NULL,
            NULL
        )
    );

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS,
            .threshold.authmode = WIFI_AUTH_OPEN,
        },
    };

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));

    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));

    ESP_LOGI(TAG, "Connecting to WiFi...");
}

void app_main(void)
{
    ESP_ERROR_CHECK(nvs_flash_init());

    wifi_init_sta();

    xEventGroupWaitBits(
        wifi_event_group,
        WIFI_CONNECTED_BIT,
        false,
        true,
        portMAX_DELAY
    );

    vTaskDelay(pdMS_TO_TICKS(1000));

    udp_init();

    vTaskDelay(pdMS_TO_TICKS(500));

    csi_init();

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}