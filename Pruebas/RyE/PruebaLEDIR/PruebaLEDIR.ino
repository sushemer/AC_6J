// Prueba lenta del LED infrarrojo y del transistor.
// ESP32-C3 Super Mini: GPIO7 -> resistencia 1 kOhm -> base NPN.

const uint8_t IR_LED_GPIO = 7;

void setup() {
  pinMode(IR_LED_GPIO, OUTPUT);
  digitalWrite(IR_LED_GPIO, LOW);
  Serial.begin(115200);
  delay(1000);
  Serial.println("Prueba LED IR iniciada");
}

void loop() {
  digitalWrite(IR_LED_GPIO, HIGH);
  Serial.println("LED IR: ENCENDIDO");
  delay(1000);

  digitalWrite(IR_LED_GPIO, LOW);
  Serial.println("LED IR: APAGADO");
  delay(1000);
}
