#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include "driver/gpio.h"
#include "driver/rmt_encoder.h"
#include "driver/rmt_tx.h"
#include "esp_check.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// ESP32 DevKit V1 / WROOM-32 clasico.
#define IR_TX_GPIO GPIO_NUM_32
#define BUTTON_UP_GPIO GPIO_NUM_25
#define BUTTON_DOWN_GPIO GPIO_NUM_26
#define BUTTON_POWER_GPIO GPIO_NUM_27
#define IR_CARRIER_HZ 38000
#define IR_RESOLUTION_HZ 1000000

/* Sustituye cada arreglo por la captura correspondiente del Receptor. */
static const rmt_symbol_word_t signal_up[] = {
    {.duration0=9000,.level0=0,.duration1=4500,.level1=1},
    {.duration0=560,.level0=0,.duration1=560,.level1=1},
};
static const rmt_symbol_word_t signal_down[] = {
    {.duration0=9000,.level0=0,.duration1=4500,.level1=1},
    {.duration0=560,.level0=0,.duration1=560,.level1=1},
};
/* Captura del boton POWER del control Midea.
 * Los niveles fueron invertidos: el VS1838B entrega la marca IR en nivel bajo,
 * mientras el transmisor RMT usa nivel alto para activar la portadora. */
static const rmt_symbol_word_t signal_power[] = {
    {.duration0=4368, .level0=0, .duration1=4373, .level1=0},
    {.duration0=533, .level0=1, .duration1=1634, .level1=0},
    {.duration0=507, .level0=1, .duration1=539, .level1=0},
    {.duration0=532, .level0=1, .duration1=1632, .level1=0},
    {.duration0=508, .level0=1, .duration1=539, .level1=0},
    {.duration0=531, .level0=1, .duration1=538, .level1=0},
    {.duration0=532, .level0=1, .duration1=539, .level1=0},
    {.duration0=531, .level0=1, .duration1=539, .level1=0},
    {.duration0=532, .level0=1, .duration1=1633, .level1=0},
    {.duration0=507, .level0=1, .duration1=1634, .level1=0},
    {.duration0=506, .level0=1, .duration1=540, .level1=0},
    {.duration0=531, .level0=1, .duration1=1634, .level1=0},
    {.duration0=506, .level0=1, .duration1=540, .level1=0},
    {.duration0=530, .level0=1, .duration1=540, .level1=0},
    {.duration0=530, .level0=1, .duration1=541, .level1=0},
    {.duration0=530, .level0=1, .duration1=540, .level1=0},
    {.duration0=530, .level0=1, .duration1=540, .level1=0},
    {.duration0=531, .level0=1, .duration1=540, .level1=0},
    {.duration0=529, .level0=1, .duration1=1635, .level1=0},
    {.duration0=507, .level0=1, .duration1=1634, .level1=0},
    {.duration0=506, .level0=1, .duration1=541, .level1=0},
    {.duration0=529, .level0=1, .duration1=542, .level1=0},
    {.duration0=528, .level0=1, .duration1=1635, .level1=0},
    {.duration0=506, .level0=1, .duration1=541, .level1=0},
    {.duration0=529, .level0=1, .duration1=1635, .level1=0},
    {.duration0=506, .level0=1, .duration1=1634, .level1=0},
    {.duration0=506, .level0=1, .duration1=1635, .level1=0},
    {.duration0=506, .level0=1, .duration1=1635, .level1=0},
    {.duration0=505, .level0=1, .duration1=1635, .level1=0},
    {.duration0=506, .level0=1, .duration1=1635, .level1=0},
    {.duration0=506, .level0=1, .duration1=1635, .level1=0},
    {.duration0=506, .level0=1, .duration1=1635, .level1=0},
    {.duration0=505, .level0=1, .duration1=1635, .level1=0},
    {.duration0=506, .level0=1, .duration1=1634, .level1=0},
    {.duration0=506, .level0=1, .duration1=1634, .level1=0},
    {.duration0=506, .level0=1, .duration1=1636, .level1=0},
    {.duration0=505, .level0=1, .duration1=1634, .level1=0},
    {.duration0=507, .level0=1, .duration1=1634, .level1=0},
    {.duration0=506, .level0=1, .duration1=1634, .level1=0},
    {.duration0=506, .level0=1, .duration1=1635, .level1=0},
    {.duration0=505, .level0=1, .duration1=1635, .level1=0},
    {.duration0=506, .level0=1, .duration1=542, .level1=0},
    {.duration0=528, .level0=1, .duration1=1635, .level1=0},
    {.duration0=505, .level0=1, .duration1=543, .level1=0},
    {.duration0=528, .level0=1, .duration1=542, .level1=0},
    {.duration0=527, .level0=1, .duration1=1635, .level1=0},
    {.duration0=507, .level0=1, .duration1=541, .level1=0},
    {.duration0=527, .level0=1, .duration1=1636, .level1=0},
    {.duration0=505, .level0=1, .duration1=1634, .level1=0},
    {.duration0=506, .level0=1, .duration1=5208, .level1=0},
    {.duration0=4344, .level0=1, .duration1=4379, .level1=0},
    {.duration0=527, .level0=1, .duration1=542, .level1=0},
    {.duration0=528, .level0=1, .duration1=1634, .level1=0},
    {.duration0=505, .level0=1, .duration1=543, .level1=0},
    {.duration0=527, .level0=1, .duration1=1634, .level1=0},
    {.duration0=506, .level0=1, .duration1=1634, .level1=0},
    {.duration0=506, .level0=1, .duration1=1634, .level1=0},
    {.duration0=506, .level0=1, .duration1=1633, .level1=0},
    {.duration0=507, .level0=1, .duration1=541, .level1=0},
    {.duration0=528, .level0=1, .duration1=542, .level1=0},
    {.duration0=528, .level0=1, .duration1=1634, .level1=0},
    {.duration0=506, .level0=1, .duration1=541, .level1=0},
    {.duration0=528, .level0=1, .duration1=1634, .level1=0},
    {.duration0=506, .level0=1, .duration1=1634, .level1=0},
    {.duration0=506, .level0=1, .duration1=1634, .level1=0},
    {.duration0=506, .level0=1, .duration1=1633, .level1=0},
    {.duration0=507, .level0=1, .duration1=1633, .level1=0},
    {.duration0=505, .level0=1, .duration1=1634, .level1=0},
    {.duration0=507, .level0=1, .duration1=541, .level1=0},
    {.duration0=528, .level0=1, .duration1=541, .level1=0},
    {.duration0=529, .level0=1, .duration1=1634, .level1=0},
    {.duration0=505, .level0=1, .duration1=1634, .level1=0},
    {.duration0=506, .level0=1, .duration1=541, .level1=0},
    {.duration0=528, .level0=1, .duration1=1634, .level1=0},
    {.duration0=506, .level0=1, .duration1=542, .level1=0},
    {.duration0=529, .level0=1, .duration1=540, .level1=0},
    {.duration0=529, .level0=1, .duration1=542, .level1=0},
    {.duration0=528, .level0=1, .duration1=541, .level1=0},
    {.duration0=528, .level0=1, .duration1=541, .level1=0},
    {.duration0=529, .level0=1, .duration1=541, .level1=0},
    {.duration0=529, .level0=1, .duration1=541, .level1=0},
    {.duration0=529, .level0=1, .duration1=541, .level1=0},
    {.duration0=529, .level0=1, .duration1=541, .level1=0},
    {.duration0=528, .level0=1, .duration1=542, .level1=0},
    {.duration0=528, .level0=1, .duration1=542, .level1=0},
    {.duration0=527, .level0=1, .duration1=543, .level1=0},
    {.duration0=528, .level0=1, .duration1=542, .level1=0},
    {.duration0=527, .level0=1, .duration1=543, .level1=0},
    {.duration0=527, .level0=1, .duration1=542, .level1=0},
    {.duration0=528, .level0=1, .duration1=542, .level1=0},
    {.duration0=527, .level0=1, .duration1=542, .level1=0},
    {.duration0=528, .level0=1, .duration1=1631, .level1=0},
    {.duration0=508, .level0=1, .duration1=542, .level1=0},
    {.duration0=528, .level0=1, .duration1=1631, .level1=0},
    {.duration0=508, .level0=1, .duration1=1632, .level1=0},
    {.duration0=508, .level0=1, .duration1=542, .level1=0},
    {.duration0=528, .level0=1, .duration1=1630, .level1=0},
    {.duration0=509, .level0=1, .duration1=542, .level1=0},
    {.duration0=528, .level0=1, .duration1=542, .level1=0},
    {.duration0=528, .level0=1, .duration1=20000, .level1=0},
};

static rmt_channel_handle_t tx_channel;
static rmt_encoder_handle_t copy_encoder;

static void send_signal(const rmt_symbol_word_t *signal, size_t count, const char *name)
{
    rmt_transmit_config_t config = {.loop_count = 0};
    ESP_ERROR_CHECK(rmt_transmit(tx_channel, copy_encoder, signal,
                                 count * sizeof(rmt_symbol_word_t), &config));
    ESP_ERROR_CHECK(rmt_tx_wait_all_done(tx_channel, -1));
    printf("IR enviado: %s\n", name);
}

static bool button_pressed(gpio_num_t pin)
{
    if (gpio_get_level(pin) != 0) return false;
    vTaskDelay(pdMS_TO_TICKS(35));
    if (gpio_get_level(pin) != 0) return false;
    while (gpio_get_level(pin) == 0) vTaskDelay(pdMS_TO_TICKS(10));
    return true;
}

void app_main(void)
{
    const gpio_config_t buttons = {
        .pin_bit_mask=(1ULL<<BUTTON_UP_GPIO)|(1ULL<<BUTTON_DOWN_GPIO)|(1ULL<<BUTTON_POWER_GPIO),
        .mode=GPIO_MODE_INPUT, .pull_up_en=GPIO_PULLUP_ENABLE,
        .pull_down_en=GPIO_PULLDOWN_DISABLE, .intr_type=GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&buttons));

    const rmt_tx_channel_config_t tx_config = {
        .gpio_num=IR_TX_GPIO, .clk_src=RMT_CLK_SRC_DEFAULT,
        .resolution_hz=IR_RESOLUTION_HZ, .mem_block_symbols=128,
        .trans_queue_depth=2, .flags.invert_out=false,
    };
    ESP_ERROR_CHECK(rmt_new_tx_channel(&tx_config, &tx_channel));
    const rmt_carrier_config_t carrier = {
        .frequency_hz=IR_CARRIER_HZ,
        .duty_cycle=0.33,
        .flags.polarity_active_low=false,
    };
    ESP_ERROR_CHECK(rmt_apply_carrier(tx_channel, &carrier));
    const rmt_copy_encoder_config_t copy_config = {};
    ESP_ERROR_CHECK(rmt_new_copy_encoder(&copy_config, &copy_encoder));
    ESP_ERROR_CHECK(rmt_enable(tx_channel));

    printf("\n=== EMISOR IR - ESP32 DevKit V1 ===\n");
    printf("LED GPIO%d | SUBIR GPIO%d | BAJAR GPIO%d | POWER GPIO%d\n",
           IR_TX_GPIO, BUTTON_UP_GPIO, BUTTON_DOWN_GPIO, BUTTON_POWER_GPIO);
    printf("Cada boton va entre su GPIO y GND.\n");
    printf("Listo. Esperando botones...\n");

    while (true) {
        if (button_pressed(BUTTON_UP_GPIO)) {
            printf("Boton SUBIR detectado\n");
            send_signal(signal_up, sizeof(signal_up)/sizeof(signal_up[0]), "SUBIR");
        } else if (button_pressed(BUTTON_DOWN_GPIO)) {
            printf("Boton BAJAR detectado\n");
            send_signal(signal_down, sizeof(signal_down)/sizeof(signal_down[0]), "BAJAR");
        } else if (button_pressed(BUTTON_POWER_GPIO)) {
            printf("Boton POWER detectado\n");
            send_signal(signal_power, sizeof(signal_power)/sizeof(signal_power[0]), "POWER");
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
