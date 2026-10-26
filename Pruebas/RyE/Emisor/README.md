# Emisor IR con tres botones — ESP32 DevKit V1

Proyecto ESP-IDF para ESP32 DevKit V1 / WROOM-32. LED IR en GPIO32 mediante transistor,
con portadora de 38 kHz.

La captura se realiza con el proyecto ESP-IDF `..\Receptor`, no con
`IRrecvDumpV3`. `IRrecvDumpV3` es un ejemplo Arduino/PlatformIO separado.

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

Las capturas actuales de encendido y apagado a 67 °F están en `signal_on[]` y
`signal_off[]` de `main/emisor.c`. El botón POWER alterna entre ambas tramas.
Las ranuras `signal_temp_up[]` y `signal_temp_down[]` corresponden a los
botones de subir y bajar temperatura.
Cada captura ya contiene el checksum que envió el control; no se calcula otro.
Para cambiar temperatura, modo o ventilador se deben capturar nuevas tramas
completas, porque el checksum también cambia.

## Copiar una captura rápidamente

El archivo `copiar_captura.ps1` toma el bloque `ir_signal[]` pegado desde el
receptor, invierte los niveles para el transistor NPN del emisor y lo coloca en
la ranura indicada. Ejemplos desde esta carpeta:

```powershell
Set-ExecutionPolicy -Scope Process Bypass
.\copiar_captura.ps1 -InputFile "C:\ruta\captura_on.txt" -SignalName signal_on
.\copiar_captura.ps1 -InputFile "C:\ruta\captura_off.txt" -SignalName signal_off
.\copiar_captura.ps1 -InputFile "C:\ruta\captura_temp_up.txt" -SignalName signal_temp_up
.\copiar_captura.ps1 -InputFile "C:\ruta\captura_temp_down.txt" -SignalName signal_temp_down
```

Después de copiar las cuatro señales:

```powershell
idf.py build
idf.py -p COM10 flash monitor
```

```text
idf.py set-target esp32
idf.py build
idf.py -p COMx flash monitor
```

Después de cargarlo, presiona un botón físico; no se necesitan comandos en el monitor.
