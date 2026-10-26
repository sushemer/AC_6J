#include <Arduino.h>
#include <IRremoteESP8266.h>
#include <IRsend.h>

// ESP32 DevKit V1 (ESP32 clasico / WROOM-32)
// No usar GPIO6-GPIO11: estan conectados a la memoria flash.
const uint16_t IR_LED_PIN = 32;
const uint8_t BUTTON_UP = 25;
const uint8_t BUTTON_DOWN = 26;
const uint8_t BUTTON_POWER = 27;
const uint16_t IR_FREQUENCY_KHZ = 38;

IRsend irsend(IR_LED_PIN);

/*
 * Pega aqui los arreglos rawData[] que imprime IRrecvDumpV3.
 * Estos valores deben ser los del control RG574(B)/BGEFU1.
 * Los valores de abajo son solo una señal de prueba y no controlan
 * necesariamente tu aire acondicionado.
 */
static const uint16_t raw_up[] = {
  9000, 4500, 560, 560
};

static const uint16_t raw_down[] = {
  9000, 4500, 560, 560
};

static const uint16_t raw_power[] = {
  9000, 4500, 560, 560
};

bool pressed(uint8_t pin, bool &armed) {
  const bool isLow = digitalRead(pin) == LOW;
  if (isLow && armed) {
    armed = false;
    return true;
  }
  if (!isLow) armed = true;
  return false;
}

void sendRawSignal(const uint16_t *rawData, size_t length, const char *name) {
  Serial.print("Transmitiendo RAW: ");
  Serial.println(name);
  Serial.print("Duraciones: ");
  Serial.println(length);
  Serial.println("Portadora: 38 kHz");

  irsend.sendRaw(rawData, length, IR_FREQUENCY_KHZ);

  Serial.println("Transmision RAW terminada");
}

void setup() {
  Serial.begin(115200);
  delay(2000);

  pinMode(BUTTON_UP, INPUT_PULLUP);
  pinMode(BUTTON_DOWN, INPUT_PULLUP);
  pinMode(BUTTON_POWER, INPUT_PULLUP);
  irsend.begin();

  Serial.println();
  Serial.println("=== EMISOR IR RAW - ESP32 DevKit V1 ===");
  Serial.println("LED IR: GPIO32");
  Serial.println("SUBIR: GPIO25 | BAJAR: GPIO26 | POWER: GPIO27");
  Serial.println("Serial: 4=SUBIR, 5=BAJAR, 0=POWER");
  Serial.println("Reemplace los arreglos de prueba por rawData reales.");
  Serial.println("Listo.");
}

void loop() {
  static bool upArmed = true;
  static bool downArmed = true;
  static bool powerArmed = true;

  if (pressed(BUTTON_UP, upArmed)) {
    sendRawSignal(raw_up, sizeof(raw_up) / sizeof(raw_up[0]), "SUBIR");
  } else if (pressed(BUTTON_DOWN, downArmed)) {
    sendRawSignal(raw_down, sizeof(raw_down) / sizeof(raw_down[0]), "BAJAR");
  } else if (pressed(BUTTON_POWER, powerArmed)) {
    sendRawSignal(raw_power, sizeof(raw_power) / sizeof(raw_power[0]), "POWER");
  }

  while (Serial.available()) {
    const char command = Serial.read();
    if (command == '4') {
      sendRawSignal(raw_up, sizeof(raw_up) / sizeof(raw_up[0]), "SUBIR");
    } else if (command == '5') {
      sendRawSignal(raw_down, sizeof(raw_down) / sizeof(raw_down[0]), "BAJAR");
    } else if (command == '0') {
      sendRawSignal(raw_power, sizeof(raw_power) / sizeof(raw_power[0]), "POWER");
    }
  }

  delay(10);
}
