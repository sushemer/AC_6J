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

/* Capturas completas del control Midea a 67 F.
 * signal_on: captura con el aire encendido.
 * signal_off: captura con el aire apagado.
 * El checksum ya viene incluido en cada trama capturada.
 * Niveles normalizados para NPN: marca HIGH (transistor conduce), espacio LOW.
 */
/* Estas dos ranuras se llenan con capturas completas del receptor.
 * Cada captura debe corresponder a un estado real del aire:
 * signal_temp_up: pulsacion que sube la temperatura.
 * signal_temp_down: pulsacion que baja la temperatura.
 */
static const rmt_symbol_word_t legacy_signal_temp_up[] __attribute__((unused)) = {

    {.duration0 = 4367, .level0 = 1, .duration1 = 4374, .level1 = 0},

    {.duration0 = 557, .level0 = 1, .duration1 = 1613, .level1 = 0},

    {.duration0 = 502, .level0 = 1, .duration1 = 565, .level1 = 0},

    {.duration0 = 528, .level0 = 1, .duration1 = 1611, .level1 = 0},

    {.duration0 = 505, .level0 = 1, .duration1 = 565, .level1 = 0},

    {.duration0 = 505, .level0 = 1, .duration1 = 565, .level1 = 0},

    {.duration0 = 505, .level0 = 1, .duration1 = 565, .level1 = 0},

    {.duration0 = 528, .level0 = 1, .duration1 = 541, .level1 = 0},

    {.duration0 = 505, .level0 = 1, .duration1 = 1635, .level1 = 0},

    {.duration0 = 530, .level0 = 1, .duration1 = 1609, .level1 = 0},

    {.duration0 = 506, .level0 = 1, .duration1 = 564, .level1 = 0},

    {.duration0 = 506, .level0 = 1, .duration1 = 564, .level1 = 0},

    {.duration0 = 528, .level0 = 1, .duration1 = 542, .level1 = 0},

    {.duration0 = 504, .level0 = 1, .duration1 = 565, .level1 = 0},

    {.duration0 = 530, .level0 = 1, .duration1 = 540, .level1 = 0},

    {.duration0 = 520, .level0 = 1, .duration1 = 1620, .level1 = 0},

    {.duration0 = 504, .level0 = 1, .duration1 = 566, .level1 = 0},

    {.duration0 = 504, .level0 = 1, .duration1 = 565, .level1 = 0},

    {.duration0 = 506, .level0 = 1, .duration1 = 1634, .level1 = 0},

    {.duration0 = 505, .level0 = 1, .duration1 = 1635, .level1 = 0},

    {.duration0 = 505, .level0 = 1, .duration1 = 565, .level1 = 0},

    {.duration0 = 528, .level0 = 1, .duration1 = 541, .level1 = 0},

    {.duration0 = 506, .level0 = 1, .duration1 = 1633, .level1 = 0},

    {.duration0 = 530, .level0 = 1, .duration1 = 1610, .level1 = 0},

    {.duration0 = 505, .level0 = 1, .duration1 = 1634, .level1 = 0},

    {.duration0 = 529, .level0 = 1, .duration1 = 1610, .level1 = 0},

    {.duration0 = 505, .level0 = 1, .duration1 = 1635, .level1 = 0},

    {.duration0 = 505, .level0 = 1, .duration1 = 1634, .level1 = 0},

    {.duration0 = 506, .level0 = 1, .duration1 = 1633, .level1 = 0},

    {.duration0 = 507, .level0 = 1, .duration1 = 1632, .level1 = 0},

    {.duration0 = 507, .level0 = 1, .duration1 = 1633, .level1 = 0},

    {.duration0 = 507, .level0 = 1, .duration1 = 1632, .level1 = 0},

    {.duration0 = 507, .level0 = 1, .duration1 = 1632, .level1 = 0},

    {.duration0 = 508, .level0 = 1, .duration1 = 1632, .level1 = 0},

    {.duration0 = 508, .level0 = 1, .duration1 = 1631, .level1 = 0},

    {.duration0 = 506, .level0 = 1, .duration1 = 1634, .level1 = 0},

    {.duration0 = 506, .level0 = 1, .duration1 = 1634, .level1 = 0},

    {.duration0 = 506, .level0 = 1, .duration1 = 1634, .level1 = 0},

    {.duration0 = 506, .level0 = 1, .duration1 = 1611, .level1 = 0},

    {.duration0 = 529, .level0 = 1, .duration1 = 1609, .level1 = 0},

    {.duration0 = 529, .level0 = 1, .duration1 = 1610, .level1 = 0},

    {.duration0 = 530, .level0 = 1, .duration1 = 565, .level1 = 0},

    {.duration0 = 507, .level0 = 1, .duration1 = 1608, .level1 = 0},

    {.duration0 = 530, .level0 = 1, .duration1 = 1608, .level1 = 0},

    {.duration0 = 530, .level0 = 1, .duration1 = 567, .level1 = 0},

    {.duration0 = 503, .level0 = 1, .duration1 = 1610, .level1 = 0},

    {.duration0 = 530, .level0 = 1, .duration1 = 565, .level1 = 0},

    {.duration0 = 505, .level0 = 1, .duration1 = 1609, .level1 = 0},

    {.duration0 = 530, .level0 = 1, .duration1 = 566, .level1 = 0},

    {.duration0 = 504, .level0 = 1, .duration1 = 5183, .level1 = 0},

    {.duration0 = 4366, .level0 = 1, .duration1 = 4376, .level1 = 0},

    {.duration0 = 529, .level0 = 1, .duration1 = 542, .level1 = 0},

    {.duration0 = 528, .level0 = 1, .duration1 = 1610, .level1 = 0},

    {.duration0 = 530, .level0 = 1, .duration1 = 542, .level1 = 0},

    {.duration0 = 528, .level0 = 1, .duration1 = 1610, .level1 = 0},

    {.duration0 = 529, .level0 = 1, .duration1 = 1610, .level1 = 0},

    {.duration0 = 530, .level0 = 1, .duration1 = 1609, .level1 = 0},

    {.duration0 = 530, .level0 = 1, .duration1 = 1610, .level1 = 0},

    {.duration0 = 530, .level0 = 1, .duration1 = 541, .level1 = 0},

    {.duration0 = 528, .level0 = 1, .duration1 = 541, .level1 = 0},

    {.duration0 = 528, .level0 = 1, .duration1 = 1611, .level1 = 0},

    {.duration0 = 529, .level0 = 1, .duration1 = 1611, .level1 = 0},

    {.duration0 = 529, .level0 = 1, .duration1 = 1610, .level1 = 0},

    {.duration0 = 529, .level0 = 1, .duration1 = 1610, .level1 = 0},

    {.duration0 = 529, .level0 = 1, .duration1 = 1611, .level1 = 0},

    {.duration0 = 529, .level0 = 1, .duration1 = 542, .level1 = 0},

    {.duration0 = 527, .level0 = 1, .duration1 = 1612, .level1 = 0},

    {.duration0 = 528, .level0 = 1, .duration1 = 1611, .level1 = 0},

    {.duration0 = 528, .level0 = 1, .duration1 = 543, .level1 = 0},

    {.duration0 = 527, .level0 = 1, .duration1 = 542, .level1 = 0},

    {.duration0 = 528, .level0 = 1, .duration1 = 1611, .level1 = 0},

    {.duration0 = 528, .level0 = 1, .duration1 = 1612, .level1 = 0},

    {.duration0 = 528, .level0 = 1, .duration1 = 542, .level1 = 0},

    {.duration0 = 527, .level0 = 1, .duration1 = 543, .level1 = 0},

    {.duration0 = 527, .level0 = 1, .duration1 = 543, .level1 = 0},

    {.duration0 = 527, .level0 = 1, .duration1 = 543, .level1 = 0},

    {.duration0 = 526, .level0 = 1, .duration1 = 544, .level1 = 0},

    {.duration0 = 527, .level0 = 1, .duration1 = 543, .level1 = 0},

    {.duration0 = 526, .level0 = 1, .duration1 = 544, .level1 = 0},

    {.duration0 = 526, .level0 = 1, .duration1 = 543, .level1 = 0},

    {.duration0 = 526, .level0 = 1, .duration1 = 544, .level1 = 0},

    {.duration0 = 526, .level0 = 1, .duration1 = 544, .level1 = 0},

    {.duration0 = 525, .level0 = 1, .duration1 = 544, .level1 = 0},

    {.duration0 = 527, .level0 = 1, .duration1 = 545, .level1 = 0},

    {.duration0 = 524, .level0 = 1, .duration1 = 545, .level1 = 0},

    {.duration0 = 524, .level0 = 1, .duration1 = 547, .level1 = 0},

    {.duration0 = 523, .level0 = 1, .duration1 = 566, .level1 = 0},

    {.duration0 = 504, .level0 = 1, .duration1 = 566, .level1 = 0},

    {.duration0 = 503, .level0 = 1, .duration1 = 567, .level1 = 0},

    {.duration0 = 504, .level0 = 1, .duration1 = 566, .level1 = 0},

    {.duration0 = 503, .level0 = 1, .duration1 = 567, .level1 = 0},

    {.duration0 = 503, .level0 = 1, .duration1 = 1637, .level1 = 0},

    {.duration0 = 502, .level0 = 1, .duration1 = 567, .level1 = 0},

    {.duration0 = 503, .level0 = 1, .duration1 = 568, .level1 = 0},

    {.duration0 = 502, .level0 = 1, .duration1 = 1638, .level1 = 0},

    {.duration0 = 501, .level0 = 1, .duration1 = 567, .level1 = 0},

    {.duration0 = 503, .level0 = 1, .duration1 = 1637, .level1 = 0},

    {.duration0 = 502, .level0 = 1, .duration1 = 568, .level1 = 0},

    {.duration0 = 501, .level0 = 1, .duration1 = 1639, .level1 = 0},

    {.duration0 = 501, .level0 = 1, .duration1 = 20000, .level1 = 0},

};

static const rmt_symbol_word_t legacy_signal_temp_down[] __attribute__((unused)) = {

    {.duration0 = 4395, .level0 = 1, .duration1 = 4346, .level1 = 0},

    {.duration0 = 558, .level0 = 1, .duration1 = 1608, .level1 = 0},

    {.duration0 = 531, .level0 = 1, .duration1 = 518, .level1 = 0},

    {.duration0 = 553, .level0 = 1, .duration1 = 1608, .level1 = 0},

    {.duration0 = 531, .level0 = 1, .duration1 = 514, .level1 = 0},

    {.duration0 = 556, .level0 = 1, .duration1 = 539, .level1 = 0},

    {.duration0 = 531, .level0 = 1, .duration1 = 538, .level1 = 0},

    {.duration0 = 531, .level0 = 1, .duration1 = 540, .level1 = 0},

    {.duration0 = 530, .level0 = 1, .duration1 = 1609, .level1 = 0},

    {.duration0 = 530, .level0 = 1, .duration1 = 1609, .level1 = 0},

    {.duration0 = 531, .level0 = 1, .duration1 = 539, .level1 = 0},

    {.duration0 = 531, .level0 = 1, .duration1 = 539, .level1 = 0},

    {.duration0 = 530, .level0 = 1, .duration1 = 539, .level1 = 0},

    {.duration0 = 531, .level0 = 1, .duration1 = 539, .level1 = 0},

    {.duration0 = 531, .level0 = 1, .duration1 = 539, .level1 = 0},

    {.duration0 = 531, .level0 = 1, .duration1 = 1609, .level1 = 0},

    {.duration0 = 530, .level0 = 1, .duration1 = 515, .level1 = 0},

    {.duration0 = 555, .level0 = 1, .duration1 = 539, .level1 = 0},

    {.duration0 = 531, .level0 = 1, .duration1 = 1609, .level1 = 0},

    {.duration0 = 530, .level0 = 1, .duration1 = 1609, .level1 = 0},

    {.duration0 = 531, .level0 = 1, .duration1 = 538, .level1 = 0},

    {.duration0 = 532, .level0 = 1, .duration1 = 538, .level1 = 0},

    {.duration0 = 531, .level0 = 1, .duration1 = 1609, .level1 = 0},

    {.duration0 = 531, .level0 = 1, .duration1 = 1608, .level1 = 0},

    {.duration0 = 531, .level0 = 1, .duration1 = 539, .level1 = 0},

    {.duration0 = 532, .level0 = 1, .duration1 = 1607, .level1 = 0},

    {.duration0 = 531, .level0 = 1, .duration1 = 1609, .level1 = 0},

    {.duration0 = 531, .level0 = 1, .duration1 = 1608, .level1 = 0},

    {.duration0 = 531, .level0 = 1, .duration1 = 1609, .level1 = 0},

    {.duration0 = 531, .level0 = 1, .duration1 = 1608, .level1 = 0},

    {.duration0 = 532, .level0 = 1, .duration1 = 1607, .level1 = 0},

    {.duration0 = 531, .level0 = 1, .duration1 = 1609, .level1 = 0},

    {.duration0 = 531, .level0 = 1, .duration1 = 1609, .level1 = 0},

    {.duration0 = 531, .level0 = 1, .duration1 = 1608, .level1 = 0},

    {.duration0 = 531, .level0 = 1, .duration1 = 1609, .level1 = 0},

    {.duration0 = 531, .level0 = 1, .duration1 = 1608, .level1 = 0},

    {.duration0 = 531, .level0 = 1, .duration1 = 1609, .level1 = 0},

    {.duration0 = 531, .level0 = 1, .duration1 = 1608, .level1 = 0},

    {.duration0 = 531, .level0 = 1, .duration1 = 1608, .level1 = 0},

    {.duration0 = 532, .level0 = 1, .duration1 = 1608, .level1 = 0},

    {.duration0 = 531, .level0 = 1, .duration1 = 1609, .level1 = 0},

    {.duration0 = 530, .level0 = 1, .duration1 = 540, .level1 = 0},

    {.duration0 = 530, .level0 = 1, .duration1 = 1608, .level1 = 0},

    {.duration0 = 531, .level0 = 1, .duration1 = 1609, .level1 = 0},

    {.duration0 = 531, .level0 = 1, .duration1 = 539, .level1 = 0},

    {.duration0 = 531, .level0 = 1, .duration1 = 1608, .level1 = 0},

    {.duration0 = 531, .level0 = 1, .duration1 = 536, .level1 = 0},

    {.duration0 = 534, .level0 = 1, .duration1 = 1608, .level1 = 0},

    {.duration0 = 532, .level0 = 1, .duration1 = 1608, .level1 = 0},

    {.duration0 = 531, .level0 = 1, .duration1 = 5156, .level1 = 0},

    {.duration0 = 4394, .level0 = 1, .duration1 = 4347, .level1 = 0},

    {.duration0 = 557, .level0 = 1, .duration1 = 539, .level1 = 0},

    {.duration0 = 531, .level0 = 1, .duration1 = 1608, .level1 = 0},

    {.duration0 = 531, .level0 = 1, .duration1 = 539, .level1 = 0},

    {.duration0 = 532, .level0 = 1, .duration1 = 1607, .level1 = 0},

    {.duration0 = 532, .level0 = 1, .duration1 = 1608, .level1 = 0},

    {.duration0 = 531, .level0 = 1, .duration1 = 1608, .level1 = 0},

    {.duration0 = 531, .level0 = 1, .duration1 = 1609, .level1 = 0},

    {.duration0 = 531, .level0 = 1, .duration1 = 539, .level1 = 0},

    {.duration0 = 531, .level0 = 1, .duration1 = 539, .level1 = 0},

    {.duration0 = 531, .level0 = 1, .duration1 = 1607, .level1 = 0},

    {.duration0 = 532, .level0 = 1, .duration1 = 1607, .level1 = 0},

    {.duration0 = 533, .level0 = 1, .duration1 = 1607, .level1 = 0},

    {.duration0 = 533, .level0 = 1, .duration1 = 1606, .level1 = 0},

    {.duration0 = 532, .level0 = 1, .duration1 = 1607, .level1 = 0},

    {.duration0 = 532, .level0 = 1, .duration1 = 539, .level1 = 0},

    {.duration0 = 532, .level0 = 1, .duration1 = 1606, .level1 = 0},

    {.duration0 = 533, .level0 = 1, .duration1 = 1607, .level1 = 0},

    {.duration0 = 533, .level0 = 1, .duration1 = 539, .level1 = 0},

    {.duration0 = 530, .level0 = 1, .duration1 = 540, .level1 = 0},

    {.duration0 = 531, .level0 = 1, .duration1 = 1606, .level1 = 0},

    {.duration0 = 533, .level0 = 1, .duration1 = 1585, .level1 = 0},

    {.duration0 = 554, .level0 = 1, .duration1 = 539, .level1 = 0},

    {.duration0 = 531, .level0 = 1, .duration1 = 517, .level1 = 0},

    {.duration0 = 553, .level0 = 1, .duration1 = 1582, .level1 = 0},

    {.duration0 = 557, .level0 = 1, .duration1 = 540, .level1 = 0},

    {.duration0 = 530, .level0 = 1, .duration1 = 540, .level1 = 0},

    {.duration0 = 530, .level0 = 1, .duration1 = 539, .level1 = 0},

    {.duration0 = 531, .level0 = 1, .duration1 = 539, .level1 = 0},

    {.duration0 = 530, .level0 = 1, .duration1 = 517, .level1 = 0},

    {.duration0 = 553, .level0 = 1, .duration1 = 540, .level1 = 0},

    {.duration0 = 531, .level0 = 1, .duration1 = 539, .level1 = 0},

    {.duration0 = 529, .level0 = 1, .duration1 = 541, .level1 = 0},

    {.duration0 = 530, .level0 = 1, .duration1 = 516, .level1 = 0},

    {.duration0 = 553, .level0 = 1, .duration1 = 515, .level1 = 0},

    {.duration0 = 554, .level0 = 1, .duration1 = 516, .level1 = 0},

    {.duration0 = 532, .level0 = 1, .duration1 = 538, .level1 = 0},

    {.duration0 = 531, .level0 = 1, .duration1 = 539, .level1 = 0},

    {.duration0 = 531, .level0 = 1, .duration1 = 538, .level1 = 0},

    {.duration0 = 531, .level0 = 1, .duration1 = 539, .level1 = 0},

    {.duration0 = 531, .level0 = 1, .duration1 = 538, .level1 = 0},

    {.duration0 = 532, .level0 = 1, .duration1 = 1607, .level1 = 0},

    {.duration0 = 532, .level0 = 1, .duration1 = 537, .level1 = 0},

    {.duration0 = 533, .level0 = 1, .duration1 = 538, .level1 = 0},

    {.duration0 = 532, .level0 = 1, .duration1 = 1606, .level1 = 0},

    {.duration0 = 533, .level0 = 1, .duration1 = 538, .level1 = 0},

    {.duration0 = 532, .level0 = 1, .duration1 = 1606, .level1 = 0},

    {.duration0 = 533, .level0 = 1, .duration1 = 538, .level1 = 0},

    {.duration0 = 532, .level0 = 1, .duration1 = 538, .level1 = 0},

    {.duration0 = 532, .level0 = 1, .duration1 = 20000, .level1 = 0},

};

static const rmt_symbol_word_t signal_on[] = {

    {.duration0 = 4395, .level0 = 1, .duration1 = 4347, .level1 = 0},

    {.duration0 = 558, .level0 = 1, .duration1 = 1609, .level1 = 0},

    {.duration0 = 531, .level0 = 1, .duration1 = 538, .level1 = 0},

    {.duration0 = 531, .level0 = 1, .duration1 = 1609, .level1 = 0},

    {.duration0 = 530, .level0 = 1, .duration1 = 540, .level1 = 0},

    {.duration0 = 531, .level0 = 1, .duration1 = 539, .level1 = 0},

    {.duration0 = 530, .level0 = 1, .duration1 = 539, .level1 = 0},

    {.duration0 = 530, .level0 = 1, .duration1 = 540, .level1 = 0},

    {.duration0 = 530, .level0 = 1, .duration1 = 1610, .level1 = 0},

    {.duration0 = 530, .level0 = 1, .duration1 = 1609, .level1 = 0},

    {.duration0 = 530, .level0 = 1, .duration1 = 540, .level1 = 0},

    {.duration0 = 531, .level0 = 1, .duration1 = 538, .level1 = 0},

    {.duration0 = 531, .level0 = 1, .duration1 = 539, .level1 = 0},

    {.duration0 = 531, .level0 = 1, .duration1 = 539, .level1 = 0},

    {.duration0 = 534, .level0 = 1, .duration1 = 536, .level1 = 0},

    {.duration0 = 530, .level0 = 1, .duration1 = 1610, .level1 = 0},

    {.duration0 = 530, .level0 = 1, .duration1 = 540, .level1 = 0},

    {.duration0 = 529, .level0 = 1, .duration1 = 541, .level1 = 0},

    {.duration0 = 529, .level0 = 1, .duration1 = 1610, .level1 = 0},

    {.duration0 = 529, .level0 = 1, .duration1 = 1610, .level1 = 0},

    {.duration0 = 530, .level0 = 1, .duration1 = 539, .level1 = 0},

    {.duration0 = 531, .level0 = 1, .duration1 = 539, .level1 = 0},

    {.duration0 = 530, .level0 = 1, .duration1 = 1610, .level1 = 0},

    {.duration0 = 529, .level0 = 1, .duration1 = 1610, .level1 = 0},

    {.duration0 = 530, .level0 = 1, .duration1 = 540, .level1 = 0},

    {.duration0 = 530, .level0 = 1, .duration1 = 1609, .level1 = 0},

    {.duration0 = 530, .level0 = 1, .duration1 = 1609, .level1 = 0},

    {.duration0 = 531, .level0 = 1, .duration1 = 1609, .level1 = 0},

    {.duration0 = 529, .level0 = 1, .duration1 = 1610, .level1 = 0},

    {.duration0 = 530, .level0 = 1, .duration1 = 1609, .level1 = 0},

    {.duration0 = 529, .level0 = 1, .duration1 = 1610, .level1 = 0},

    {.duration0 = 529, .level0 = 1, .duration1 = 1610, .level1 = 0},

    {.duration0 = 531, .level0 = 1, .duration1 = 1608, .level1 = 0},

    {.duration0 = 532, .level0 = 1, .duration1 = 1608, .level1 = 0},

    {.duration0 = 507, .level0 = 1, .duration1 = 1632, .level1 = 0},

    {.duration0 = 507, .level0 = 1, .duration1 = 1632, .level1 = 0},

    {.duration0 = 531, .level0 = 1, .duration1 = 1607, .level1 = 0},

    {.duration0 = 508, .level0 = 1, .duration1 = 1632, .level1 = 0},

    {.duration0 = 531, .level0 = 1, .duration1 = 1607, .level1 = 0},

    {.duration0 = 532, .level0 = 1, .duration1 = 1608, .level1 = 0},

    {.duration0 = 531, .level0 = 1, .duration1 = 1608, .level1 = 0},

    {.duration0 = 532, .level0 = 1, .duration1 = 539, .level1 = 0},

    {.duration0 = 531, .level0 = 1, .duration1 = 1606, .level1 = 0},

    {.duration0 = 508, .level0 = 1, .duration1 = 1631, .level1 = 0},

    {.duration0 = 507, .level0 = 1, .duration1 = 564, .level1 = 0},

    {.duration0 = 507, .level0 = 1, .duration1 = 1630, .level1 = 0},

    {.duration0 = 508, .level0 = 1, .duration1 = 563, .level1 = 0},

    {.duration0 = 506, .level0 = 1, .duration1 = 1611, .level1 = 0},

    {.duration0 = 552, .level0 = 1, .duration1 = 1585, .level1 = 0},

    {.duration0 = 530, .level0 = 1, .duration1 = 5182, .level1 = 0},

    {.duration0 = 4366, .level0 = 1, .duration1 = 4372, .level1 = 0},

    {.duration0 = 531, .level0 = 1, .duration1 = 565, .level1 = 0},

    {.duration0 = 506, .level0 = 1, .duration1 = 1607, .level1 = 0},

    {.duration0 = 532, .level0 = 1, .duration1 = 563, .level1 = 0},

    {.duration0 = 505, .level0 = 1, .duration1 = 1608, .level1 = 0},

    {.duration0 = 532, .level0 = 1, .duration1 = 1608, .level1 = 0},

    {.duration0 = 532, .level0 = 1, .duration1 = 1606, .level1 = 0},

    {.duration0 = 531, .level0 = 1, .duration1 = 1608, .level1 = 0},

    {.duration0 = 532, .level0 = 1, .duration1 = 541, .level1 = 0},

    {.duration0 = 528, .level0 = 1, .duration1 = 540, .level1 = 0},

    {.duration0 = 530, .level0 = 1, .duration1 = 1607, .level1 = 0},

    {.duration0 = 530, .level0 = 1, .duration1 = 1609, .level1 = 0},

    {.duration0 = 531, .level0 = 1, .duration1 = 1608, .level1 = 0},

    {.duration0 = 531, .level0 = 1, .duration1 = 1608, .level1 = 0},

    {.duration0 = 530, .level0 = 1, .duration1 = 1609, .level1 = 0},

    {.duration0 = 531, .level0 = 1, .duration1 = 538, .level1 = 0},

    {.duration0 = 531, .level0 = 1, .duration1 = 1608, .level1 = 0},

    {.duration0 = 530, .level0 = 1, .duration1 = 1609, .level1 = 0},

    {.duration0 = 531, .level0 = 1, .duration1 = 539, .level1 = 0},

    {.duration0 = 530, .level0 = 1, .duration1 = 539, .level1 = 0},

    {.duration0 = 531, .level0 = 1, .duration1 = 1608, .level1 = 0},

    {.duration0 = 530, .level0 = 1, .duration1 = 1609, .level1 = 0},

    {.duration0 = 529, .level0 = 1, .duration1 = 541, .level1 = 0},

    {.duration0 = 529, .level0 = 1, .duration1 = 540, .level1 = 0},

    {.duration0 = 529, .level0 = 1, .duration1 = 1610, .level1 = 0},

    {.duration0 = 529, .level0 = 1, .duration1 = 541, .level1 = 0},

    {.duration0 = 529, .level0 = 1, .duration1 = 540, .level1 = 0},

    {.duration0 = 529, .level0 = 1, .duration1 = 540, .level1 = 0},

    {.duration0 = 529, .level0 = 1, .duration1 = 541, .level1 = 0},

    {.duration0 = 529, .level0 = 1, .duration1 = 541, .level1 = 0},

    {.duration0 = 528, .level0 = 1, .duration1 = 541, .level1 = 0},

    {.duration0 = 529, .level0 = 1, .duration1 = 541, .level1 = 0},

    {.duration0 = 528, .level0 = 1, .duration1 = 541, .level1 = 0},

    {.duration0 = 528, .level0 = 1, .duration1 = 542, .level1 = 0},

    {.duration0 = 527, .level0 = 1, .duration1 = 542, .level1 = 0},

    {.duration0 = 528, .level0 = 1, .duration1 = 542, .level1 = 0},

    {.duration0 = 527, .level0 = 1, .duration1 = 542, .level1 = 0},

    {.duration0 = 528, .level0 = 1, .duration1 = 542, .level1 = 0},

    {.duration0 = 527, .level0 = 1, .duration1 = 542, .level1 = 0},

    {.duration0 = 527, .level0 = 1, .duration1 = 543, .level1 = 0},

    {.duration0 = 527, .level0 = 1, .duration1 = 542, .level1 = 0},

    {.duration0 = 527, .level0 = 1, .duration1 = 1612, .level1 = 0},

    {.duration0 = 528, .level0 = 1, .duration1 = 543, .level1 = 0},

    {.duration0 = 526, .level0 = 1, .duration1 = 543, .level1 = 0},

    {.duration0 = 526, .level0 = 1, .duration1 = 1613, .level1 = 0},

    {.duration0 = 526, .level0 = 1, .duration1 = 544, .level1 = 0},

    {.duration0 = 526, .level0 = 1, .duration1 = 1613, .level1 = 0},

    {.duration0 = 525, .level0 = 1, .duration1 = 545, .level1 = 0},

    {.duration0 = 525, .level0 = 1, .duration1 = 545, .level1 = 0},

    {.duration0 = 524, .level0 = 1, .duration1 = 20000, .level1 = 0},

};
static const rmt_symbol_word_t signal_off[] = {

    {.duration0 = 4371, .level0 = 1, .duration1 = 4369, .level1 = 0},

    {.duration0 = 538, .level0 = 1, .duration1 = 1605, .level1 = 0},

    {.duration0 = 536, .level0 = 1, .duration1 = 560, .level1 = 0},

    {.duration0 = 509, .level0 = 1, .duration1 = 1633, .level1 = 0},

    {.duration0 = 506, .level0 = 1, .duration1 = 564, .level1 = 0},

    {.duration0 = 531, .level0 = 1, .duration1 = 539, .level1 = 0},

    {.duration0 = 505, .level0 = 1, .duration1 = 564, .level1 = 0},

    {.duration0 = 508, .level0 = 1, .duration1 = 563, .level1 = 0},

    {.duration0 = 507, .level0 = 1, .duration1 = 1607, .level1 = 0},

    {.duration0 = 532, .level0 = 1, .duration1 = 563, .level1 = 0},

    {.duration0 = 508, .level0 = 1, .duration1 = 563, .level1 = 0},

    {.duration0 = 507, .level0 = 1, .duration1 = 563, .level1 = 0},

    {.duration0 = 507, .level0 = 1, .duration1 = 563, .level1 = 0},

    {.duration0 = 507, .level0 = 1, .duration1 = 563, .level1 = 0},

    {.duration0 = 506, .level0 = 1, .duration1 = 564, .level1 = 0},

    {.duration0 = 506, .level0 = 1, .duration1 = 1634, .level1 = 0},

    {.duration0 = 507, .level0 = 1, .duration1 = 563, .level1 = 0},

    {.duration0 = 507, .level0 = 1, .duration1 = 562, .level1 = 0},

    {.duration0 = 507, .level0 = 1, .duration1 = 1609, .level1 = 0},

    {.duration0 = 531, .level0 = 1, .duration1 = 1611, .level1 = 0},

    {.duration0 = 529, .level0 = 1, .duration1 = 564, .level1 = 0},

    {.duration0 = 506, .level0 = 1, .duration1 = 563, .level1 = 0},

    {.duration0 = 507, .level0 = 1, .duration1 = 1633, .level1 = 0},

    {.duration0 = 507, .level0 = 1, .duration1 = 1633, .level1 = 0},

    {.duration0 = 506, .level0 = 1, .duration1 = 563, .level1 = 0},

    {.duration0 = 513, .level0 = 1, .duration1 = 1628, .level1 = 0},

    {.duration0 = 505, .level0 = 1, .duration1 = 1634, .level1 = 0},

    {.duration0 = 508, .level0 = 1, .duration1 = 1631, .level1 = 0},

    {.duration0 = 507, .level0 = 1, .duration1 = 1633, .level1 = 0},

    {.duration0 = 508, .level0 = 1, .duration1 = 1633, .level1 = 0},

    {.duration0 = 506, .level0 = 1, .duration1 = 1610, .level1 = 0},

    {.duration0 = 530, .level0 = 1, .duration1 = 1608, .level1 = 0},

    {.duration0 = 532, .level0 = 1, .duration1 = 1606, .level1 = 0},

    {.duration0 = 533, .level0 = 1, .duration1 = 1606, .level1 = 0},

    {.duration0 = 534, .level0 = 1, .duration1 = 1605, .level1 = 0},

    {.duration0 = 534, .level0 = 1, .duration1 = 1606, .level1 = 0},

    {.duration0 = 534, .level0 = 1, .duration1 = 1606, .level1 = 0},

    {.duration0 = 533, .level0 = 1, .duration1 = 1604, .level1 = 0},

    {.duration0 = 535, .level0 = 1, .duration1 = 1606, .level1 = 0},

    {.duration0 = 533, .level0 = 1, .duration1 = 1606, .level1 = 0},

    {.duration0 = 535, .level0 = 1, .duration1 = 1603, .level1 = 0},

    {.duration0 = 537, .level0 = 1, .duration1 = 1603, .level1 = 0},

    {.duration0 = 537, .level0 = 1, .duration1 = 1603, .level1 = 0},

    {.duration0 = 535, .level0 = 1, .duration1 = 1603, .level1 = 0},

    {.duration0 = 534, .level0 = 1, .duration1 = 537, .level1 = 0},

    {.duration0 = 535, .level0 = 1, .duration1 = 1605, .level1 = 0},

    {.duration0 = 534, .level0 = 1, .duration1 = 535, .level1 = 0},

    {.duration0 = 533, .level0 = 1, .duration1 = 1606, .level1 = 0},

    {.duration0 = 532, .level0 = 1, .duration1 = 1609, .level1 = 0},

    {.duration0 = 530, .level0 = 1, .duration1 = 5181, .level1 = 0},

    {.duration0 = 4364, .level0 = 1, .duration1 = 4374, .level1 = 0},

    {.duration0 = 529, .level0 = 1, .duration1 = 542, .level1 = 0},

    {.duration0 = 530, .level0 = 1, .duration1 = 1608, .level1 = 0},

    {.duration0 = 532, .level0 = 1, .duration1 = 537, .level1 = 0},

    {.duration0 = 531, .level0 = 1, .duration1 = 1609, .level1 = 0},

    {.duration0 = 532, .level0 = 1, .duration1 = 1607, .level1 = 0},

    {.duration0 = 528, .level0 = 1, .duration1 = 1611, .level1 = 0},

    {.duration0 = 528, .level0 = 1, .duration1 = 1610, .level1 = 0},

    {.duration0 = 529, .level0 = 1, .duration1 = 541, .level1 = 0},

    {.duration0 = 529, .level0 = 1, .duration1 = 1610, .level1 = 0},

    {.duration0 = 530, .level0 = 1, .duration1 = 1609, .level1 = 0},

    {.duration0 = 531, .level0 = 1, .duration1 = 1609, .level1 = 0},

    {.duration0 = 528, .level0 = 1, .duration1 = 1610, .level1 = 0},

    {.duration0 = 531, .level0 = 1, .duration1 = 1609, .level1 = 0},

    {.duration0 = 528, .level0 = 1, .duration1 = 1611, .level1 = 0},

    {.duration0 = 530, .level0 = 1, .duration1 = 539, .level1 = 0},

    {.duration0 = 528, .level0 = 1, .duration1 = 1610, .level1 = 0},

    {.duration0 = 528, .level0 = 1, .duration1 = 1611, .level1 = 0},

    {.duration0 = 528, .level0 = 1, .duration1 = 543, .level1 = 0},

    {.duration0 = 528, .level0 = 1, .duration1 = 541, .level1 = 0},

    {.duration0 = 528, .level0 = 1, .duration1 = 1611, .level1 = 0},

    {.duration0 = 529, .level0 = 1, .duration1 = 1610, .level1 = 0},

    {.duration0 = 528, .level0 = 1, .duration1 = 542, .level1 = 0},

    {.duration0 = 527, .level0 = 1, .duration1 = 543, .level1 = 0},

    {.duration0 = 527, .level0 = 1, .duration1 = 1612, .level1 = 0},

    {.duration0 = 528, .level0 = 1, .duration1 = 541, .level1 = 0},

    {.duration0 = 528, .level0 = 1, .duration1 = 542, .level1 = 0},

    {.duration0 = 528, .level0 = 1, .duration1 = 541, .level1 = 0},

    {.duration0 = 528, .level0 = 1, .duration1 = 542, .level1 = 0},

    {.duration0 = 527, .level0 = 1, .duration1 = 543, .level1 = 0},

    {.duration0 = 527, .level0 = 1, .duration1 = 543, .level1 = 0},

    {.duration0 = 527, .level0 = 1, .duration1 = 542, .level1 = 0},

    {.duration0 = 528, .level0 = 1, .duration1 = 542, .level1 = 0},

    {.duration0 = 527, .level0 = 1, .duration1 = 544, .level1 = 0},

    {.duration0 = 526, .level0 = 1, .duration1 = 543, .level1 = 0},

    {.duration0 = 524, .level0 = 1, .duration1 = 545, .level1 = 0},

    {.duration0 = 526, .level0 = 1, .duration1 = 543, .level1 = 0},

    {.duration0 = 527, .level0 = 1, .duration1 = 543, .level1 = 0},

    {.duration0 = 525, .level0 = 1, .duration1 = 544, .level1 = 0},

    {.duration0 = 526, .level0 = 1, .duration1 = 545, .level1 = 0},

    {.duration0 = 525, .level0 = 1, .duration1 = 544, .level1 = 0},

    {.duration0 = 526, .level0 = 1, .duration1 = 544, .level1 = 0},

    {.duration0 = 525, .level0 = 1, .duration1 = 544, .level1 = 0},

    {.duration0 = 525, .level0 = 1, .duration1 = 567, .level1 = 0},

    {.duration0 = 503, .level0 = 1, .duration1 = 1635, .level1 = 0},

    {.duration0 = 505, .level0 = 1, .duration1 = 546, .level1 = 0},

    {.duration0 = 522, .level0 = 1, .duration1 = 1615, .level1 = 0},

    {.duration0 = 524, .level0 = 1, .duration1 = 567, .level1 = 0},

    {.duration0 = 502, .level0 = 1, .duration1 = 547, .level1 = 0},

    {.duration0 = 523, .level0 = 1, .duration1 = 20000, .level1 = 0},

};

/* Capturas nuevas: ON/OFF a 70 F; maximo 86 F; minimo 62 F. */
static const rmt_symbol_word_t signal_on_new[] __attribute__((unused)) = {
    {.duration0 = 4374, .level0 = 1, .duration1 = 4372, .level1 = 0},
    {.duration0 = 535, .level0 = 1, .duration1 = 1608, .level1 = 0},
    {.duration0 = 533, .level0 = 1, .duration1 = 563, .level1 = 0},
    {.duration0 = 508, .level0 = 1, .duration1 = 1604, .level1 = 0},
    {.duration0 = 535, .level0 = 1, .duration1 = 564, .level1 = 0},
    {.duration0 = 506, .level0 = 1, .duration1 = 565, .level1 = 0},
    {.duration0 = 506, .level0 = 1, .duration1 = 564, .level1 = 0},
    {.duration0 = 507, .level0 = 1, .duration1 = 562, .level1 = 0},
    {.duration0 = 508, .level0 = 1, .duration1 = 1606, .level1 = 0},
    {.duration0 = 535, .level0 = 1, .duration1 = 1604, .level1 = 0},
    {.duration0 = 537, .level0 = 1, .duration1 = 563, .level1 = 0},
    {.duration0 = 505, .level0 = 1, .duration1 = 565, .level1 = 0},
    {.duration0 = 509, .level0 = 1, .duration1 = 537, .level1 = 0},
    {.duration0 = 531, .level0 = 1, .duration1 = 538, .level1 = 0},
    {.duration0 = 533, .level0 = 1, .duration1 = 564, .level1 = 0},
    {.duration0 = 507, .level0 = 1, .duration1 = 1608, .level1 = 0},
    {.duration0 = 532, .level0 = 1, .duration1 = 564, .level1 = 0},
    {.duration0 = 507, .level0 = 1, .duration1 = 563, .level1 = 0},
    {.duration0 = 508, .level0 = 1, .duration1 = 1605, .level1 = 0},
    {.duration0 = 536, .level0 = 1, .duration1 = 1603, .level1 = 0},
    {.duration0 = 538, .level0 = 1, .duration1 = 537, .level1 = 0},
    {.duration0 = 532, .level0 = 1, .duration1 = 1604, .level1 = 0},
    {.duration0 = 539, .level0 = 1, .duration1 = 536, .level1 = 0},
    {.duration0 = 534, .level0 = 1, .duration1 = 561, .level1 = 0},
    {.duration0 = 509, .level0 = 1, .duration1 = 536, .level1 = 0},
    {.duration0 = 536, .level0 = 1, .duration1 = 1604, .level1 = 0},
    {.duration0 = 535, .level0 = 1, .duration1 = 1605, .level1 = 0},
    {.duration0 = 533, .level0 = 1, .duration1 = 1606, .level1 = 0},
    {.duration0 = 538, .level0 = 1, .duration1 = 1604, .level1 = 0},
    {.duration0 = 536, .level0 = 1, .duration1 = 1606, .level1 = 0},
    {.duration0 = 535, .level0 = 1, .duration1 = 1605, .level1 = 0},
    {.duration0 = 537, .level0 = 1, .duration1 = 1603, .level1 = 0},
    {.duration0 = 537, .level0 = 1, .duration1 = 1604, .level1 = 0},
    {.duration0 = 537, .level0 = 1, .duration1 = 1603, .level1 = 0},
    {.duration0 = 535, .level0 = 1, .duration1 = 1606, .level1 = 0},
    {.duration0 = 537, .level0 = 1, .duration1 = 1605, .level1 = 0},
    {.duration0 = 536, .level0 = 1, .duration1 = 1604, .level1 = 0},
    {.duration0 = 536, .level0 = 1, .duration1 = 1604, .level1 = 0},
    {.duration0 = 534, .level0 = 1, .duration1 = 1608, .level1 = 0},
    {.duration0 = 534, .level0 = 1, .duration1 = 1604, .level1 = 0},
    {.duration0 = 537, .level0 = 1, .duration1 = 1604, .level1 = 0},
    {.duration0 = 537, .level0 = 1, .duration1 = 536, .level1 = 0},
    {.duration0 = 533, .level0 = 1, .duration1 = 1605, .level1 = 0},
    {.duration0 = 534, .level0 = 1, .duration1 = 1606, .level1 = 0},
    {.duration0 = 538, .level0 = 1, .duration1 = 535, .level1 = 0},
    {.duration0 = 534, .level0 = 1, .duration1 = 535, .level1 = 0},
    {.duration0 = 533, .level0 = 1, .duration1 = 1607, .level1 = 0},
    {.duration0 = 537, .level0 = 1, .duration1 = 534, .level1 = 0},
    {.duration0 = 535, .level0 = 1, .duration1 = 535, .level1 = 0},
    {.duration0 = 534, .level0 = 1, .duration1 = 5181, .level1 = 0},
    {.duration0 = 4372, .level0 = 1, .duration1 = 4370, .level1 = 0},
    {.duration0 = 536, .level0 = 1, .duration1 = 534, .level1 = 0},
    {.duration0 = 535, .level0 = 1, .duration1 = 1606, .level1 = 0},
    {.duration0 = 535, .level0 = 1, .duration1 = 535, .level1 = 0},
    {.duration0 = 536, .level0 = 1, .duration1 = 1603, .level1 = 0},
    {.duration0 = 537, .level0 = 1, .duration1 = 1604, .level1 = 0},
    {.duration0 = 535, .level0 = 1, .duration1 = 1606, .level1 = 0},
    {.duration0 = 533, .level0 = 1, .duration1 = 1604, .level1 = 0},
    {.duration0 = 537, .level0 = 1, .duration1 = 534, .level1 = 0},
    {.duration0 = 535, .level0 = 1, .duration1 = 535, .level1 = 0},
    {.duration0 = 535, .level0 = 1, .duration1 = 1605, .level1 = 0},
    {.duration0 = 536, .level0 = 1, .duration1 = 1604, .level1 = 0},
    {.duration0 = 535, .level0 = 1, .duration1 = 1603, .level1 = 0},
    {.duration0 = 537, .level0 = 1, .duration1 = 1603, .level1 = 0},
    {.duration0 = 560, .level0 = 1, .duration1 = 1580, .level1 = 0},
    {.duration0 = 537, .level0 = 1, .duration1 = 533, .level1 = 0},
    {.duration0 = 535, .level0 = 1, .duration1 = 1604, .level1 = 0},
    {.duration0 = 538, .level0 = 1, .duration1 = 1603, .level1 = 0},
    {.duration0 = 535, .level0 = 1, .duration1 = 535, .level1 = 0},
    {.duration0 = 535, .level0 = 1, .duration1 = 534, .level1 = 0},
    {.duration0 = 536, .level0 = 1, .duration1 = 1605, .level1 = 0},
    {.duration0 = 535, .level0 = 1, .duration1 = 535, .level1 = 0},
    {.duration0 = 561, .level0 = 1, .duration1 = 1578, .level1 = 0},
    {.duration0 = 536, .level0 = 1, .duration1 = 1604, .level1 = 0},
    {.duration0 = 534, .level0 = 1, .duration1 = 1606, .level1 = 0},
    {.duration0 = 535, .level0 = 1, .duration1 = 534, .level1 = 0},
    {.duration0 = 535, .level0 = 1, .duration1 = 536, .level1 = 0},
    {.duration0 = 541, .level0 = 1, .duration1 = 529, .level1 = 0},
    {.duration0 = 558, .level0 = 1, .duration1 = 511, .level1 = 0},
    {.duration0 = 534, .level0 = 1, .duration1 = 537, .level1 = 0},
    {.duration0 = 532, .level0 = 1, .duration1 = 537, .level1 = 0},
    {.duration0 = 535, .level0 = 1, .duration1 = 535, .level1 = 0},
    {.duration0 = 534, .level0 = 1, .duration1 = 536, .level1 = 0},
    {.duration0 = 533, .level0 = 1, .duration1 = 537, .level1 = 0},
    {.duration0 = 556, .level0 = 1, .duration1 = 514, .level1 = 0},
    {.duration0 = 533, .level0 = 1, .duration1 = 537, .level1 = 0},
    {.duration0 = 557, .level0 = 1, .duration1 = 512, .level1 = 0},
    {.duration0 = 534, .level0 = 1, .duration1 = 537, .level1 = 0},
    {.duration0 = 533, .level0 = 1, .duration1 = 537, .level1 = 0},
    {.duration0 = 532, .level0 = 1, .duration1 = 538, .level1 = 0},
    {.duration0 = 531, .level0 = 1, .duration1 = 538, .level1 = 0},
    {.duration0 = 533, .level0 = 1, .duration1 = 1607, .level1 = 0},
    {.duration0 = 534, .level0 = 1, .duration1 = 536, .level1 = 0},
    {.duration0 = 532, .level0 = 1, .duration1 = 538, .level1 = 0},
    {.duration0 = 532, .level0 = 1, .duration1 = 1607, .level1 = 0},
    {.duration0 = 533, .level0 = 1, .duration1 = 1607, .level1 = 0},
    {.duration0 = 533, .level0 = 1, .duration1 = 538, .level1 = 0},
    {.duration0 = 531, .level0 = 1, .duration1 = 1608, .level1 = 0},
    {.duration0 = 532, .level0 = 1, .duration1 = 1607, .level1 = 0},
    {.duration0 = 557, .level0 = 1, .duration1 = 20000, .level1 = 0},
};

static const rmt_symbol_word_t signal_off_new[] __attribute__((unused)) = {
    {.duration0 = 4399, .level0 = 1, .duration1 = 4348, .level1 = 0},
    {.duration0 = 558, .level0 = 1, .duration1 = 1610, .level1 = 0},
    {.duration0 = 531, .level0 = 1, .duration1 = 540, .level1 = 0},
    {.duration0 = 529, .level0 = 1, .duration1 = 1611, .level1 = 0},
    {.duration0 = 529, .level0 = 1, .duration1 = 541, .level1 = 0},
    {.duration0 = 530, .level0 = 1, .duration1 = 540, .level1 = 0},
    {.duration0 = 532, .level0 = 1, .duration1 = 539, .level1 = 0},
    {.duration0 = 531, .level0 = 1, .duration1 = 539, .level1 = 0},
    {.duration0 = 531, .level0 = 1, .duration1 = 1611, .level1 = 0},
    {.duration0 = 529, .level0 = 1, .duration1 = 540, .level1 = 0},
    {.duration0 = 532, .level0 = 1, .duration1 = 539, .level1 = 0},
    {.duration0 = 531, .level0 = 1, .duration1 = 539, .level1 = 0},
    {.duration0 = 531, .level0 = 1, .duration1 = 539, .level1 = 0},
    {.duration0 = 531, .level0 = 1, .duration1 = 540, .level1 = 0},
    {.duration0 = 531, .level0 = 1, .duration1 = 537, .level1 = 0},
    {.duration0 = 533, .level0 = 1, .duration1 = 1610, .level1 = 0},
    {.duration0 = 530, .level0 = 1, .duration1 = 540, .level1 = 0},
    {.duration0 = 530, .level0 = 1, .duration1 = 541, .level1 = 0},
    {.duration0 = 530, .level0 = 1, .duration1 = 1611, .level1 = 0},
    {.duration0 = 530, .level0 = 1, .duration1 = 1610, .level1 = 0},
    {.duration0 = 531, .level0 = 1, .duration1 = 540, .level1 = 0},
    {.duration0 = 530, .level0 = 1, .duration1 = 1611, .level1 = 0},
    {.duration0 = 531, .level0 = 1, .duration1 = 538, .level1 = 0},
    {.duration0 = 532, .level0 = 1, .duration1 = 539, .level1 = 0},
    {.duration0 = 531, .level0 = 1, .duration1 = 540, .level1 = 0},
    {.duration0 = 530, .level0 = 1, .duration1 = 1611, .level1 = 0},
    {.duration0 = 530, .level0 = 1, .duration1 = 1611, .level1 = 0},
    {.duration0 = 531, .level0 = 1, .duration1 = 1609, .level1 = 0},
    {.duration0 = 530, .level0 = 1, .duration1 = 1611, .level1 = 0},
    {.duration0 = 530, .level0 = 1, .duration1 = 1610, .level1 = 0},
    {.duration0 = 530, .level0 = 1, .duration1 = 1611, .level1 = 0},
    {.duration0 = 529, .level0 = 1, .duration1 = 1612, .level1 = 0},
    {.duration0 = 529, .level0 = 1, .duration1 = 1612, .level1 = 0},
    {.duration0 = 530, .level0 = 1, .duration1 = 1610, .level1 = 0},
    {.duration0 = 528, .level0 = 1, .duration1 = 1612, .level1 = 0},
    {.duration0 = 528, .level0 = 1, .duration1 = 1613, .level1 = 0},
    {.duration0 = 529, .level0 = 1, .duration1 = 1612, .level1 = 0},
    {.duration0 = 532, .level0 = 1, .duration1 = 1608, .level1 = 0},
    {.duration0 = 531, .level0 = 1, .duration1 = 1610, .level1 = 0},
    {.duration0 = 531, .level0 = 1, .duration1 = 1609, .level1 = 0},
    {.duration0 = 507, .level0 = 1, .duration1 = 1634, .level1 = 0},
    {.duration0 = 529, .level0 = 1, .duration1 = 1611, .level1 = 0},
    {.duration0 = 530, .level0 = 1, .duration1 = 1610, .level1 = 0},
    {.duration0 = 505, .level0 = 1, .duration1 = 1636, .level1 = 0},
    {.duration0 = 530, .level0 = 1, .duration1 = 540, .level1 = 0},
    {.duration0 = 531, .level0 = 1, .duration1 = 538, .level1 = 0},
    {.duration0 = 531, .level0 = 1, .duration1 = 1610, .level1 = 0},
    {.duration0 = 505, .level0 = 1, .duration1 = 565, .level1 = 0},
    {.duration0 = 531, .level0 = 1, .duration1 = 539, .level1 = 0},
    {.duration0 = 531, .level0 = 1, .duration1 = 5158, .level1 = 0},
    {.duration0 = 4368, .level0 = 1, .duration1 = 4375, .level1 = 0},
    {.duration0 = 556, .level0 = 1, .duration1 = 540, .level1 = 0},
    {.duration0 = 532, .level0 = 1, .duration1 = 1609, .level1 = 0},
    {.duration0 = 529, .level0 = 1, .duration1 = 541, .level1 = 0},
    {.duration0 = 529, .level0 = 1, .duration1 = 1611, .level1 = 0},
    {.duration0 = 528, .level0 = 1, .duration1 = 1612, .level1 = 0},
    {.duration0 = 528, .level0 = 1, .duration1 = 1611, .level1 = 0},
    {.duration0 = 530, .level0 = 1, .duration1 = 1610, .level1 = 0},
    {.duration0 = 531, .level0 = 1, .duration1 = 539, .level1 = 0},
    {.duration0 = 529, .level0 = 1, .duration1 = 1611, .level1 = 0},
    {.duration0 = 530, .level0 = 1, .duration1 = 1610, .level1 = 0},
    {.duration0 = 530, .level0 = 1, .duration1 = 1610, .level1 = 0},
    {.duration0 = 506, .level0 = 1, .duration1 = 1634, .level1 = 0},
    {.duration0 = 505, .level0 = 1, .duration1 = 1634, .level1 = 0},
    {.duration0 = 505, .level0 = 1, .duration1 = 1635, .level1 = 0},
    {.duration0 = 531, .level0 = 1, .duration1 = 539, .level1 = 0},
    {.duration0 = 531, .level0 = 1, .duration1 = 1608, .level1 = 0},
    {.duration0 = 530, .level0 = 1, .duration1 = 1610, .level1 = 0},
    {.duration0 = 530, .level0 = 1, .duration1 = 541, .level1 = 0},
    {.duration0 = 506, .level0 = 1, .duration1 = 564, .level1 = 0},
    {.duration0 = 505, .level0 = 1, .duration1 = 1633, .level1 = 0},
    {.duration0 = 507, .level0 = 1, .duration1 = 564, .level1 = 0},
    {.duration0 = 507, .level0 = 1, .duration1 = 1632, .level1 = 0},
    {.duration0 = 506, .level0 = 1, .duration1 = 1634, .level1 = 0},
    {.duration0 = 530, .level0 = 1, .duration1 = 1610, .level1 = 0},
    {.duration0 = 507, .level0 = 1, .duration1 = 564, .level1 = 0},
    {.duration0 = 505, .level0 = 1, .duration1 = 564, .level1 = 0},
    {.duration0 = 507, .level0 = 1, .duration1 = 563, .level1 = 0},
    {.duration0 = 506, .level0 = 1, .duration1 = 564, .level1 = 0},
    {.duration0 = 506, .level0 = 1, .duration1 = 564, .level1 = 0},
    {.duration0 = 527, .level0 = 1, .duration1 = 543, .level1 = 0},
    {.duration0 = 506, .level0 = 1, .duration1 = 564, .level1 = 0},
    {.duration0 = 505, .level0 = 1, .duration1 = 565, .level1 = 0},
    {.duration0 = 506, .level0 = 1, .duration1 = 563, .level1 = 0},
    {.duration0 = 506, .level0 = 1, .duration1 = 564, .level1 = 0},
    {.duration0 = 506, .level0 = 1, .duration1 = 565, .level1 = 0},
    {.duration0 = 505, .level0 = 1, .duration1 = 565, .level1 = 0},
    {.duration0 = 505, .level0 = 1, .duration1 = 565, .level1 = 0},
    {.duration0 = 504, .level0 = 1, .duration1 = 565, .level1 = 0},
    {.duration0 = 505, .level0 = 1, .duration1 = 565, .level1 = 0},
    {.duration0 = 505, .level0 = 1, .duration1 = 565, .level1 = 0},
    {.duration0 = 504, .level0 = 1, .duration1 = 566, .level1 = 0},
    {.duration0 = 504, .level0 = 1, .duration1 = 566, .level1 = 0},
    {.duration0 = 504, .level0 = 1, .duration1 = 566, .level1 = 0},
    {.duration0 = 504, .level0 = 1, .duration1 = 1609, .level1 = 0},
    {.duration0 = 530, .level0 = 1, .duration1 = 1609, .level1 = 0},
    {.duration0 = 532, .level0 = 1, .duration1 = 565, .level1 = 0},
    {.duration0 = 505, .level0 = 1, .duration1 = 1608, .level1 = 0},
    {.duration0 = 530, .level0 = 1, .duration1 = 1610, .level1 = 0},
    {.duration0 = 530, .level0 = 1, .duration1 = 20000, .level1 = 0},
};

static const rmt_symbol_word_t signal_temp_down[] = {
    {.duration0 = 4373, .level0 = 1, .duration1 = 4370, .level1 = 0},
    {.duration0 = 530, .level0 = 1, .duration1 = 1608, .level1 = 0},
    {.duration0 = 537, .level0 = 1, .duration1 = 533, .level1 = 0},
    {.duration0 = 534, .level0 = 1, .duration1 = 1607, .level1 = 0},
    {.duration0 = 508, .level0 = 1, .duration1 = 562, .level1 = 0},
    {.duration0 = 534, .level0 = 1, .duration1 = 537, .level1 = 0},
    {.duration0 = 532, .level0 = 1, .duration1 = 536, .level1 = 0},
    {.duration0 = 536, .level0 = 1, .duration1 = 535, .level1 = 0},
    {.duration0 = 531, .level0 = 1, .duration1 = 1608, .level1 = 0},
    {.duration0 = 536, .level0 = 1, .duration1 = 1605, .level1 = 0},
    {.duration0 = 530, .level0 = 1, .duration1 = 538, .level1 = 0},
    {.duration0 = 563, .level0 = 1, .duration1 = 507, .level1 = 0},
    {.duration0 = 535, .level0 = 1, .duration1 = 538, .level1 = 0},
    {.duration0 = 530, .level0 = 1, .duration1 = 539, .level1 = 0},
    {.duration0 = 533, .level0 = 1, .duration1 = 535, .level1 = 0},
    {.duration0 = 532, .level0 = 1, .duration1 = 1608, .level1 = 0},
    {.duration0 = 537, .level0 = 1, .duration1 = 534, .level1 = 0},
    {.duration0 = 533, .level0 = 1, .duration1 = 538, .level1 = 0},
    {.duration0 = 533, .level0 = 1, .duration1 = 1606, .level1 = 0},
    {.duration0 = 532, .level0 = 1, .duration1 = 1607, .level1 = 0},
    {.duration0 = 535, .level0 = 1, .duration1 = 536, .level1 = 0},
    {.duration0 = 534, .level0 = 1, .duration1 = 536, .level1 = 0},
    {.duration0 = 533, .level0 = 1, .duration1 = 537, .level1 = 0},
    {.duration0 = 531, .level0 = 1, .duration1 = 539, .level1 = 0},
    {.duration0 = 534, .level0 = 1, .duration1 = 535, .level1 = 0},
    {.duration0 = 531, .level0 = 1, .duration1 = 1609, .level1 = 0},
    {.duration0 = 533, .level0 = 1, .duration1 = 1607, .level1 = 0},
    {.duration0 = 531, .level0 = 1, .duration1 = 1609, .level1 = 0},
    {.duration0 = 532, .level0 = 1, .duration1 = 1607, .level1 = 0},
    {.duration0 = 533, .level0 = 1, .duration1 = 1608, .level1 = 0},
    {.duration0 = 531, .level0 = 1, .duration1 = 1608, .level1 = 0},
    {.duration0 = 532, .level0 = 1, .duration1 = 1610, .level1 = 0},
    {.duration0 = 529, .level0 = 1, .duration1 = 1609, .level1 = 0},
    {.duration0 = 532, .level0 = 1, .duration1 = 1608, .level1 = 0},
    {.duration0 = 530, .level0 = 1, .duration1 = 1610, .level1 = 0},
    {.duration0 = 528, .level0 = 1, .duration1 = 1613, .level1 = 0},
    {.duration0 = 528, .level0 = 1, .duration1 = 1610, .level1 = 0},
    {.duration0 = 531, .level0 = 1, .duration1 = 1609, .level1 = 0},
    {.duration0 = 530, .level0 = 1, .duration1 = 1611, .level1 = 0},
    {.duration0 = 530, .level0 = 1, .duration1 = 1609, .level1 = 0},
    {.duration0 = 532, .level0 = 1, .duration1 = 1608, .level1 = 0},
    {.duration0 = 530, .level0 = 1, .duration1 = 539, .level1 = 0},
    {.duration0 = 531, .level0 = 1, .duration1 = 1610, .level1 = 0},
    {.duration0 = 529, .level0 = 1, .duration1 = 1612, .level1 = 0},
    {.duration0 = 529, .level0 = 1, .duration1 = 540, .level1 = 0},
    {.duration0 = 530, .level0 = 1, .duration1 = 1610, .level1 = 0},
    {.duration0 = 530, .level0 = 1, .duration1 = 1609, .level1 = 0},
    {.duration0 = 531, .level0 = 1, .duration1 = 540, .level1 = 0},
    {.duration0 = 530, .level0 = 1, .duration1 = 539, .level1 = 0},
    {.duration0 = 531, .level0 = 1, .duration1 = 5184, .level1 = 0},
    {.duration0 = 4366, .level0 = 1, .duration1 = 4376, .level1 = 0},
    {.duration0 = 529, .level0 = 1, .duration1 = 542, .level1 = 0},
    {.duration0 = 528, .level0 = 1, .duration1 = 1611, .level1 = 0},
    {.duration0 = 528, .level0 = 1, .duration1 = 542, .level1 = 0},
    {.duration0 = 529, .level0 = 1, .duration1 = 1611, .level1 = 0},
    {.duration0 = 527, .level0 = 1, .duration1 = 1613, .level1 = 0},
    {.duration0 = 528, .level0 = 1, .duration1 = 1612, .level1 = 0},
    {.duration0 = 528, .level0 = 1, .duration1 = 1612, .level1 = 0},
    {.duration0 = 528, .level0 = 1, .duration1 = 542, .level1 = 0},
    {.duration0 = 529, .level0 = 1, .duration1 = 540, .level1 = 0},
    {.duration0 = 529, .level0 = 1, .duration1 = 1611, .level1 = 0},
    {.duration0 = 529, .level0 = 1, .duration1 = 1610, .level1 = 0},
    {.duration0 = 530, .level0 = 1, .duration1 = 1611, .level1 = 0},
    {.duration0 = 529, .level0 = 1, .duration1 = 1611, .level1 = 0},
    {.duration0 = 529, .level0 = 1, .duration1 = 1610, .level1 = 0},
    {.duration0 = 529, .level0 = 1, .duration1 = 541, .level1 = 0},
    {.duration0 = 529, .level0 = 1, .duration1 = 1611, .level1 = 0},
    {.duration0 = 529, .level0 = 1, .duration1 = 1611, .level1 = 0},
    {.duration0 = 529, .level0 = 1, .duration1 = 541, .level1 = 0},
    {.duration0 = 530, .level0 = 1, .duration1 = 540, .level1 = 0},
    {.duration0 = 529, .level0 = 1, .duration1 = 1612, .level1 = 0},
    {.duration0 = 527, .level0 = 1, .duration1 = 1612, .level1 = 0},
    {.duration0 = 528, .level0 = 1, .duration1 = 1612, .level1 = 0},
    {.duration0 = 529, .level0 = 1, .duration1 = 1611, .level1 = 0},
    {.duration0 = 528, .level0 = 1, .duration1 = 1611, .level1 = 0},
    {.duration0 = 529, .level0 = 1, .duration1 = 542, .level1 = 0},
    {.duration0 = 529, .level0 = 1, .duration1 = 540, .level1 = 0},
    {.duration0 = 529, .level0 = 1, .duration1 = 541, .level1 = 0},
    {.duration0 = 529, .level0 = 1, .duration1 = 541, .level1 = 0},
    {.duration0 = 529, .level0 = 1, .duration1 = 541, .level1 = 0},
    {.duration0 = 529, .level0 = 1, .duration1 = 542, .level1 = 0},
    {.duration0 = 529, .level0 = 1, .duration1 = 540, .level1 = 0},
    {.duration0 = 528, .level0 = 1, .duration1 = 542, .level1 = 0},
    {.duration0 = 528, .level0 = 1, .duration1 = 542, .level1 = 0},
    {.duration0 = 528, .level0 = 1, .duration1 = 543, .level1 = 0},
    {.duration0 = 527, .level0 = 1, .duration1 = 541, .level1 = 0},
    {.duration0 = 530, .level0 = 1, .duration1 = 541, .level1 = 0},
    {.duration0 = 527, .level0 = 1, .duration1 = 543, .level1 = 0},
    {.duration0 = 527, .level0 = 1, .duration1 = 543, .level1 = 0},
    {.duration0 = 528, .level0 = 1, .duration1 = 542, .level1 = 0},
    {.duration0 = 528, .level0 = 1, .duration1 = 542, .level1 = 0},
    {.duration0 = 527, .level0 = 1, .duration1 = 1613, .level1 = 0},
    {.duration0 = 527, .level0 = 1, .duration1 = 566, .level1 = 0},
    {.duration0 = 503, .level0 = 1, .duration1 = 546, .level1 = 0},
    {.duration0 = 524, .level0 = 1, .duration1 = 1636, .level1 = 0},
    {.duration0 = 504, .level0 = 1, .duration1 = 567, .level1 = 0},
    {.duration0 = 502, .level0 = 1, .duration1 = 568, .level1 = 0},
    {.duration0 = 502, .level0 = 1, .duration1 = 1638, .level1 = 0},
    {.duration0 = 501, .level0 = 1, .duration1 = 1638, .level1 = 0},
    {.duration0 = 501, .level0 = 1, .duration1 = 20000, .level1 = 0},
};

static const rmt_symbol_word_t signal_temp_up[] = {
    {.duration0 = 4396, .level0 = 1, .duration1 = 4347, .level1 = 0},
    {.duration0 = 559, .level0 = 1, .duration1 = 1609, .level1 = 0},
    {.duration0 = 531, .level0 = 1, .duration1 = 538, .level1 = 0},
    {.duration0 = 531, .level0 = 1, .duration1 = 1610, .level1 = 0},
    {.duration0 = 530, .level0 = 1, .duration1 = 539, .level1 = 0},
    {.duration0 = 532, .level0 = 1, .duration1 = 538, .level1 = 0},
    {.duration0 = 532, .level0 = 1, .duration1 = 539, .level1 = 0},
    {.duration0 = 531, .level0 = 1, .duration1 = 539, .level1 = 0},
    {.duration0 = 530, .level0 = 1, .duration1 = 1609, .level1 = 0},
    {.duration0 = 532, .level0 = 1, .duration1 = 1609, .level1 = 0},
    {.duration0 = 531, .level0 = 1, .duration1 = 539, .level1 = 0},
    {.duration0 = 531, .level0 = 1, .duration1 = 538, .level1 = 0},
    {.duration0 = 532, .level0 = 1, .duration1 = 539, .level1 = 0},
    {.duration0 = 531, .level0 = 1, .duration1 = 539, .level1 = 0},
    {.duration0 = 531, .level0 = 1, .duration1 = 539, .level1 = 0},
    {.duration0 = 530, .level0 = 1, .duration1 = 1610, .level1 = 0},
    {.duration0 = 531, .level0 = 1, .duration1 = 538, .level1 = 0},
    {.duration0 = 532, .level0 = 1, .duration1 = 539, .level1 = 0},
    {.duration0 = 530, .level0 = 1, .duration1 = 1610, .level1 = 0},
    {.duration0 = 531, .level0 = 1, .duration1 = 1609, .level1 = 0},
    {.duration0 = 531, .level0 = 1, .duration1 = 1609, .level1 = 0},
    {.duration0 = 531, .level0 = 1, .duration1 = 1609, .level1 = 0},
    {.duration0 = 531, .level0 = 1, .duration1 = 538, .level1 = 0},
    {.duration0 = 532, .level0 = 1, .duration1 = 538, .level1 = 0},
    {.duration0 = 532, .level0 = 1, .duration1 = 539, .level1 = 0},
    {.duration0 = 531, .level0 = 1, .duration1 = 1608, .level1 = 0},
    {.duration0 = 532, .level0 = 1, .duration1 = 1609, .level1 = 0},
    {.duration0 = 531, .level0 = 1, .duration1 = 1609, .level1 = 0},
    {.duration0 = 530, .level0 = 1, .duration1 = 1610, .level1 = 0},
    {.duration0 = 531, .level0 = 1, .duration1 = 1610, .level1 = 0},
    {.duration0 = 530, .level0 = 1, .duration1 = 1609, .level1 = 0},
    {.duration0 = 531, .level0 = 1, .duration1 = 1608, .level1 = 0},
    {.duration0 = 531, .level0 = 1, .duration1 = 1610, .level1 = 0},
    {.duration0 = 531, .level0 = 1, .duration1 = 1609, .level1 = 0},
    {.duration0 = 531, .level0 = 1, .duration1 = 1609, .level1 = 0},
    {.duration0 = 531, .level0 = 1, .duration1 = 1609, .level1 = 0},
    {.duration0 = 530, .level0 = 1, .duration1 = 1610, .level1 = 0},
    {.duration0 = 531, .level0 = 1, .duration1 = 1609, .level1 = 0},
    {.duration0 = 530, .level0 = 1, .duration1 = 1610, .level1 = 0},
    {.duration0 = 531, .level0 = 1, .duration1 = 1609, .level1 = 0},
    {.duration0 = 531, .level0 = 1, .duration1 = 1609, .level1 = 0},
    {.duration0 = 531, .level0 = 1, .duration1 = 538, .level1 = 0},
    {.duration0 = 532, .level0 = 1, .duration1 = 1609, .level1 = 0},
    {.duration0 = 506, .level0 = 1, .duration1 = 1633, .level1 = 0},
    {.duration0 = 530, .level0 = 1, .duration1 = 1610, .level1 = 0},
    {.duration0 = 531, .level0 = 1, .duration1 = 1609, .level1 = 0},
    {.duration0 = 531, .level0 = 1, .duration1 = 539, .level1 = 0},
    {.duration0 = 531, .level0 = 1, .duration1 = 539, .level1 = 0},
    {.duration0 = 531, .level0 = 1, .duration1 = 539, .level1 = 0},
    {.duration0 = 530, .level0 = 1, .duration1 = 5157, .level1 = 0},
    {.duration0 = 4396, .level0 = 1, .duration1 = 4348, .level1 = 0},
    {.duration0 = 557, .level0 = 1, .duration1 = 539, .level1 = 0},
    {.duration0 = 531, .level0 = 1, .duration1 = 1610, .level1 = 0},
    {.duration0 = 528, .level0 = 1, .duration1 = 541, .level1 = 0},
    {.duration0 = 508, .level0 = 1, .duration1 = 1633, .level1 = 0},
    {.duration0 = 507, .level0 = 1, .duration1 = 1632, .level1 = 0},
    {.duration0 = 513, .level0 = 1, .duration1 = 1627, .level1 = 0},
    {.duration0 = 507, .level0 = 1, .duration1 = 1633, .level1 = 0},
    {.duration0 = 507, .level0 = 1, .duration1 = 563, .level1 = 0},
    {.duration0 = 507, .level0 = 1, .duration1 = 563, .level1 = 0},
    {.duration0 = 508, .level0 = 1, .duration1 = 1632, .level1 = 0},
    {.duration0 = 508, .level0 = 1, .duration1 = 1632, .level1 = 0},
    {.duration0 = 508, .level0 = 1, .duration1 = 1632, .level1 = 0},
    {.duration0 = 507, .level0 = 1, .duration1 = 1632, .level1 = 0},
    {.duration0 = 531, .level0 = 1, .duration1 = 1609, .level1 = 0},
    {.duration0 = 508, .level0 = 1, .duration1 = 562, .level1 = 0},
    {.duration0 = 531, .level0 = 1, .duration1 = 1609, .level1 = 0},
    {.duration0 = 507, .level0 = 1, .duration1 = 1633, .level1 = 0},
    {.duration0 = 508, .level0 = 1, .duration1 = 562, .level1 = 0},
    {.duration0 = 507, .level0 = 1, .duration1 = 563, .level1 = 0},
    {.duration0 = 507, .level0 = 1, .duration1 = 563, .level1 = 0},
    {.duration0 = 507, .level0 = 1, .duration1 = 563, .level1 = 0},
    {.duration0 = 507, .level0 = 1, .duration1 = 1633, .level1 = 0},
    {.duration0 = 507, .level0 = 1, .duration1 = 1632, .level1 = 0},
    {.duration0 = 508, .level0 = 1, .duration1 = 1633, .level1 = 0},
    {.duration0 = 507, .level0 = 1, .duration1 = 563, .level1 = 0},
    {.duration0 = 507, .level0 = 1, .duration1 = 563, .level1 = 0},
    {.duration0 = 507, .level0 = 1, .duration1 = 563, .level1 = 0},
    {.duration0 = 508, .level0 = 1, .duration1 = 562, .level1 = 0},
    {.duration0 = 507, .level0 = 1, .duration1 = 563, .level1 = 0},
    {.duration0 = 507, .level0 = 1, .duration1 = 564, .level1 = 0},
    {.duration0 = 507, .level0 = 1, .duration1 = 563, .level1 = 0},
    {.duration0 = 507, .level0 = 1, .duration1 = 563, .level1 = 0},
    {.duration0 = 506, .level0 = 1, .duration1 = 564, .level1 = 0},
    {.duration0 = 507, .level0 = 1, .duration1 = 563, .level1 = 0},
    {.duration0 = 506, .level0 = 1, .duration1 = 564, .level1 = 0},
    {.duration0 = 506, .level0 = 1, .duration1 = 563, .level1 = 0},
    {.duration0 = 507, .level0 = 1, .duration1 = 564, .level1 = 0},
    {.duration0 = 507, .level0 = 1, .duration1 = 562, .level1 = 0},
    {.duration0 = 507, .level0 = 1, .duration1 = 564, .level1 = 0},
    {.duration0 = 529, .level0 = 1, .duration1 = 517, .level1 = 0},
    {.duration0 = 530, .level0 = 1, .duration1 = 1606, .level1 = 0},
    {.duration0 = 534, .level0 = 1, .duration1 = 563, .level1 = 0},
    {.duration0 = 507, .level0 = 1, .duration1 = 539, .level1 = 0},
    {.duration0 = 531, .level0 = 1, .duration1 = 539, .level1 = 0},
    {.duration0 = 530, .level0 = 1, .duration1 = 540, .level1 = 0},
    {.duration0 = 530, .level0 = 1, .duration1 = 1606, .level1 = 0},
    {.duration0 = 557, .level0 = 1, .duration1 = 1584, .level1 = 0},
    {.duration0 = 533, .level0 = 1, .duration1 = 1607, .level1 = 0},
    {.duration0 = 533, .level0 = 1, .duration1 = 20000, .level1 = 0},
};

static bool ac_is_on = false;

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
            printf("Boton TEMP + detectado\n");
            send_signal(signal_temp_up, sizeof(signal_temp_up) / sizeof(signal_temp_up[0]), "TEMP +");
        } else if (button_pressed(BUTTON_DOWN_GPIO)) {
            printf("Boton TEMP - detectado\n");
            send_signal(signal_temp_down, sizeof(signal_temp_down) / sizeof(signal_temp_down[0]), "TEMP -");
        } else if (button_pressed(BUTTON_POWER_GPIO)) {
            ac_is_on = !ac_is_on;
            printf("Boton POWER detectado; nuevo estado: %s\n",
                   ac_is_on ? "ON" : "OFF");
            if (ac_is_on) {
                send_signal(signal_on, sizeof(signal_on) / sizeof(signal_on[0]), "POWER ON");
            } else {
                send_signal(signal_off, sizeof(signal_off) / sizeof(signal_off[0]), "POWER OFF");
            }
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
