#include <stdbool.h>
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

#define ESPNOW_CHANNEL 1

static const char *TAG = "OFFICIAL_RECEIVER_TEST";

typedef struct {
    uint32_t magic;
    uint32_t sequence;
    char text[24];
} test_packet_t;

typedef struct {
    uint8_t source_mac[ESP_NOW_ETH_ALEN];
    test_packet_t packet;
    int rssi;
    int64_t received_at_us;
} received_event_t;

static QueueHandle_t s_receive_queue;
static volatile uint32_t s_callback_frames;
static volatile uint32_t s_callback_bad_len;
static volatile uint32_t s_queue_dropped;

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
    // Mantener el mismo PHY basico que el sender para la prueba cruzada.
    ESP_ERROR_CHECK(esp_wifi_set_protocol(WIFI_IF_STA, WIFI_PROTOCOL_11B));
    ESP_ERROR_CHECK(esp_wifi_set_channel(ESPNOW_CHANNEL, WIFI_SECOND_CHAN_NONE));
}

// El callback de ESP-NOW debe copiar y salir rapido.
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

static void receiver_task(void *arg)
{
    (void)arg;
    received_event_t event;
    uint32_t received = 0;
    uint32_t last_sequence = 0;
    uint32_t lost = 0;
    int64_t rssi_sum = 0;
    int rssi_min = 0;
    int rssi_max = 0;
    bool first = true;

    while (xQueueReceive(s_receive_queue, &event, portMAX_DELAY) == pdTRUE) {
        if (event.packet.magic != 0x4553504E) {
            ESP_LOGW(TAG, "Paquete desconocido recibido");
            continue;
        }
        if (!first) {
            const uint32_t delta = event.packet.sequence - last_sequence;
            if (delta > 1 && delta < 0x80000000u) {
                lost += delta - 1;
            }
        } else {
            rssi_min = event.rssi;
            rssi_max = event.rssi;
        }
        first = false;
        last_sequence = event.packet.sequence;
        received++;
        rssi_sum += event.rssi;
        if (event.rssi < rssi_min) rssi_min = event.rssi;
        if (event.rssi > rssi_max) rssi_max = event.rssi;
        uint32_t expected = received + lost;
        float loss_pct = expected == 0 ? 0.0f : 100.0f * lost / expected;
        float rssi_avg = (float)rssi_sum / received;
        ESP_LOGI(TAG, "ESPNOW_RX role=receiver src=" MACSTR
                 " seq=%lu rssi_dbm=%d rx_total=%lu lost_total=%lu"
                 " loss_pct=%.2f queue_dropped=%lu callback_frames=%lu"
                 " bad_len=%lu payload_bytes=%u"
                 " esp_time_us=%lld rssi_min_dbm=%d rssi_max_dbm=%d"
                 " rssi_avg_dbm=%.1f text=%s",
                 MAC2STR(event.source_mac),
                 (unsigned long)event.packet.sequence,
                 event.rssi, (unsigned long)received, (unsigned long)lost,
                 loss_pct, (unsigned long)s_queue_dropped,
                 (unsigned long)s_callback_frames, (unsigned long)s_callback_bad_len,
                 (unsigned)sizeof(event.packet), (long long)event.received_at_us,
                 rssi_min, rssi_max, rssi_avg, event.packet.text);
        ESP_LOGI(TAG, "RESULT rx=%lu lost=%lu loss_pct=%.2f",
                 (unsigned long)received, (unsigned long)lost, loss_pct);
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

    s_receive_queue = xQueueCreate(8, sizeof(received_event_t));
    if (s_receive_queue == NULL) {
        ESP_LOGE(TAG, "No se pudo crear la cola de recepcion");
        return;
    }
    ESP_ERROR_CHECK(esp_now_register_recv_cb(receive_callback));

    uint8_t mac[ESP_NOW_ETH_ALEN];
    ESP_ERROR_CHECK(esp_wifi_get_mac(WIFI_IF_STA, mac));
    ESP_LOGI(TAG, "FIRMWARE_RECEIVER_METRICS_V4 LISTO MAC=" MACSTR " canal=%d esperando unicast/broadcast bytes=%u",
             MAC2STR(mac), ESPNOW_CHANNEL, (unsigned)sizeof(test_packet_t));
    if (xTaskCreate(receiver_task, "receiver_task", 4096, NULL, 4, NULL) != pdPASS) {
        ESP_LOGE(TAG, "No se pudo crear receiver_task");
    }
}
