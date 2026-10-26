# Prueba ESP-NOW bidireccional 11b/broadcast: C3 Super Mini ↔ DevKit V1

Esta carpeta es una variante separada de `NOW_RANGO_RESPUESTA`; no cambia el proyecto original ni la variante anterior. Se excluyeron carpetas de compilación para no reutilizar firmware antiguo. Conserva el firmware, el logger y el registro histórico `registros/prueba_201.jsonl`.

## Qué prueba

1. El C3 envía un paquete broadcast numerado al DevKit.
2. El DevKit mide RSSI y pérdida por MAC, y responde con un ACK de aplicación que contiene la misma secuencia.
3. El C3 comprueba la secuencia recibida, mide RTT (desde `esp_now_send()` hasta recibir el ACK), registra el RSSI del ACK y confirma el resultado en su siguiente paquete.
4. El DevKit registra esa confirmación. Así, con solo el USB del DevKit conectado a la laptop, el log distingue un paquete recibido por el gateway de un ACK que además regresó y fue procesado por el C3.

Los logs `ESPNOW_RX` muestran el RSSI del paquete C3→DevKit (`rssi_dbm`), el RSSI que el DevKit midió y devolvió dentro del ACK (`gateway_rx_rssi_dbm`), el RSSI de ACK recibido en el C3 (`ack_rssi_dbm`), el RTT (`ack_rtt_us`) y el resultado reportado (`ack_state=received|timeout|tx_error`). El ACK del gateway es una respuesta de aplicación, no solo el callback MAC.

## Parámetros de la primera corrida

- Esta variante habilita únicamente 802.11b en ambos equipos. El C3 envía datos por broadcast, como la prueba V1 que funcionó; el gateway responde con un ACK de aplicación unicast al MAC de origen. El C3 fija TX en 34 cuartos de dBm (8.5 dBm), igual que la V1 funcional. Así se comprueba recepción de datos por `ESPNOW_RX` y el sentido de regreso por `ESPNOW_APP_ACK ... result=received`. El ACK —no el callback MAC del broadcast— confirma recepción de aplicación.
- Cada nodo C3 envía **un paquete por segundo**, igual que el intervalo de referencia de las pruebas anteriores. No aceleré el tráfico: así evitamos añadir colisiones como variable.
- El C3 espera hasta 300 ms por el ACK. Si no llega, la siguiente trama informa `ack_state=timeout`.
- Los IDs 1..8 tienen un desfase inicial de 40 ms para que ocho nodos arrancados juntos no transmitan a la vez; cada uno conserva su intervalo de 1 s.
- Los callbacks de Wi-Fi solo copian información a una cola; el trabajo y el envío del ACK ocurren en tareas.

## Antes de compilar y grabar

- En `espnowSender/main/espnow_example_main.c`, confirma `s_gateway_mac` contra la MAC STA que imprime el DevKit. El valor inicial es `e0:8c:fe:5d:04:ec` de los logs compartidos; si la tarjeta cambió, actualízalo.
- En cada C3, asigna un `ESPNOW_NODE_ID` único del 1 al 8.
- No conectes ni grabes ocho placas todavía. Primero prueba una pareja: un DevKit y un C3. El gateway comparador imprime `FIRMWARE_NOW_BIDIR_11B_GATE_DUAL_V2` y acepta tanto el paquete V1 de 32 bytes como el bidireccional de 44 bytes. El sender nuevo imprime `FIRMWARE_NOW_BIDIR_11B_BCAST_TX8D5_C3_V2`, con `tx_power_qdbm=34`.
- En VS Code, los directorios exclusivos son `build_bidir_esp32` (gateway) y `build_bidir_c3` (C3). Verifica los marcadores de firmware al arrancar para confirmar que se ejecutó la versión nueva.
- La compilación no se ha verificado en este equipo: al preparar ESP-IDF 5.5.2 aparecieron faltantes de instalación (constraints Python de IDF) y una línea inválida en la configuración Git de componentes de ESP-IDF. No interpretes el código como listo para grabar hasta que una compilación exitosa confirme ambos proyectos.

## Logger (solo el DevKit conectado a la laptop)

Desde esta carpeta raíz, instala dependencias una vez y ejecuta:

```powershell
py -m pip install -r tools\requirements.txt
py tools\espnow_logger_excel.py --port COM5 --output registros\prueba_bidir_lr.xlsx
```

Cambia COM5 según Windows. Cierra el monitor serial de ESP-IDF para liberar el puerto. El logger conserva JSONL de respaldo junto al Excel. Excel debe estar cerrado mientras se escribe.

## Lectura del resultado

- `ESPNOW_RX ...` prueba que el DevKit recibió el C3.
- `ESPNOW_APP_ACK_TX ... status=queued` solo indica que el gateway pasó la respuesta a la interfaz Wi-Fi.
- `ESPNOW_ACK_MAC_TX ... status=ok` indica éxito del enlace unicast en ese sentido, pero no sustituye la confirmación de aplicación.
- El siguiente `ESPNOW_RX` con `ack_state=received` y `ack_seq` igual a la secuencia anterior confirma que el C3 recibió y validó el ACK del DevKit. `ack_state=timeout` localiza fallo/retardo en la respuesta o su recepción, pero por sí solo no distingue interferencia de un error físico.
- Si no aparecen `ESPNOW_RX` en absoluto, el primer sentido C3→gateway aún no funciona. Si aparecen RX pero solo `ack_state=timeout`, investigar DevKit→C3 (canal, MAC/peer, firmware/recepción, antena/ubicación).
- En el gateway, `payload_format=v1_32B` identifica el sender anterior y `payload_format=bidir_44B` la variante nueva. Ambos generan las mismas métricas de RSSI y pérdida. La trama V1 no trae ID de nodo ni información del ACK; para esta comparación usa un solo nodo (ID 1).

Para comparar alcance, graba una sola vez este gateway dual en el DevKit y déjalo instalado durante ambas corridas. Ejecuta el sender V1 desde `NOW\espnowSender`, registra la prueba, reinicia el DevKit con EN para reiniciar sus contadores, y luego ejecuta el sender nuevo desde `NOW_BIDIRECCIONAL\espnowSender`. Mantén la misma pareja de placas, altura, orientación, ruta, pausa y duración; cada sender debe usar canal 1, 11b, broadcast, 8.5 dBm y 1 paquete/s. El tamaño del payload es 32 B en V1 y 44 B en la versión nueva, una diferencia pequeña pero anótala. En ambas corridas compara `rx_total`, `lost_total`, `loss_pct` y `rssi_avg_dbm`; en la nueva añade `ack_state`, RTT y RSSI de ACK. No es necesario cambiar puertos ni volver a grabar el gateway entre corridas. Para demostrar ida y vuelta, exige además `ESPNOW_APP_ACK ... result=received` en el sender nuevo.

## Antenas y compra

El ESP32-C3 Super Mini y el DevKit V1 pueden compartir el protocolo ESP-NOW/LR; no hace falta comprar un chip con más GPIO para una tarea de pocos pines. En la radio, la antena, el diseño RF de la placa, orientación, alimentación y despeje alrededor de la antena suelen importar más que los núcleos/GPIO. LR también es compatible entre familias ESP32 (excepto ESP32-C2), según Espressif; el DevKit V1 y C3 deben habilitar LR en ambos extremos.

Primero mide esta variante con las tarjetas actuales. Si el límite se concentra en puntos detrás de muros, compra **una sola** placa C3 con antena externa para A/B en el punto crítico, o prueba elevar/mover el gateway. La opción oficial ESP32-C3-MINI-1U usa conector externo, pero el módulo no incluye antena; la placa Super Mini con conector debe llevar la selección RF conectada de fábrica al conector. No modifiques resistencias ni conectes una antena a una placa cuyo trazado no esté diseñado para ello. Una antena externa no atraviesa mágicamente losas/muros y su ganancia debe respetar la regulación local.

Precios vistos para referencia (pueden cambiar): C3 Super Mini estándar a $82 MXN o $77.90 c/u comprando 5+, ocho ≈ $623.20; [publicación del vendedor](https://sandorobotics.com.mx/producto/hs5347/). Una placa ESP32-WROOM-32U se publicaba a $120 pero estaba agotada y la antena va aparte; no es un argumento para pagar más por GPIO que no usarás. [ESP32-C3-MINI-1/1U: opciones de antena y límites del fabricante](https://documentation.espressif.com/esp32-c3-mini-1_datasheet_en.html). Cotiza el modelo exacto, conector y antena antes de comprar.

Si una sola pareja no mantiene enlace en los salones, no gastes todavía en ocho radios. Primero prueba movimiento/altura del gateway, luego el C3 externo en el punto débil. Si el problema es una zona sombra del edificio, un nodo intermedio como repetidor puede resultar mejor costo/rendimiento, pero requeriría firmware adicional: ESP-NOW no hace mesh/reenvío automático.
