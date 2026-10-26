#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
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
#define ESPNOW_TX_POWER_QDBM    34  // Perfil de la V1 que sí recibió: 8.5 dBm.
#define ESPNOW_NODE_ID          1   // Asigna un ID único 1..8 a cada C3.
#define ESPNOW_NODE_COUNT       8
#define ESPNOW_TX_INTERVAL_MS   1000 // Conserva el intervalo anterior: 1 paquete/s/nodo.
#define ESPNOW_NODE_SLOT_MS     40
#define ESPNOW_ACK_TIMEOUT_MS   300
#define ESPNOW_DATA_MAGIC       0x4553504E
#define ESPNOW_ACK_MAGIC        0x41434B4E
#define ACK_REPORT_NONE         0
#define ACK_REPORT_RECEIVED     1
#define ACK_REPORT_TIMEOUT      2
#define ACK_REPORT_TX_ERROR     3

// MAC STA del gateway DevKit indicada en los logs de las pruebas.
static const uint8_t s_gateway_mac[ESP_NOW_ETH_ALEN] = {
    0xE0, 0x8C, 0xFE, 0x5D, 0x04, 0xEC
};
static const uint8_t s_broadcast_mac[ESP_NOW_ETH_ALEN] = {
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF
};

static const char *TAG = "BIDIR_C3";

typedef struct __attribute__((packed)) {
    uint32_t magic;
    uint32_t sequence;
    uint32_t confirmed_ack_sequence;
    uint32_t confirmed_ack_rtt_us;
    uint8_t node_id;
    uint8_t ack_report_state;
    int8_t ack_rssi_dbm;
    int8_t gateway_rx_rssi_dbm;
    char text[24];
} data_packet_t;

typedef struct __attribute__((packed)) {
    uint32_t magic;
    uint32_t sequence;
    uint8_t node_id;
    int8_t gateway_rx_rssi_dbm;
} ack_packet_t;

typedef struct {
    ack_packet_t packet;
    int rssi_dbm;
    int64_t received_at_us;
} ack_event_t;

static QueueHandle_t s_ack_queue;
static volatile uint32_t s_sequence;
static volatile uint32_t s_sending_sequence;

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
    ESP_ERROR_CHECK(esp_wifi_set_max_tx_power(ESPNOW_TX_POWER_QDBM));
    ESP_ERROR_CHECK(esp_wifi_set_protocol(
        WIFI_IF_STA, WIFI_PROTOCOL_11B));
    ESP_ERROR_CHECK(esp_wifi_set_channel(ESPNOW_CHANNEL, WIFI_SECOND_CHAN_NONE));

    int8_t max_tx_qdbm = 0;
    ESP_ERROR_CHECK(esp_wifi_get_max_tx_power(&max_tx_qdbm));
    ESP_LOGI(TAG, "Driver TX power cap=%d quarter-dBm", max_tx_qdbm);
}

static void send_callback(const esp_now_send_info_t *tx_info,
                          esp_now_send_status_t status)
{
    if (tx_info == NULL) return;
    ESP_LOGI(TAG, "ESPNOW_DATA_TX node_id=%u seq=%" PRIu32
             " mac_status=%s dst=" MACSTR,
             (unsigned)ESPNOW_NODE_ID, (uint32_t)s_sending_sequence,
             status == ESP_NOW_SEND_SUCCESS ? "ok" : "fail",
             MAC2STR(tx_info->des_addr));
}

// Keep the Wi-Fi callback short; validate and queue the application ACK.
static void receive_ack_callback(const esp_now_recv_info_t *info,
                                 const uint8_t *data, int len)
{
    if (info == NULL || info->src_addr == NULL || data == NULL ||
        len != sizeof(ack_packet_t) || s_ack_queue == NULL) return;

    ack_event_t event = {0};
    memcpy(&event.packet, data, sizeof(event.packet));
    if (event.packet.magic != ESPNOW_ACK_MAGIC ||
        event.packet.node_id != ESPNOW_NODE_ID ||
        memcmp(info->src_addr, s_gateway_mac, ESP_NOW_ETH_ALEN) != 0) return;
    event.rssi_dbm = info->rx_ctrl ? info->rx_ctrl->rssi : 0;
    event.received_at_us = esp_timer_get_time();
    (void)xQueueSend(s_ack_queue, &event, 0);
}

static void sender_task(void *arg)
{
    (void)arg;
    if (ESPNOW_NODE_ID < 1 || ESPNOW_NODE_ID > ESPNOW_NODE_COUNT) {
        ESP_LOGE(TAG, "ESPNOW_NODE_ID debe estar entre 1 y %d", ESPNOW_NODE_COUNT);
        vTaskDelete(NULL);
        return;
    }

    vTaskDelay(pdMS_TO_TICKS((ESPNOW_NODE_ID - 1) * ESPNOW_NODE_SLOT_MS));
    TickType_t last_wake = xTaskGetTickCount();
    uint8_t report_state = ACK_REPORT_NONE;
    uint32_t confirmed_ack_sequence = 0;
    uint32_t confirmed_ack_rtt_us = 0;
    int8_t confirmed_ack_rssi_dbm = 0;
    int8_t gateway_rx_rssi_dbm = 0;

    while (true) {
        data_packet_t packet = {0};
        packet.magic = ESPNOW_DATA_MAGIC;
        packet.sequence = s_sequence++;
        packet.confirmed_ack_sequence = confirmed_ack_sequence;
        packet.confirmed_ack_rtt_us = confirmed_ack_rtt_us;
        packet.node_id = ESPNOW_NODE_ID;
        packet.ack_report_state = report_state;
        packet.ack_rssi_dbm = confirmed_ack_rssi_dbm;
        packet.gateway_rx_rssi_dbm = gateway_rx_rssi_dbm;
        snprintf(packet.text, sizeof(packet.text), "range-node-%u",
                 (unsigned)ESPNOW_NODE_ID);

        (void)xQueueReset(s_ack_queue);
        s_sending_sequence = packet.sequence;
        const int64_t send_started_us = esp_timer_get_time();
        const esp_err_t err = esp_now_send(s_broadcast_mac,
                                            (const uint8_t *)&packet, sizeof(packet));
        report_state = ACK_REPORT_TX_ERROR;
        confirmed_ack_sequence = packet.sequence;
        confirmed_ack_rtt_us = 0;
        confirmed_ack_rssi_dbm = 0;
        gateway_rx_rssi_dbm = 0;

        if (err != ESP_OK) {
            ESP_LOGE(TAG, "ESPNOW_DATA_ERROR node_id=%u seq=%" PRIu32 " err=%s",
                     (unsigned)ESPNOW_NODE_ID, packet.sequence, esp_err_to_name(err));
        } else {
            ESP_LOGI(TAG, "ESPNOW_DATA_TX_REQUEST node_id=%u seq=%" PRIu32
                     " dst=" MACSTR " bytes=%u interval_ms=%u",
                     (unsigned)ESPNOW_NODE_ID, packet.sequence,
                     MAC2STR(s_broadcast_mac),
                     (unsigned)sizeof(packet), (unsigned)ESPNOW_TX_INTERVAL_MS);

            const TickType_t deadline = xTaskGetTickCount() +
                                        pdMS_TO_TICKS(ESPNOW_ACK_TIMEOUT_MS);
            ack_event_t ack_event;
            bool got_matching_ack = false;
            while (xTaskGetTickCount() < deadline) {
                const TickType_t remaining = deadline - xTaskGetTickCount();
                if (xQueueReceive(s_ack_queue, &ack_event, remaining) != pdTRUE) break;
                if (ack_event.packet.sequence == packet.sequence) {
                    got_matching_ack = true;
                    const int64_t rtt = ack_event.received_at_us - send_started_us;
                    report_state = ACK_REPORT_RECEIVED;
                    confirmed_ack_rtt_us = (uint32_t)rtt;
                    confirmed_ack_rssi_dbm = (int8_t)ack_event.rssi_dbm;
                    gateway_rx_rssi_dbm = ack_event.packet.gateway_rx_rssi_dbm;
                    ESP_LOGI(TAG, "ESPNOW_APP_ACK node_id=%u seq=%" PRIu32
                             " result=received rtt_us=%" PRIu32 " ack_rssi_dbm=%d",
                             (unsigned)ESPNOW_NODE_ID, packet.sequence,
                             confirmed_ack_rtt_us, ack_event.rssi_dbm);
                    break;
                }
                ESP_LOGW(TAG, "ACK obsoleto seq=%" PRIu32 " esperado=%" PRIu32,
                         ack_event.packet.sequence, packet.sequence);
            }
            if (!got_matching_ack) {
                report_state = ACK_REPORT_TIMEOUT;
                ESP_LOGW(TAG, "ESPNOW_APP_ACK node_id=%u seq=%" PRIu32
                         " result=timeout timeout_ms=%u",
                         (unsigned)ESPNOW_NODE_ID, packet.sequence,
                         (unsigned)ESPNOW_ACK_TIMEOUT_MS);
            }
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
    s_ack_queue = xQueueCreate(4, sizeof(ack_event_t));
    if (s_ack_queue == NULL) {
        ESP_LOGE(TAG, "No se pudo crear la cola de ACK");
        return;
    }
    ESP_ERROR_CHECK(esp_now_register_send_cb(send_callback));
    ESP_ERROR_CHECK(esp_now_register_recv_cb(receive_ack_callback));

    // Broadcast requiere estar registrado como peer para transmitir.
    // El ACK unicast entrante no requiere registrar el gateway en este C3.
    esp_now_peer_info_t peer = {0};
    memcpy(peer.peer_addr, s_broadcast_mac, ESP_NOW_ETH_ALEN);
    peer.channel = ESPNOW_CHANNEL;
    peer.ifidx = WIFI_IF_STA;
    peer.encrypt = false;
    ESP_ERROR_CHECK(esp_now_add_peer(&peer));
    uint8_t mac[ESP_NOW_ETH_ALEN];
    ESP_ERROR_CHECK(esp_wifi_get_mac(WIFI_IF_STA, mac));
    ESP_LOGI(TAG, "FIRMWARE_NOW_BIDIR_11B_BCAST_TX8D5_C3_V2 node_id=%u MAC=" MACSTR
             " gateway=" MACSTR " data_dst=" MACSTR
             " channel=%d rate=11B_DEFAULT tx_power_qdbm=%d data_bytes=%u ack_bytes=%u"
             " interval_ms=%u ack_timeout_ms=%u",
             (unsigned)ESPNOW_NODE_ID, MAC2STR(mac), MAC2STR(s_gateway_mac),
             MAC2STR(s_broadcast_mac),
             ESPNOW_CHANNEL, ESPNOW_TX_POWER_QDBM, (unsigned)sizeof(data_packet_t),
             (unsigned)sizeof(ack_packet_t), (unsigned)ESPNOW_TX_INTERVAL_MS,
             (unsigned)ESPNOW_ACK_TIMEOUT_MS);

    if (xTaskCreate(sender_task, "sender_task", 4096, NULL, 4, NULL) != pdPASS)
        ESP_LOGE(TAG, "No se pudo crear sender_task");
}
