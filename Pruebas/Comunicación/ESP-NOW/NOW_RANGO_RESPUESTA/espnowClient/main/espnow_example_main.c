#include <stdbool.h>
#include <inttypes.h>
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

#define ESPNOW_CHANNEL       1
#define ESPNOW_MAX_NODES     8
#define ESPNOW_QUEUE_LENGTH  64

static const char *TAG = "RANGE_GATEWAY";

typedef struct {
    uint32_t magic;
    uint32_t sequence;
    uint32_t previous_tx_callback_us;
    uint8_t node_id;
    uint8_t previous_mac_status;
    char text[24];
} __attribute__((packed)) test_packet_t;

typedef struct {
    uint8_t source_mac[ESP_NOW_ETH_ALEN];
    test_packet_t packet;
    int rssi;
    int64_t received_at_us;
} received_event_t;

typedef struct {
    bool used;
    uint8_t source_mac[ESP_NOW_ETH_ALEN];
    uint8_t node_id;
    bool have_sequence;
    uint32_t last_sequence;
    uint32_t received_total;
    uint32_t lost_total;
    uint32_t duplicate_total;
    uint32_t restart_or_reorder_total;
    uint32_t rssi_samples;
    int64_t rssi_sum;
    int rssi_min;
    int rssi_max;
} node_metrics_t;

static QueueHandle_t s_receive_queue;
static node_metrics_t s_nodes[ESPNOW_MAX_NODES];
static volatile uint32_t s_callback_frames;
static volatile uint32_t s_callback_bad_len;
static volatile uint32_t s_queue_dropped;

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
    ESP_ERROR_CHECK(esp_wifi_set_protocol(
        WIFI_IF_STA,
        WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G | WIFI_PROTOCOL_11N | WIFI_PROTOCOL_LR));
    ESP_ERROR_CHECK(esp_wifi_set_channel(ESPNOW_CHANNEL, WIFI_SECOND_CHAN_NONE));
}

// Copy data quickly; process packets and metrics outside the Wi-Fi callback.
static void receive_callback(const esp_now_recv_info_t *recv_info,
                             const uint8_t *data, int len)
{
    if (recv_info == NULL || recv_info->src_addr == NULL || data == NULL ||
        s_receive_queue == NULL) {
        return;
    }
    s_callback_frames++;
    if (len != sizeof(test_packet_t)) {
        s_callback_bad_len++;
        ESP_LOGW(TAG, "FRAME ignorado src=" MACSTR " len=%d esperado=%u",
                 MAC2STR(recv_info->src_addr), len, (unsigned)sizeof(test_packet_t));
        return;
    }

    received_event_t event = {0};
    memcpy(event.source_mac, recv_info->src_addr, ESP_NOW_ETH_ALEN);
    memcpy(&event.packet, data, sizeof(event.packet));
    event.packet.text[sizeof(event.packet.text) - 1] = '\0';
    event.rssi = recv_info->rx_ctrl ? recv_info->rx_ctrl->rssi : 0;
    event.received_at_us = esp_timer_get_time();
    if (xQueueSend(s_receive_queue, &event, 0) != pdTRUE) {
        s_queue_dropped++;
    }
}

static node_metrics_t *get_node_metrics(const uint8_t *mac, uint8_t node_id)
{
    node_metrics_t *free_slot = NULL;
    for (size_t i = 0; i < ESPNOW_MAX_NODES; i++) {
        if (s_nodes[i].used &&
            memcmp(s_nodes[i].source_mac, mac, ESP_NOW_ETH_ALEN) == 0) {
            s_nodes[i].node_id = node_id;
            return &s_nodes[i];
        }
        if (!s_nodes[i].used && free_slot == NULL) {
            free_slot = &s_nodes[i];
        }
    }
    if (free_slot != NULL) {
        memset(free_slot, 0, sizeof(*free_slot));
        free_slot->used = true;
        free_slot->node_id = node_id;
        memcpy(free_slot->source_mac, mac, ESP_NOW_ETH_ALEN);
    }
    return free_slot;
}

static void receiver_task(void *arg)
{
    (void)arg;
    received_event_t event;

    while (xQueueReceive(s_receive_queue, &event, portMAX_DELAY) == pdTRUE) {
        if (event.packet.magic != 0x4553504E) {
            ESP_LOGW(TAG, "Paquete ignorado por magic invalido src=" MACSTR,
                     MAC2STR(event.source_mac));
            continue;
        }
        if (event.packet.node_id < 1 || event.packet.node_id > ESPNOW_MAX_NODES) {
            ESP_LOGW(TAG, "Paquete ignorado por node_id=%u src=" MACSTR,
                     event.packet.node_id, MAC2STR(event.source_mac));
            continue;
        }

        node_metrics_t *m = get_node_metrics(event.source_mac, event.packet.node_id);
        if (m == NULL) {
            ESP_LOGE(TAG, "Tabla llena: se recibio un nodo adicional");
            continue;
        }

        if (!m->have_sequence) {
            m->have_sequence = true;
            m->last_sequence = event.packet.sequence;
            m->received_total = 1;
            m->rssi_min = event.rssi;
            m->rssi_max = event.rssi;
        } else {
            const uint32_t delta = event.packet.sequence - m->last_sequence;
            if (delta == 0) {
                m->duplicate_total++;
            } else if (delta < 0x80000000u) {
                if (delta > 1) {
                    m->lost_total += delta - 1;
                }
                m->received_total++;
                m->last_sequence = event.packet.sequence;
            } else {
                // Sender reboot, reordered packet, or stale sequence: do not count
                // the large unsigned delta as thousands of lost packets.
                m->restart_or_reorder_total++;
                m->received_total++;
                m->last_sequence = event.packet.sequence;
            }
        }

        m->rssi_samples++;
        m->rssi_sum += event.rssi;
        if (event.rssi < m->rssi_min) m->rssi_min = event.rssi;
        if (event.rssi > m->rssi_max) m->rssi_max = event.rssi;

        const uint32_t expected = m->received_total + m->lost_total;
        const float loss_pct = expected == 0 ? 0.0f :
                               100.0f * (float)m->lost_total / (float)expected;
        const float rssi_avg = m->rssi_samples == 0 ? 0.0f :
                               (float)m->rssi_sum / (float)m->rssi_samples;

        ESP_LOGI(TAG, "ESPNOW_RX role=receiver src=" MACSTR
                 " seq=%" PRIu32 " rssi_dbm=%d rx_total=%" PRIu32
                 " lost_total=%" PRIu32 " loss_pct=%.2f"
                 " queue_dropped=%" PRIu32 " callback_frames=%" PRIu32
                 " bad_len=%" PRIu32 " payload_bytes=%u esp_time_us=%" PRId64
                 " rssi_min_dbm=%d rssi_max_dbm=%d rssi_avg_dbm=%.1f"
                 " text=node_id=%u prev_mac_status=%s prev_tx_callback_us=%" PRIu32
                 " duplicate_total=%" PRIu32 " restart_or_reorder_total=%" PRIu32
                 " value=%s",
                 MAC2STR(event.source_mac), event.packet.sequence, event.rssi,
                 m->received_total, m->lost_total, loss_pct,
                 (uint32_t)s_queue_dropped, (uint32_t)s_callback_frames,
                 (uint32_t)s_callback_bad_len, (unsigned)sizeof(event.packet),
                 event.received_at_us, m->rssi_min, m->rssi_max, rssi_avg,
                 event.packet.node_id, mac_status_text(event.packet.previous_mac_status),
                 event.packet.previous_tx_callback_us, m->duplicate_total,
                 m->restart_or_reorder_total, event.packet.text);
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

    s_receive_queue = xQueueCreate(ESPNOW_QUEUE_LENGTH, sizeof(received_event_t));
    if (s_receive_queue == NULL) {
        ESP_LOGE(TAG, "No se pudo crear la cola de recepcion");
        return;
    }
    ESP_ERROR_CHECK(esp_now_register_recv_cb(receive_callback));

    uint8_t mac[ESP_NOW_ETH_ALEN];
    ESP_ERROR_CHECK(esp_wifi_get_mac(WIFI_IF_STA, mac));
    ESP_LOGI(TAG, "FIRMWARE_NOW_RANGE_LR500K_RX_V1 role=gateway"
             " MAC=" MACSTR " channel=%d rate=LR500K nodes=%d queue=%d",
             MAC2STR(mac), ESPNOW_CHANNEL, ESPNOW_MAX_NODES, ESPNOW_QUEUE_LENGTH);

    if (xTaskCreate(receiver_task, "receiver_task", 6144, NULL, 4, NULL) != pdPASS) {
        ESP_LOGE(TAG, "No se pudo crear receiver_task");
    }
}
