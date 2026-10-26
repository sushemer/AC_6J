#include <errno.h>
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

static const char *TAG = "wifi_receptor";
static EventGroupHandle_t wifi_events;
static int retry_count;

static void wifi_event_handler(void *arg, esp_event_base_t base, int32_t id, void *data){
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START) {
        ESP_LOGI(TAG, "Iniciando conexion WiFi al SSID: %s", CONFIG_WIFI_SSID);
        esp_err_t err = esp_wifi_connect();
        ESP_LOGI(TAG, "esp_wifi_connect() inicial: %s", esp_err_to_name(err));
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        wifi_event_sta_disconnected_t *event = (wifi_event_sta_disconnected_t *)data;
        ESP_LOGW(TAG, "WiFi desconectado; razon=%d", event->reason);
        esp_err_t err = esp_wifi_connect();
        retry_count++;
        ESP_LOGW(TAG, "Reintento continuo #%d; resultado=%s", retry_count, esp_err_to_name(err));
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)data;
        retry_count = 0;
        ESP_LOGI(TAG, "IP recibida: " IPSTR, IP2STR(&event->ip_info.ip));
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
    // La red de prueba usa WPA2-PSK. Se declara explícitamente para evitar
    // que el C3 negocie primero como OPEN y el router expire la autenticación.
    wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    wifi_config.sta.pmf_cfg.capable = true;
    wifi_config.sta.pmf_cfg.required = false;
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_LOGI(TAG, "Configuracion WiFi cargada; SSID=%s", CONFIG_WIFI_SSID);
    uint8_t mac[6] = {0};
    ESP_ERROR_CHECK(esp_wifi_get_mac(WIFI_IF_STA, mac));
    ESP_LOGI(TAG, "MAC WiFi del receptor: %02X:%02X:%02X:%02X:%02X:%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    ESP_ERROR_CHECK(esp_wifi_start());
    xEventGroupWaitBits(wifi_events, WIFI_CONNECTED_BIT, pdFALSE, pdFALSE, portMAX_DELAY);
    ESP_LOGI(TAG, "Conectado al WiFi; esperando emisor TCP en puerto %d", CONFIG_TCP_PORT);
}

static void log_link_quality(void){
    wifi_ap_record_t ap_info = {0};
    esp_err_t err = esp_wifi_sta_get_ap_info(&ap_info);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "No se pudo leer RSSI/calidad WiFi: %s", esp_err_to_name(err));
        return;
    }

    // Estimacion sencilla de calidad a partir del RSSI, en escala 0-100.
    int quality = 2 * (ap_info.rssi + 100);
    if (quality < 0) quality = 0;
    if (quality > 100) quality = 100;

    ESP_LOGI(TAG, "Enlace WiFi: RSSI=%d dBm, calidad=%d%%, canal=%d",
             ap_info.rssi, quality, ap_info.primary);
}

static void tcp_server_task(void *arg){
    char rx_buffer[256];
    struct sockaddr_in address = { 0 };
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(CONFIG_TCP_PORT);
    int server = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
    if (server < 0) { ESP_LOGE(TAG, "socket() fallo: errno %d", errno); vTaskDelete(NULL); return; }
    int reuse = 1;
    setsockopt(server, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
    if (bind(server, (struct sockaddr *)&address, sizeof(address)) < 0 || listen(server, 1) < 0) {
        ESP_LOGE(TAG, "bind/listen fallo: errno %d", errno); close(server); vTaskDelete(NULL); return;
    }
    ESP_LOGI(TAG, "Receptor listo; esperando conexion TCP");
    while (true) {
        struct sockaddr_in source = { 0 };
        socklen_t addr_len = sizeof(source);
        int client = accept(server, (struct sockaddr *)&source, &addr_len);
        if (client < 0) { ESP_LOGW(TAG, "accept fallo: errno %d", errno); continue; }
        char source_ip[INET_ADDRSTRLEN] = {0};
        inet_ntoa_r(source.sin_addr, source_ip, sizeof(source_ip));
        ESP_LOGI(TAG, "Emisor conectado desde %s", source_ip);
        int len;
        while ((len = recv(client, rx_buffer, sizeof(rx_buffer) - 1, 0)) > 0) {
            rx_buffer[len] = '\0';
            log_link_quality();
            ESP_LOGI(TAG, "RX %d bytes: %s", len, rx_buffer);
            const char ack[] = "ACK\n";
            send(client, ack, sizeof(ack) - 1, 0);
        }
        ESP_LOGW(TAG, "Emisor desconectado; esperando reconexion");
        shutdown(client, 0);
        close(client);
    }
}

void app_main(void){
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    wifi_init_sta();
    xTaskCreate(tcp_server_task, "tcp_server", 4096, NULL, 5, NULL);
}
