# Flujo de captura y emision IR para varios controles Midea

Este flujo usa dos proyectos independientes de ESP-IDF:

- `Receptor`: VS1838B en GPIO22. Captura el mensaje que envia el control.
- `Emisor`: LED IR en GPIO32 y tres botones en GPIO25, GPIO26 y GPIO27.

La señal de los aires Midea no es solamente un codigo de encendido. Cada mensaje
contiene el estado completo: encendido, modo, ventilador, temperatura y checksum.
Por eso se debe capturar un estado valido y conservar la trama completa.

## 1. Capturas minimas por control

Para cada uno de los tres controles, guardar estas cinco capturas:

| Archivo | Accion del control |
|---|---|
| `controlN_on.txt` | Aire encendido |
| `controlN_off.txt` | Aire apagado |
| `controlN_auto.txt` | Modo automatico |
| `controlN_temp_up.txt` | Boton de aumentar temperatura |
| `controlN_temp_down.txt` | Boton de disminuir temperatura |
| `controlN_temp_base.txt` | Temperatura elegida, por ejemplo 20 C |
| `controlN_temp_siguiente.txt` | Un grado arriba, por ejemplo 21 C |

Las dos temperaturas consecutivas permiten comprobar cuanto cambia el byte de
temperatura. No es necesario capturar todos los grados antes de automatizarlo.

## 2. Receptor

Conectar:

```text
VS1838B VCC  -> 3.3 V
VS1838B GND  -> GND
VS1838B OUT  -> GPIO22
```

Compilar y cargar:

```powershell
idf.py set-target esp32
idf.py build
idf.py -p COMx flash monitor
```

Una captura manual se inicia escribiendo `r` y Enter en el monitor. El receptor
imprime el arreglo `ir_signal[]` completo para poder copiarlo al emisor.

Para guardar una captura desde PowerShell, con el monitor cerrado:

```powershell
Set-ExecutionPolicy -Scope Process Bypass
.\capturar_lote.ps1 -Port COMx -Control control1 -Accion on
```

Repetir cambiando `control1` por `control2` o `control3`, y `on` por `off`,
`auto`, `temp_up`, `temp_down`, `temp_base` y `temp_siguiente`. El script guarda los bloques en
`Receptor/capturas/`.

## 3. Emisor

Circuito:

```text
GPIO32 -- 1 kOhm -- base del NPN
emisor NPN -------- GND
5 V -- 220 Ohm -- anodo LED IR
catodo LED IR ----- colector NPN
GND de 5 V -------- GND del ESP32
```

Botones, cada uno entre GPIO y GND:

| Funcion | GPIO |
|---|---:|
| Temperatura + | 25 |
| Temperatura - | 26 |
| Encendido/apagado | 27 |

El emisor actual tiene ranuras para `signal_on`, `signal_off`, `signal_temp_up`
y `signal_temp_down`. El script `Emisor/copiar_captura.ps1` invierte los niveles
del VS1838B para el transistor NPN y sustituye una ranura completa.

Ejemplo:

```powershell
Set-ExecutionPolicy -Scope Process Bypass
.\copiar_captura.ps1 -InputFile "..\Receptor\capturas\control1_on.txt" -SignalName signal_on
.\copiar_captura.ps1 -InputFile "..\Receptor\capturas\control1_off.txt" -SignalName signal_off
```

El checksum de una captura se conserva. Antes de usar temperaturas generadas,
comparar las capturas `temp_base` y `temp_siguiente`; solamente si cambia un
campo de temperatura de forma regular se debe generar el resto automáticamente.

## 4. Orden recomendado de pruebas

1. Probar primero `signal_on` y `signal_off` con el mismo control.
2. Confirmar la señal con otro receptor VS1838B antes de apuntar al aire.
3. Capturar dos temperaturas consecutivas.
4. Verificar que el byte de temperatura cambia y que el checksum también cambia.
5. Automatizar el rango normal de temperatura.
6. Agregar el modo automatico como una plantilla independiente.

No se deben mezclar capturas de controles distintos. Cada control debe conservar
su propia plantilla de seis bytes y sus propias señales, aunque los controles
parezcan iguales.
