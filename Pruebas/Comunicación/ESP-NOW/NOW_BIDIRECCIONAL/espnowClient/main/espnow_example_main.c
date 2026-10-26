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
#define ESPNOW_DATA_MAGIC    0x4553504E
#define ESPNOW_ACK_MAGIC     0x41434B4E
#define ACK_REPORT_RECEIVED  1
#define ACK_REPORT_TIMEOUT   2
#define ACK_REPORT_TX_ERROR  3

static const char *TAG = "BIDIR_GATE";

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

// Formato exacto del sender V1: magic + sequence + text[24] = 32 bytes.
typedef struct __attribute__((packed)) {
    uint32_t magic;
    uint32_t sequence;
    char text[24];
} v1_broadcast_packet_t;

typedef struct __attribute__((packed)) {
    uint32_t magic;
    uint32_t sequence;
    uint8_t node_id;
    int8_t gateway_rx_rssi_dbm;
} ack_packet_t;

typedef struct {
    uint8_t source_mac[ESP_NOW_ETH_ALEN];
    data_packet_t packet;
    uint16_t payload_bytes;
    bool legacy_v1_format;
    int rssi_dbm;
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
    uint32_t ack_confirmed_total;
    uint32_t ack_timeout_total;
    uint32_t ack_tx_error_total;
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

static const char *ack_report_name(uint8_t state)
{
    switch (state) {
    case ACK_REPORT_RECEIVED: return "received";
    case ACK_REPORT_TIMEOUT: return "timeout";
    case ACK_REPORT_TX_ERROR: return "tx_error";
    default: return "none";
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
        WIFI_IF_STA, WIFI_PROTOCOL_11B));
    ESP_ERROR_CHECK(esp_wifi_set_channel(ESPNOW_CHANNEL, WIFI_SECOND_CHAN_NONE));
}

// Copy the frame into a queue. Do not send a response inside the Wi-Fi callback.
static void receive_callback(const esp_now_recv_info_t *info,
                             const uint8_t *data, int len)
{
    if (info == NULL || info->src_addr == NULL || data == NULL ||
        s_receive_queue == NULL) return;
    s_callback_frames++;
    if (len != sizeof(data_packet_t) && len != sizeof(v1_broadcast_packet_t)) {
        s_callback_bad_len++;
        uint32_t prefix_magic = 0;
        uint32_t prefix_sequence = 0;
        if (len >= (int)(2 * sizeof(uint32_t))) {
            memcpy(&prefix_magic, data, sizeof(prefix_magic));
            memcpy(&prefix_sequence, data + sizeof(prefix_magic), sizeof(prefix_sequence));
        }
        ESP_LOGW(TAG, "FRAME_SIZE_MISMATCH src=" MACSTR
                 " len=%d esperado=%u magic=0x%08" PRIx32 " seq=%" PRIu32,
                 MAC2STR(info->src_addr), len, (unsigned)sizeof(data_packet_t),
                 prefix_magic, prefix_sequence);
        if (len > 0) ESP_LOG_BUFFER_HEX_LEVEL(TAG, data, len < 16 ? len : 16, ESP_LOG_WARN);
        return;
    }
    received_event_t event = {0};
    memcpy(event.source_mac, info->src_addr, ESP_NOW_ETH_ALEN);
    event.payload_bytes = (uint16_t)len;
    if (len == sizeof(data_packet_t)) {
        memcpy(&event.packet, data, sizeof(event.packet));
    } else {
        v1_broadcast_packet_t legacy = {0};
        memcpy(&legacy, data, sizeof(legacy));
        if (legacy.magic != ESPNOW_DATA_MAGIC) {
            ESP_LOGW(TAG, "LEGACY_FRAME_INVALID src=" MACSTR " magic=0x%08" PRIx32,
                     MAC2STR(info->src_addr), legacy.magic);
            return;
        }
        event.packet.magic = legacy.magic;
        event.packet.sequence = legacy.sequence;
        event.packet.node_id = 1; // Comparación controlada con un solo sender V1.
        memcpy(event.packet.text, legacy.text, sizeof(legacy.text));
        event.legacy_v1_format = true;
    }
    event.packet.text[sizeof(event.packet.text) - 1] = '\0';
    event.rssi_dbm = info->rx_ctrl ? info->rx_ctrl->rssi : 0;
    event.received_at_us = esp_timer_get_time();
    if (xQueueSend(s_receive_queue, &event, 0) != pdTRUE) s_queue_dropped++;
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
        if (!s_nodes[i].used && free_slot == NULL) free_slot = &s_nodes[i];
    }
    if (free_slot != NULL) {
        memset(free_slot, 0, sizeof(*free_slot));
        free_slot->used = true;
        free_slot->node_id = node_id;
        memcpy(free_slot->source_mac, mac, ESP_NOW_ETH_ALEN);
    }
    return free_slot;
}

static void ack_send_callback(const esp_now_send_info_t *info,
                              esp_now_send_status_t status)
{
    if (info == NULL) return;
    ESP_LOGI(TAG, "ESPNOW_ACK_MAC_TX dst=" MACSTR " status=%s",
             MAC2STR(info->des_addr),
             status == ESP_NOW_SEND_SUCCESS ? "ok" : "fail");
}

static bool ensure_peer_for_ack(const uint8_t *mac)
{
    if (esp_now_is_peer_exist(mac)) return true;
    esp_now_peer_info_t peer = {0};
    memcpy(peer.peer_addr, mac, ESP_NOW_ETH_ALEN);
    peer.channel = ESPNOW_CHANNEL;
    peer.ifidx = WIFI_IF_STA;
    peer.encrypt = false;
    const esp_err_t add_err = esp_now_add_peer(&peer);
    if (add_err != ESP_OK) {
        ESP_LOGE(TAG, "PEER_ADD_FAIL peer=" MACSTR " err=%s",
                 MAC2STR(mac), esp_err_to_name(add_err));
        return false;
    }
    return true;
}

static void process_data(received_event_t *event)
{
    if (event->packet.magic != ESPNOW_DATA_MAGIC ||
        event->packet.node_id < 1 || event->packet.node_id > ESPNOW_MAX_NODES) {
        ESP_LOGW(TAG, "DATA inválido src=" MACSTR " node_id=%u",
                 MAC2STR(event->source_mac), event->packet.node_id);
        return;
    }
    node_metrics_t *m = get_node_metrics(event->source_mac, event->packet.node_id);
    if (m == NULL) {
        ESP_LOGE(TAG, "Tabla llena; nodo adicional src=" MACSTR,
                 MAC2STR(event->source_mac));
        return;
    }

    if (!m->have_sequence) {
        m->have_sequence = true;
        m->last_sequence = event->packet.sequence;
        m->received_total = 1;
        m->rssi_min = event->rssi_dbm;
        m->rssi_max = event->rssi_dbm;
    } else {
        const uint32_t delta = event->packet.sequence - m->last_sequence;
        if (delta == 0) {
            m->duplicate_total++;
        } else if (delta < 0x80000000u) {
            if (delta > 1) m->lost_total += delta - 1;
            m->received_total++;
            m->last_sequence = event->packet.sequence;
        } else {
            m->restart_or_reorder_total++;
            m->received_total++;
            m->last_sequence = event->packet.sequence;
        }
    }
    m->rssi_samples++;
    m->rssi_sum += event->rssi_dbm;
    if (event->rssi_dbm < m->rssi_min) m->rssi_min = event->rssi_dbm;
    if (event->rssi_dbm > m->rssi_max) m->rssi_max = event->rssi_dbm;

    if (event->packet.ack_report_state == ACK_REPORT_RECEIVED) m->ack_confirmed_total++;
    else if (event->packet.ack_report_state == ACK_REPORT_TIMEOUT) m->ack_timeout_total++;
    else if (event->packet.ack_report_state == ACK_REPORT_TX_ERROR) m->ack_tx_error_total++;

    const uint32_t expected = m->received_total + m->lost_total;
    const float loss_pct = expected ? 100.0f * (float)m->lost_total / (float)expected : 0;
    const float rssi_avg = m->rssi_samples ? (float)m->rssi_sum / (float)m->rssi_samples : 0;
    ESP_LOGI(TAG, "ESPNOW_RX role=receiver src=" MACSTR
             " seq=%" PRIu32 " rssi_dbm=%d rx_total=%" PRIu32
             " lost_total=%" PRIu32 " loss_pct=%.2f queue_dropped=%" PRIu32
             " callback_frames=%" PRIu32 " bad_len=%" PRIu32
             " payload_bytes=%u payload_format=%s esp_time_us=%" PRId64
             " rssi_min_dbm=%d rssi_max_dbm=%d rssi_avg_dbm=%.1f"
             " text=node_id=%u ack_state=%s ack_seq=%" PRIu32
             " ack_rtt_us=%" PRIu32 " ack_rssi_dbm=%d gateway_rx_rssi_dbm=%d"
             " ack_confirmed_total=%" PRIu32 " ack_timeout_total=%" PRIu32
             " ack_tx_error_total=%" PRIu32 " duplicate_total=%" PRIu32
             " restart_or_reorder_total=%" PRIu32 " value=%s",
             MAC2STR(event->source_mac), event->packet.sequence, event->rssi_dbm,
             m->received_total, m->lost_total, loss_pct,
             (uint32_t)s_queue_dropped, (uint32_t)s_callback_frames,
             (uint32_t)s_callback_bad_len, (unsigned)event->payload_bytes,
             event->legacy_v1_format ? "v1_32B" : "bidir_44B",
             event->received_at_us, m->rssi_min, m->rssi_max, rssi_avg,
             event->packet.node_id, ack_report_name(event->packet.ack_report_state),
             event->packet.confirmed_ack_sequence, event->packet.confirmed_ack_rtt_us,
             event->packet.ack_rssi_dbm, event->packet.gateway_rx_rssi_dbm,
             m->ack_confirmed_total,
             m->ack_timeout_total, m->ack_tx_error_total, m->duplicate_total,
             m->restart_or_reorder_total, event->packet.text);

    if (!ensure_peer_for_ack(event->source_mac)) return;
    const ack_packet_t ack = {
        .magic = ESPNOW_ACK_MAGIC,
        .sequence = event->packet.sequence,
        .node_id = event->packet.node_id,
        .gateway_rx_rssi_dbm = (int8_t)event->rssi_dbm,
    };
    const esp_err_t err = esp_now_send(event->source_mac,
                                       (const uint8_t *)&ack, sizeof(ack));
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "ESPNOW_APP_ACK_TX dst=" MACSTR " node_id=%u seq=%" PRIu32
                 " gateway_rx_rssi_dbm=%d status=queued",
                 MAC2STR(event->source_mac), event->packet.node_id,
                 event->packet.sequence, event->rssi_dbm);
    } else {
        ESP_LOGE(TAG, "ESPNOW_APP_ACK_TX dst=" MACSTR " node_id=%u seq=%" PRIu32
                 " status=error err=%s",
                 MAC2STR(event->source_mac), event->packet.node_id,
                 event->packet.sequence, esp_err_to_name(err));
    }
}

static void receiver_task(void *arg)
{
    (void)arg;
    received_event_t event;
    while (xQueueReceive(s_receive_queue, &event, portMAX_DELAY) == pdTRUE)
        process_data(&event);
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
    ESP_ERROR_CHECK(esp_now_register_send_cb(ack_send_callback));
    s_receive_queue = xQueueCreate(ESPNOW_QUEUE_LENGTH, sizeof(received_event_t));
    if (s_receive_queue == NULL) {
        ESP_LOGE(TAG, "No se pudo crear la cola de recepción");
        return;
    }
    ESP_ERROR_CHECK(esp_now_register_recv_cb(receive_callback));

    uint8_t mac[ESP_NOW_ETH_ALEN];
    ESP_ERROR_CHECK(esp_wifi_get_mac(WIFI_IF_STA, mac));
    ESP_LOGI(TAG, "FIRMWARE_NOW_BIDIR_11B_GATE_DUAL_V2 role=gateway MAC=" MACSTR
             " channel=%d rate=11B_DEFAULT accepts_bytes=%u,%u ack_bytes=%u nodes=%d interval_ms=1000",
             MAC2STR(mac), ESPNOW_CHANNEL, (unsigned)sizeof(data_packet_t),
             (unsigned)sizeof(v1_broadcast_packet_t), (unsigned)sizeof(ack_packet_t),
             ESPNOW_MAX_NODES);

    if (xTaskCreate(receiver_task, "receiver_task", 6144, NULL, 4, NULL) != pdPASS)
        ESP_LOGE(TAG, "No se pudo crear receiver_task");
}
