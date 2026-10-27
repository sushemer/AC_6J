#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "esp_err.h"
#include "esp_intr_alloc.h"

#define IR_RX_GPIO              22
#define IR_MAX_EDGES            400
#define IR_FRAME_TIMEOUT_US     60000
#define IR_MAX_CAPTURE_US       65535

static volatile uint32_t raw_duration[IR_MAX_EDGES];
static volatile uint8_t raw_level[IR_MAX_EDGES];
static volatile size_t raw_count;
static volatile int64_t last_edge_us;
static volatile int last_level;
static volatile bool capturing;
static volatile bool armed;
static bool isr_installed;

static uint32_t saved_duration[IR_MAX_EDGES];
static uint8_t saved_level[IR_MAX_EDGES];
static size_t saved_count;
static volatile bool capture_ready;

static void arm_capture(void)
{
    gpio_intr_disable(IR_RX_GPIO);
    raw_count = 0;
    capturing = false;
    capture_ready = false;
    last_edge_us = 0;
    last_level = gpio_get_level(IR_RX_GPIO);
    armed = true;
    gpio_set_intr_type(IR_RX_GPIO, GPIO_INTR_ANYEDGE);
    gpio_intr_enable(IR_RX_GPIO);
}

static void IRAM_ATTR ir_gpio_isr(void *arg){
    if (!armed) return;

    const int level = gpio_get_level(IR_RX_GPIO);
    const int64_t now = esp_timer_get_time();

    if (!capturing) {
        capturing = true;
        raw_count = 0;
        last_edge_us = now;
        last_level = !level; // nivel que existia antes del primer flanco
        return;
    }

    int64_t duration = now - last_edge_us;
    if (duration < 1) duration = 1;
    if (duration > IR_MAX_CAPTURE_US) duration = IR_MAX_CAPTURE_US;

    if (raw_count < IR_MAX_EDGES) {
        raw_duration[raw_count] = (uint32_t)duration;
        raw_level[raw_count] = (uint8_t)last_level;
        raw_count++;
    }
    last_edge_us = now;
    last_level = level;
}

static void print_capture_array(const char *name, bool ready_for_npn)
{
    printf("static const rmt_symbol_word_t %s[] = {\n", name);

    size_t symbols = (saved_count + 1) / 2;
    for (size_t i = 0; i < symbols; ++i) {
        size_t p0 = i * 2;
        size_t p1 = p0 + 1;
        uint32_t d0 = saved_duration[p0];
        uint8_t l0 = saved_level[p0];
        uint32_t d1 = 20000;
        uint8_t l1 = 1;
        if (p1 < saved_count) {
            d1 = saved_duration[p1];
            l1 = saved_level[p1];
        }

        /*
         * El VS1838B entrega la marca IR en nivel bajo. El emisor contransistor NPN necesita la marca en HIGH y el espacio en LOW.
         * En la primera pareja la captura puede comenzar durante el estado inactivo; por eso se normalizan ambos niveles para el emisor.
         */
        if (ready_for_npn) {
            l0 = 1;
            l1 = 0;
        }

        printf("    {.duration0 = %u, .level0 = %u, .duration1 = %u, .level1 = %u},\n",
               (unsigned)d0, l0, (unsigned)d1, l1);
    }
    printf("};\nstatic const size_t %s_count = %u;\n",
           name,
           (unsigned)symbols);
}

static void print_capture(void)
{
    printf("\n=== VERSION ORIGINAL DEL VS1838B ===\n");
    print_capture_array("ir_signal_raw", false);

    printf("\n=== VERSION LISTA PARA EMISOR NPN ===\n");
    print_capture_array("ir_signal_npn", true);

    printf("// Pulsos capturados: %u\n", (unsigned)saved_count);
    printf("// Fin de captura\n\n");
}

static void finish_capture(void)
{
    gpio_intr_disable(IR_RX_GPIO);
    gpio_set_intr_type(IR_RX_GPIO, GPIO_INTR_DISABLE);

    saved_count = raw_count;
    if (saved_count > IR_MAX_EDGES) saved_count = IR_MAX_EDGES;
    memcpy(saved_duration, (const void *)raw_duration,
           saved_count * sizeof(saved_duration[0]));
    memcpy(saved_level, (const void *)raw_level,
           saved_count * sizeof(saved_level[0]));

    raw_count = 0;
    capturing = false;
    capture_ready = true;
    armed = false;
}

static void console_task(void *arg)
{
    (void)arg;
    while (true) {
        int c = getchar();
        if (c == 'r' || c == 'R') {
            arm_capture();
            printf("Captura armada. Presiona un boton del control Midea.\n");
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void app_main(void){
    gpio_config_t io = {
        .pin_bit_mask = 1ULL << IR_RX_GPIO,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&io));
    ESP_ERROR_CHECK(gpio_install_isr_service(ESP_INTR_FLAG_IRAM));
    ESP_ERROR_CHECK(gpio_isr_handler_add(IR_RX_GPIO, ir_gpio_isr, NULL));

    printf("\n=== Receptor IR ESP-IDF para ESP32 DevKit V1 ===\n");
    printf("VS1838B OUT conectado a GPIO22\n");
    printf("VCC del VS1838B a 3.3 V, GND comun y OUT a GPIO22.\n");
    printf("Captura armada automaticamente. Presiona un boton del control Midea.\n");
    printf("Escribe r y presiona Enter para repetir una captura.\n");

    xTaskCreate(console_task, "ir_console", 2048, NULL, 5, NULL);
    arm_capture();

    while (true) {
        if (capturing && (esp_timer_get_time() - last_edge_us) > IR_FRAME_TIMEOUT_US) {
            finish_capture();
        }
        if (capture_ready) {
            capture_ready = false;
            print_capture();
            printf("Captura lista. Escribe r + Enter para repetir.\n");
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
