# ESP-NOW

## Capturar datos en Excel

`tools/espnow_logger_excel.py` adapta la idea del registrador GPS de Cistable.
Lee las consolas seriales sin modificar el firmware. Desde AC-TEMP:

```sh
py -m pip install -r tools/requirements.txt
py tools/espnow_logger_excel.py --list-ports
py tools/espnow_logger_excel.py --port COM7 --output registros/prueba_01.xlsx
```

Sustituye COM7 por el puerto del **receptor**. Cierra antes `idf.py monitor` o
cualquier monitor que use ese COM. La velocidad predeterminada es 115200 baud;
se puede cambiar con `--baud`. Inicia la captura antes de iniciar el emisor.
En la configuración actual, receptor COM5 y emisor COM10:

```sh
py tools/espnow_logger_excel.py --port COM5 --sender-port COM10 --output registros/prueba_espnow.xlsx
```

Si solo quieres registrar el receptor, omite `--sender-port COM10`.

El firmware sigue enviando; Python escucha. Termina con **Ctrl+C** para guardar.
El Excel se actualiza cada 30 segundos (`--save-seconds 10` cambia el intervalo).
Mantén Excel cerrado durante la captura. Sin `--output` se crea un nombre con
fecha y hora en `registros`. Al iniciar no se sobrescriben pruebas existentes.

### Contenido de la hoja Registro

Incluye fecha del PC con zona horaria, rol, puerto, tipo de evento, MAC, secuencia,
RSSI actual/mínimo/máximo/promedio en dBm, bytes del paquete, texto recibido,
conteo acumulado RX/perdidos, porcentaje de pérdida, descartes de la cola y frames
de longitud inválida. Conserva **todas las líneas** (incluidos arranque, alertas y
resultados de envío) y sus bytes en hexadecimal. Hay filtros y encabezados fijos.

- `tiempo_sesion_s`: tiempo monotónico desde el inicio hasta recibir la línea completa.
- `tiempo_esp_log_ms`: marca del log ESP-IDF, cuando existe; no duración de cómputo.
- `intervalo_rx_pc_ms`: tiempo entre líneas recibidas del mismo emisor; incluye
  efectos de serial y del sistema operativo.
- `tiempo_esp_rx_us`: reloj monotónico del receptor al recibir el paquete; sirve
  para ordenar las recepciones dentro de ese arranque, no es hora de calendario.
- `procesamiento_python_ms`: duración de decodificar, interpretar y construir
  el registro; excluye espera serial, escritura y exportación a Excel.

El firmware actual **no reporta tiempo de cómputo del ESP32 ni latencia de radio**.
Para medirlos hay que instrumentar el firmware y definir sincronización de relojes
si se comparan tiempos entre dispositivos. La fecha del PC corresponde al
procesamiento; el tiempo de sesión usa la llegada observada por el lector serial.

La pérdida del firmware se calcula por saltos entre secuencias recibidas; no incluye
paquetes que falten antes de la primera secuencia observada. Los saltos del logger
se calculan por MAC y puerto desde el primer paquete observado.
Se distinguen avances, duplicados y retrocesos/reinicios; no son un conteo exacto
de pérdidas ante reinicios o reordenamientos. El éxito de envío se guarda tal
como lo imprime el firmware: con broadcast no demuestra recepción en el cliente.

### Respaldo y recuperación

Cada línea se escribe inmediatamente en un `.jsonl` junto al Excel. La exportación
usa un archivo temporal y reemplaza el Excel al terminar. Si Excel está bloqueado,
el respaldo sigue guardándose. Para exportarlo después, usa un nombre nuevo:

```sh
py tools/espnow_logger_excel.py --from-jsonl registros/prueba_01.jsonl --output registros/prueba_01_recuperada.xlsx
```

También se intenta guardar al desconectarse un puerto. No se recuperan datos
anteriores a abrir la captura. Un corte de energía o cierre forzado puede perder
los últimos buffers del sistema, aunque se hace `flush` por registro.
Para capturas muy largas, divide las pruebas: Excel admite 1 048 576 filas por hoja
y el tiempo y memoria de exportación aumentan con el número de registros.
