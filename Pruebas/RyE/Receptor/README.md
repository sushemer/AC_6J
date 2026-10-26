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

## Uso

```text
idf.py set-target esp32
idf.py build
idf.py -p COMx flash monitor
```

Presiona un botón del control remoto. El monitor imprimirá un arreglo C con la
señal capturada. Copia ese arreglo en `Emisor/main/emisor.c` para reproducirlo.
