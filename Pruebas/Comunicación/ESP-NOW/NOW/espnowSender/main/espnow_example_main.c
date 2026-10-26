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
#include "esp_now.h"

#define ESPNOW_CHANNEL 1
#define ESPNOW_TX_POWER_QDBM 34  // 34 cuartos de dBm = 8.5 dBm

static const char *TAG = "OFFICIAL_SENDER_TEST";
// Prueba de broadcast: no depende de conocer la MAC del receptor.
// Si el DevKit recibe, despues se puede cambiar a unicast.
static const uint8_t s_broadcast_mac[ESP_NOW_ETH_ALEN] = {
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF
};

typedef struct {
    uint32_t magic;
    uint32_t sequence;
    char text[24];
} test_packet_t;

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
    // Prueba de diagnóstico para C3 Super Mini: algunas placas presentan
    // transmisión ESP-NOW inestable con potencia alta por su antena/RF.
    ESP_ERROR_CHECK(esp_wifi_set_max_tx_power(ESPNOW_TX_POWER_QDBM));
    // Forzar la tasa PHY basica compatible con ESP-NOW entre ESP32 y ESP32-C3.
    ESP_ERROR_CHECK(esp_wifi_set_protocol(WIFI_IF_STA, WIFI_PROTOCOL_11B));
    ESP_ERROR_CHECK(esp_wifi_set_channel(ESPNOW_CHANNEL, WIFI_SECOND_CHAN_NONE));
}

static void send_callback(const esp_now_send_info_t *tx_info,
                          esp_now_send_status_t status)
{
    if (tx_info == NULL) return;
    ESP_LOGI(TAG, "TX destino=" MACSTR " status=%s",
             MAC2STR(tx_info->des_addr),
             status == ESP_NOW_SEND_SUCCESS ? "ok" : "fail");
}

static void sender_task(void *arg)
{
    (void)arg;
    test_packet_t packet = {.magic = 0x4553504E, .sequence = 0};
    while (true) {
        snprintf(packet.text, sizeof(packet.text), "official-test-%lu",
                 (unsigned long)packet.sequence);
        ESP_LOGI(TAG, "TX seq=%lu canal=%d bytes=%u",
                 (unsigned long)packet.sequence, ESPNOW_CHANNEL,
                 (unsigned)sizeof(packet));
        esp_err_t err = esp_now_send(s_broadcast_mac,
                                     (const uint8_t *)&packet, sizeof(packet));
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "esp_now_send fallo: %s", esp_err_to_name(err));
        }
        packet.sequence++;
        vTaskDelay(pdMS_TO_TICKS(1000));
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
    memcpy(peer.peer_addr, s_broadcast_mac, ESP_NOW_ETH_ALEN);
    peer.channel = ESPNOW_CHANNEL;
    peer.ifidx = WIFI_IF_STA;
    peer.encrypt = false;
    ESP_ERROR_CHECK(esp_now_add_peer(&peer));

    uint8_t mac[ESP_NOW_ETH_ALEN];
    ESP_ERROR_CHECK(esp_wifi_get_mac(WIFI_IF_STA, mac));
    ESP_LOGI(TAG, "FIRMWARE_BROADCAST_TX8D5_V3 LISTO MAC=" MACSTR
             " canal=%d tx_max=8.5dBm destino=" MACSTR,
             MAC2STR(mac), ESPNOW_CHANNEL, MAC2STR(s_broadcast_mac));
    if (xTaskCreate(sender_task, "sender_task", 3072, NULL, 4, NULL) != pdPASS) {
        ESP_LOGE(TAG, "No se pudo crear sender_task");
    }
}
