# Scripts de prueba

## Capturar el monitor serie del receptor
Cuando estén conectadas las dos placas, ejecutar Python desde esta carpeta:

```powershell
python -m pip install -r requirements.txt
python collect_serial_logs.py COM6 "..\resultados\wifi_prueba_01.xlsx"
```

El script captura continuamente hasta `Ctrl+C`. Cada 10 muestras guarda automáticamente en `resultados/capturas` y actualiza `resultados/wifi_pruebas.xlsx`:
- un log crudo del receptor;
- un CSV con `Timestamp`, `Device`, `Level`, `Sequence`, `Message`, `Event`, `SourceIP`, `Bytes`, `RSSI_dBm`, `WiFiQuality_percent` y `WiFiChannel`;
- una actualización del libro Excel.

El segundo argumento es el archivo `.xlsx` de esa prueba. Si no existe, el script lo crea automáticamente con la hoja `Registro` y sus encabezados. Los archivos `.log` y `.csv` se guardan automáticamente en una subcarpeta `capturas` junto al libro indicado.

Cuando el firmware del receptor imprime `Enlace WiFi`, el script conserva ese RSSI, calidad y canal en los eventos siguientes, incluido cada mensaje `RX`. La calidad es una estimación de 0 a 100 calculada por el firmware a partir del RSSI; el valor original en dBm se conserva en `RSSI_dBm`.

Solo se captura el monitor serie del receptor. Si se interrumpe a mitad de un lote, guarda también las muestras pendientes.

## Reimportar un CSV existente
Con Microsoft Excel instalado, ejecutar:
```powershell
python import_csv_to_excel.py --csv-path "..\resultados\capturas\20261022_130000_registro.csv"
```

El importador abre `resultados/wifi_pruebas.xlsx`, agrega los eventos en la hoja `Registro`, guarda el libro y lo cierra. Solo se necesita para reimportar un CSV anterior.

