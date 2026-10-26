#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_timer.h"
#include "esp_now.h"

#define ESPNOW_CHANNEL          1
#define ESPNOW_NODE_ID          1   // Use 1..8; set a different ID on each C3.
#define ESPNOW_NODE_COUNT       8
#define ESPNOW_TX_INTERVAL_MS   500
#define ESPNOW_NODE_SLOT_MS     40

// MAC logged by the current DevKit receiver during the captured test.
// If the gateway board changes, replace this with its STA MAC from boot logs.
static const uint8_t s_gateway_mac[ESP_NOW_ETH_ALEN] = {
    0xE0, 0x8C, 0xFE, 0x5D, 0x04, 0xEC
};

static const char *TAG = "RANGE_SENDER";

typedef struct {
    uint32_t magic;
    uint32_t sequence;
    uint32_t previous_tx_callback_us;
    uint8_t node_id;
    uint8_t previous_mac_status; // 0=not available, 1=success, 2=fail, 3=API error
    char text[24];
} __attribute__((packed)) test_packet_t;

static volatile uint32_t s_sequence;
static volatile uint32_t s_previous_callback_us;
static volatile uint8_t s_previous_status;
static volatile int64_t s_send_started_us;
static volatile uint32_t s_sending_sequence;

static const char *mac_status_text(uint8_t status)
{
    switch (status) {
    case 1: return "ok";
    case 2: return "fail";
    case 3: return "api_error";
    default: return "na";
    }
}

static void wifi_init(void)
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));

    // LR is enabled at both ends; the peer rate below fixes ESP-NOW at 500 Kbps.
    ESP_ERROR_CHECK(esp_wifi_set_protocol(
        WIFI_IF_STA,
        WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G | WIFI_PROTOCOL_11N | WIFI_PROTOCOL_LR));
    ESP_ERROR_CHECK(esp_wifi_set_channel(ESPNOW_CHANNEL, WIFI_SECOND_CHAN_NONE));

    // Do not apply the former runtime 8 dBm cap. sdkconfig keeps the normal
    // ESP-IDF maximum setting; the driver/regulatory configuration remains authoritative.
    int8_t max_tx_qdbm = 0;
    ESP_ERROR_CHECK(esp_wifi_get_max_tx_power(&max_tx_qdbm));
    ESP_LOGI(TAG, "TX power cap reported by driver=%d quarter-dBm", max_tx_qdbm);
}

static void send_callback(const esp_now_send_info_t *tx_info,
                          esp_now_send_status_t status)
{
    if (tx_info == NULL) {
        return;
    }

    const int64_t callback_us = esp_timer_get_time();
    const uint32_t elapsed_us = (uint32_t)(callback_us - s_send_started_us);
    const uint8_t status_code = (status == ESP_NOW_SEND_SUCCESS) ? 1 : 2;
    s_previous_callback_us = elapsed_us;
    s_previous_status = status_code;

    ESP_LOGI(TAG,
             "ESPNOW_TX_RESULT role=sender node_id=%u seq=%" PRIu32
             " mac_status=%s callback_us=%" PRIu32 " dst=" MACSTR,
             (unsigned)ESPNOW_NODE_ID, (uint32_t)s_sending_sequence,
             mac_status_text(status_code), elapsed_us, MAC2STR(tx_info->des_addr));
}

static void sender_task(void *arg)
{
    (void)arg;
    if (ESPNOW_NODE_ID < 1 || ESPNOW_NODE_ID > ESPNOW_NODE_COUNT) {
        ESP_LOGE(TAG, "ESPNOW_NODE_ID debe estar entre 1 y %d", ESPNOW_NODE_COUNT);
        vTaskDelete(NULL);
        return;
    }

    // Separate the eight nodes in time to reduce collisions after simultaneous boot.
    vTaskDelay(pdMS_TO_TICKS((ESPNOW_NODE_ID - 1) * ESPNOW_NODE_SLOT_MS));
    TickType_t last_wake = xTaskGetTickCount();

    while (true) {
        test_packet_t packet = {0};
        packet.magic = 0x4553504E;
        packet.sequence = s_sequence++;
        packet.previous_tx_callback_us = s_previous_callback_us;
        packet.node_id = ESPNOW_NODE_ID;
        packet.previous_mac_status = s_previous_status;
        snprintf(packet.text, sizeof(packet.text), "range-node-%u",
                 (unsigned)ESPNOW_NODE_ID);

        s_sending_sequence = packet.sequence;
        s_send_started_us = esp_timer_get_time();
        esp_err_t err = esp_now_send(s_gateway_mac,
                                     (const uint8_t *)&packet, sizeof(packet));
        if (err != ESP_OK) {
            s_previous_status = 3;
            s_previous_callback_us = 0;
            ESP_LOGE(TAG, "ESPNOW_TX_ERROR node_id=%u seq=%" PRIu32 " err=%s",
                     (unsigned)ESPNOW_NODE_ID, packet.sequence, esp_err_to_name(err));
        } else {
            ESP_LOGI(TAG, "ESPNOW_TX node_id=%u seq=%" PRIu32
                     " bytes=%u interval_ms=%u",
                     (unsigned)ESPNOW_NODE_ID, packet.sequence,
                     (unsigned)sizeof(packet), (unsigned)ESPNOW_TX_INTERVAL_MS);
        }
        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(ESPNOW_TX_INTERVAL_MS));
    }
}

void app_main(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    wifi_init();
    ESP_ERROR_CHECK(esp_now_init());
    ESP_ERROR_CHECK(esp_now_register_send_cb(send_callback));

    esp_now_peer_info_t peer = {0};
    memcpy(peer.peer_addr, s_gateway_mac, ESP_NOW_ETH_ALEN);
    peer.channel = ESPNOW_CHANNEL;
    peer.ifidx = WIFI_IF_STA;
    peer.encrypt = false;
    ESP_ERROR_CHECK(esp_now_add_peer(&peer));

    esp_now_rate_config_t rate = {
        .phymode = WIFI_PHY_MODE_LR,
        .rate = WIFI_PHY_RATE_LORA_500K,
        .ersu = false,
        .dcm = false,
    };
    ESP_ERROR_CHECK(esp_now_set_peer_rate_config(s_gateway_mac, &rate));

    uint8_t mac[ESP_NOW_ETH_ALEN];
    ESP_ERROR_CHECK(esp_wifi_get_mac(WIFI_IF_STA, mac));
    ESP_LOGI(TAG, "FIRMWARE_NOW_RANGE_LR500K_V1 role=sender node_id=%u"
             " MAC=" MACSTR " gateway=" MACSTR " channel=%d rate=LR500K"
             " interval_ms=%u slot_ms=%u",
             (unsigned)ESPNOW_NODE_ID, MAC2STR(mac), MAC2STR(s_gateway_mac),
             ESPNOW_CHANNEL, (unsigned)ESPNOW_TX_INTERVAL_MS,
             (unsigned)ESPNOW_NODE_SLOT_MS);

    if (xTaskCreate(sender_task, "sender_task", 3072, NULL, 4, NULL) != pdPASS) {
        ESP_LOGE(TAG, "No se pudo crear sender_task");
    }
}
