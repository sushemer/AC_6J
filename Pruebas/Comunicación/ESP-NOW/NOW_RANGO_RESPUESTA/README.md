# Prueba ESP-NOW de alcance y respuesta

Esta carpeta es una copia independiente de `AC-TEMP/NOW`. El proyecto original no se modificó. Se copiaron firmware, herramientas y el respaldo `registros/prueba_201.jsonl`; no se copiaron los directorios `build` ni binarios viejos para evitar que ESP-IDF grabe una versión anterior. El Excel original no se pudo copiar porque sigue como archivo bajo demanda de OneDrive.

## Cambios de esta variante

- Los ocho nodos usan ESP-NOW unicast al gateway, modo LR de Espressif y velocidad fija de 500 Kbps. El modo LR debe estar habilitado en el gateway y en cada C3.
- Se quitó del sender el límite de potencia impuesto por el firmware anterior (34 cuartos de dBm, que ESP-IDF mapea a 8 dBm). La configuración del proyecto conserva el máximo ordinario de ESP-IDF; el driver y la configuración/regulación de radio siguen controlando la potencia real. No se usa un amplificador ni se fuerza potencia por encima del límite permitido.
- Cada C3 envía cada 500 ms. El ID 1..8 agrega un desfase de 40 ms para separar nodos que arrancan juntos.
- El gateway lleva pérdida, RSSI mínimo/promedio/máximo, duplicados y reinicios **por MAC**, no mezcla secuencias de distintos nodos.
- El emisor mide el tiempo entre solicitar el envío y recibir el callback de estado MAC. Ese dato se incluye en el siguiente paquete y queda guardado en el log del gateway, así se puede capturar conectando solo el gateway a la laptop. Un status MAC exitoso en unicast significa que llegó el ACK de enlace, no confirma que la aplicación haya procesado el sensor.

Los paquetes son pequeños, por lo que 500 Kbps es una velocidad baja deliberada para probar alcance; el costo es menor capacidad total. Ocho nodos a 2 paquetes/segundo suman unos 16 paquetes/segundo si los ocho están activos.

## Configuración antes de grabar

El gateway predeterminado en el código tiene MAC STA `e0:8c:fe:5d:04:ec`, que aparece en el respaldo JSONL de la prueba previa. Confirma que esa es la MAC actual del DevKit. Si cambió la tarjeta, edita `s_gateway_mac` en `espnowSender/main/espnow_example_main.c` con la MAC STA que imprime el gateway al arrancar.

En cada nodo C3, cambia `ESPNOW_NODE_ID` en el mismo archivo a un valor único de 1 a 8. Graba cada nodo después de cambiar ese valor. Si se graban los ocho con el mismo ID, las estadísticas por MAC seguirán separadas, pero la asignación lógica y el escalonamiento de transmisiones no serán los planeados.

La dependencia del logger se instala una vez con `py -m pip install -r tools\requirements.txt`.

## Compilar, grabar y probar

En VS Code abre primero `espnowClient` para el gateway DevKit y después `espnowSender` para cada C3. Las carpetas de compilación configuradas son exclusivas:

- Gateway ESP32: `build_range_esp32`
- C3: `build_range_c3`

Las etiquetas de arranque que se esperan son `FIRMWARE_NOW_RANGE_LR500K_RX_V1` en el gateway y `FIRMWARE_NOW_RANGE_LR500K_V1` en el C3. Si no aparecen, se grabó otro firmware.

El gateway receptor debe permanecer encendido durante toda la prueba. Conecta solo su USB a la laptop (actualmente COM5), cierra su monitor serial de ESP-IDF y, desde esta carpeta raíz, ejecuta:

```powershell
py tools\espnow_logger_excel.py --port COM5 --output registros\prueba_rango_lr.xlsx
```

Inicia los C3, recorre la ruta y deja correr el logger. Termina con Ctrl+C. El logger guarda JSONL de respaldo y actualiza Excel. Mantén Excel cerrado durante la captura. Cambia COM5 si Windows asigna otro puerto al gateway.

## Qué comparar en el edificio

Haz la misma ruta y ubicación del gateway en cada corrida. Registra en el orden siguiente para saber qué cambio ayudó:

1. Código anterior / 11b / potencia reducida, solo como referencia ya capturada.
2. Potencia sin el límite artificial, manteniendo el PHY anterior.
3. Mismo punto y potencia, pero modo LR a 500 Kbps (esta copia).
4. Después, mueve el gateway o prueba una antena externa; cambia una sola cosa por corrida.

Anota ubicación, distancia aproximada, pared/obstáculos y hora junto a cada tramo. RSSI bajo junto con pérdida alta identifica una zona de enlace débil. Si la pérdida se concentra detrás de muros o en áreas sin línea de vista, un repetidor bien ubicado puede ser más económico que cambiar las ocho tarjetas. ESP-NOW no retransmite entre nodos automáticamente; el firmware de aplicación tendría que implementar el reenvío.

## Evidencia de la captura anterior

`registros/prueba_201.jsonl` conserva las líneas originales de la captura. El firmware que generó ese archivo se reinició durante la sesión y reportó dos segmentos con paquetes: en uno cerró con 64 recibidos y 12 perdidos (15.79%); en otro con 114 recibidos y 100 perdidos (46.73%). En los 178 paquetes RX guardados, el RSSI observado fue de -97 a -54 dBm, promedio aproximado -74.2 dBm. Como hubo reinicios y el recorrido no marcó posición por paquete, estos datos prueban variación/pérdida, pero no permiten asignar el peor RSSI a un punto o distancia exactos.
