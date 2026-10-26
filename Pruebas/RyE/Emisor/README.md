# Emisor IR con tres botones — ESP32 DevKit V1

Proyecto ESP-IDF para ESP32 DevKit V1 / WROOM-32. LED IR en GPIO32 mediante transistor,
con portadora de 38 kHz.

## LED IR

```text
GPIO32 -- 1 kOhm -- base 2N2222 o BC337
GND ---------------- emisor
5 V -- 220 Ohm -- LED IR -- colector
```

Comparte GND entre la fuente de 5 V y el ESP32.

## Botones

Conecta cada botón entre su GPIO y GND. Se usan pull-ups internas:

| Función | GPIO |
|---|---:|
| Subir | GPIO25 |
| Bajar | GPIO26 |
| Encendido/apagado | GPIO27 |

Captura los tres botones con `Receptor` y copia sus arreglos en `signal_up`,
`signal_down` y `signal_power` de `main/emisor.c`.

```text
idf.py set-target esp32
idf.py build
idf.py -p COMx flash monitor
```

Después de cargarlo, presiona un botón físico; no se necesitan comandos en el monitor.
