#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "nvs_flash.h"

#define WIFI_CONNECTED_BIT BIT0
static const char *TAG = "wifi_emisor";
static EventGroupHandle_t wifi_events;
static int retry_count;

static void wifi_event_handler(void *arg, esp_event_base_t base, int32_t id, void *data){
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START) {
        ESP_LOGI(TAG, "Iniciando conexion WiFi al SSID: %s", CONFIG_WIFI_SSID);
        esp_wifi_connect();
    }
    else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        wifi_event_sta_disconnected_t *event = (wifi_event_sta_disconnected_t *)data;
        xEventGroupClearBits(wifi_events, WIFI_CONNECTED_BIT);
        retry_count++;
        ESP_LOGW(TAG, "WiFi desconectado; razon=%d; reintento continuo #%d", event->reason, retry_count);
        esp_wifi_connect();
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)data;
        retry_count = 0;
        ESP_LOGI(TAG, "IP recibida: " IPSTR, IP2STR(&event->ip_info.ip));
        ESP_LOGI(TAG, "Mascara: " IPSTR ", gateway: " IPSTR,
                 IP2STR(&event->ip_info.netmask), IP2STR(&event->ip_info.gw));
        xEventGroupSetBits(wifi_events, WIFI_CONNECTED_BIT);
    }
}

static void wifi_init_sta(void){
    wifi_events = xEventGroupCreate();
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL));
    wifi_config_t wifi_config = { 0 };
    strncpy((char *)wifi_config.sta.ssid, CONFIG_WIFI_SSID, sizeof(wifi_config.sta.ssid) - 1);
    strncpy((char *)wifi_config.sta.password, CONFIG_WIFI_PASSWORD, sizeof(wifi_config.sta.password) - 1);
    wifi_config.sta.threshold.authmode = WIFI_AUTH_OPEN;
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_LOGI(TAG, "Configuracion TCP: receptor=%s, puerto=%d", CONFIG_TCP_SERVER_IP, CONFIG_TCP_PORT);
    ESP_ERROR_CHECK(esp_wifi_start());
    xEventGroupWaitBits(wifi_events, WIFI_CONNECTED_BIT, pdFALSE, pdFALSE, portMAX_DELAY);
    ESP_LOGI(TAG, "Conectado al WiFi; destino %s:%d", CONFIG_TCP_SERVER_IP, CONFIG_TCP_PORT);
}

static int connect_receiver(void){
    struct sockaddr_in dest = { 0 };
    dest.sin_family = AF_INET; dest.sin_port = htons(CONFIG_TCP_PORT);
    ESP_LOGI(TAG, "Intentando conectar con receptor %s:%d", CONFIG_TCP_SERVER_IP, CONFIG_TCP_PORT);
    if (inet_pton(AF_INET, CONFIG_TCP_SERVER_IP, &dest.sin_addr) != 1) { ESP_LOGE(TAG, "IP del receptor invalida"); return -1; }
    int sock = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
    if (sock < 0) { ESP_LOGE(TAG, "socket() fallo: errno %d", errno); return -1; }
    if (connect(sock, (struct sockaddr *)&dest, sizeof(dest)) != 0) { ESP_LOGW(TAG, "No conecta al receptor: errno %d", errno); close(sock); return -1; }
    ESP_LOGI(TAG, "Conectado al receptor TCP");
    return sock;
}

static void sender_task(void *arg){
    uint32_t sequence = 1;
    char message[128];
    char ack[32];
    int sock = -1;
    while (true) {
        if ((xEventGroupGetBits(wifi_events) & WIFI_CONNECTED_BIT) == 0) {
            if (sock >= 0) { shutdown(sock, 0); close(sock); sock = -1; }
            ESP_LOGW(TAG, "Sin WiFi/IP; esperando reconexion antes de enviar");
            vTaskDelay(pdMS_TO_TICKS(2000));
            continue;
        }
        if (sock < 0) { sock = connect_receiver(); if (sock < 0) { vTaskDelay(pdMS_TO_TICKS(2000)); continue; } }
        int len = snprintf(message, sizeof(message), "SEQ=%lu;PAYLOAD=wifi-test\n", (unsigned long)sequence);
        if (send(sock, message, len, 0) < 0) { ESP_LOGW(TAG, "TX fallo: errno %d", errno); close(sock); sock = -1; continue; }
        int ack_len = recv(sock, ack, sizeof(ack) - 1, 0);
        if (ack_len > 0) { ack[ack_len] = '\0'; ESP_LOGI(TAG, "TX %lu confirmado: %s", (unsigned long)sequence, ack); sequence++; }
        else { ESP_LOGW(TAG, "ACK no recibido; reconectando"); close(sock); sock = -1; }
        vTaskDelay(pdMS_TO_TICKS(CONFIG_SEND_INTERVAL_MS));
    }
}

void app_main(void){
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) { ESP_ERROR_CHECK(nvs_flash_erase()); ret = nvs_flash_init(); }
    ESP_ERROR_CHECK(ret);
    wifi_init_sta();
    xTaskCreate(sender_task, "sender", 4096, NULL, 5, NULL);
}
