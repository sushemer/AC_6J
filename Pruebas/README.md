# Prueba Wi‑Fi con dos ESP32 mediante router

## Objetivo

Preparar una prueba de comunicación entre dos ESP32 que compartirán la red Wi‑Fi de un router, sin conexión física directa entre ellos:

- **ESP32 emisor:** genera y envía mensajes.
- **ESP32 receptor:** escucha, valida y registra los mensajes recibidos.
- **Router:** es el único enlace entre ambos dispositivos dentro de la misma red local.
- **Registro:** solo se captura y se guarda la salida del monitor serie del receptor.

Esta carpeta es temporal para código, pruebas, renders, cachés y archivos regenerables. Los archivos permanentes se copiarán a la carpeta oficial del proyecto cuando la prueba quede validada.

## Propuesta técnica inicial

La primera implementación usará **Wi‑Fi en modo estación (STA) + TCP sobre sockets BSD/LwIP**:

```text
ESP32 emisor  --Wi‑Fi STA--> Router / LAN <--Wi‑Fi STA-- ESP32 receptor
                                      |
                         captura serial únicamente del receptor
```

El receptor abrirá un puerto TCP y mostrará su dirección IP por el monitor serie. El emisor usará esa IP y el puerto configurado para conectarse y enviar mensajes periódicos. El receptor responderá con un acuse (`ACK`) para confirmar que el mensaje llegó.

### Por qué empezar con TCP

- Entrega ordenada y confiable.
- Reenvío automático ante pérdidas del enlace.
- Permite validar primero la conectividad y el protocolo de aplicación.
- Está cubierto directamente por los ejemplos oficiales de ESP-IDF.

Después de validar TCP, se puede agregar una variante UDP para comparar latencia, rendimiento y pérdida de paquetes.

## Estructura prevista

```text
Pruebas/
├── README.md
├── Wifi/
│   ├── emisor/
│   ├── receptor/
│   ├── scripts/
│   └── resultados/
├── ESP-NOW/
└── RyE/
```

Los nombres de red, contraseña, IP del receptor y puerto no deben quedar escritos directamente en el código. Se configurarán mediante `menuconfig` o valores locales de prueba que no se versionen.

## Secuencia de la prueba

1. Definir el SSID y la contraseña del router en cada proyecto.
2. Abrir el monitor serie del **receptor** y anotar la IP que recibió por DHCP.
3. Configurar esa IP en el **emisor**.
4. Verificar en el monitor serie del receptor:
   - conexión de ambos ESP32 al mismo SSID;
   - obtención de una IP en la misma subred;
   - conexión TCP;
   - mensajes enviados, recibidos y confirmados;
   - comportamiento después de reiniciar uno de los dispositivos o apagar temporalmente el router.

## Criterios de aceptación iniciales

- Ambos dispositivos se conectan al router sin intervención manual después del arranque.
- El emisor puede reconectarse si el receptor todavía no está disponible.
- El receptor recibe al menos 100 mensajes consecutivos sin corrupción del contenido.
- Cada mensaje contiene un número de secuencia para detectar duplicados o saltos.
- El monitor serie informa claramente de conexión, desconexión, error y reconexión.
- No se incluyen credenciales reales en Git ni en archivos permanentes.

## Uso de los scripts

Ejecutar desde `Wifi/scripts` y capturar únicamente el puerto serie del receptor:

```powershell
python -m pip install -r requirements.txt
python collect_serial_logs.py COM6 "..\resultados\wifi_prueba_01.xlsx"
```

El script captura continuamente hasta `Ctrl+C`, guarda cada 10 muestras y actualiza automáticamente `Wifi/resultados/wifi_pruebas.xlsx`. Si se interrumpe a mitad de un lote, guarda también las muestras pendientes. El emisor no se captura ni se guarda. Puede estar alimentado de forma independiente.

## Decisiones pendientes antes de programar

- Si el router tiene aislamiento de clientes (AP/client isolation); debe estar desactivado para que los dos ESP32 se vean entre sí.
- Tamaño, frecuencia y formato de los mensajes reales de la aplicación.
- Si se requiere únicamente entrega confiable (TCP) o también medición de latencia/rendimiento (UDP).

## Estado

- [x] Crear carpeta temporal de trabajo.
- [x] Revisar referencias oficiales similares.
- [x] Definir arquitectura inicial emisor/receptor.
- [x] Crear este README.
- [x] Crear el proyecto ESP-IDF del receptor.
- [x] Crear el proyecto ESP-IDF del emisor.
- [x] Preparar script de captura de monitores serie.
- [x] Preparar libro de Excel para configurar y registrar la prueba.
- [ ] Grabar y probar con dos placas reales.

## Archivos preparados

- `Wifi/emisor/`: cliente TCP que envía mensajes con secuencia y espera `ACK`.
- `Wifi/receptor/`: servidor TCP que recibe mensajes y responde `ACK`.
- `Wifi/scripts/`: captura del receptor e importación del CSV a Excel.
- `Wifi/resultados/wifi_pruebas.xlsx`: libro de control con configuración, instrucciones, registro y resumen.
- `Wifi/resultados/capturas/`: destino de logs y CSV generados durante las pruebas.

## Referencias ESP-IDF consultadas

- [Ejemplos de sockets BSD de ESP-IDF](https://github.com/espressif/esp-idf/tree/master/examples/protocols/sockets)
- [TCP server oficial](https://github.com/espressif/esp-idf/tree/master/examples/protocols/sockets/tcp_server)
- [TCP client oficial](https://github.com/espressif/esp-idf/tree/master/examples/protocols/sockets/tcp_client)
- [UDP server y UDP client](https://github.com/espressif/esp-idf/tree/master/examples/protocols/sockets)
- [Guía de sockets/LwIP](https://github.com/espressif/esp-idf/blob/master/docs/en/api-guides/lwip.rst)
- [Ejemplo Wi‑Fi iPerf](https://github.com/espressif/esp-idf/tree/master/examples/wifi/iperf)
- [Ejemplos Wi‑Fi de ESP-IDF](https://github.com/espressif/esp-idf/tree/master/examples/wifi)

