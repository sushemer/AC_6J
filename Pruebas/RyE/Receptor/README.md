# Receptor IR

Proyecto ESP-IDF independiente para capturar controles infrarrojos con un
VS1838B o TSOP38238.

## Conexiones

| Receptor | ESP32 |
|---|---|
| VCC | 3V3 |
| GND | GND |
| OUT | GPIO22 |

Coloca un capacitor de 100 nF entre VCC y GND cerca del receptor.

## Uso manual

```text
idf.py set-target esp32
idf.py build
idf.py -p COMx flash monitor
```

Presiona `r` y Enter en el monitor para armar una captura; después presiona un
botón del control remoto. El monitor imprimirá un arreglo C con la señal
capturada. No presiones otro botón durante la captura.

## Capturas por lote

Con el monitor cerrado se puede guardar cada captura directamente desde
PowerShell:

```powershell
Set-ExecutionPolicy -Scope Process Bypass
.\capturar_lote.ps1 -Port COM5 -Control control1 -Accion on
```

Valores válidos:

```text
Control: control1, control2, control3
Accion:  on, off, auto, temp_base, temp_siguiente
```

El archivo se guarda en `Receptor/capturas/`. Para cada control se recomienda
capturar encendido, apagado, modo automático y dos temperaturas consecutivas.
Las temperaturas consecutivas sirven para comprobar si el byte de temperatura
puede modificarse automáticamente; no se deben inventar valores hasta validar
esa diferencia.

Después se usa `Emisor/copiar_captura.ps1` para colocar la captura seleccionada
en una ranura del emisor.
