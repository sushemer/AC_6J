/* Diagnostico minimo para ESP32-C3 Super Mini.
 * Usar con la placa sin botones, transistor ni fuente externa conectados.
 */

void setup() {
  Serial.begin(115200);
  delay(2000);
  Serial.println();
  Serial.println("=== DIAGNOSTICO ESP32-C3 ===");
  Serial.println("La placa arranco correctamente.");
  Serial.println("USB CDC y CPU funcionando.");
}

void loop() {
  Serial.print("Vivo. Tiempo ms: ");
  Serial.println(millis());
  delay(1000);
}
