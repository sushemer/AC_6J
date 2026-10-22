"""Capture the receiver continuously and save each batch to CSV and Excel."""

from __future__ import annotations

import argparse
import csv
import re
from datetime import datetime
from pathlib import Path

try:
    import serial
except ImportError as exc:
    raise SystemExit("Falta pyserial. Instala con: python -m pip install pyserial") from exc

BATCH_SIZE = 10


def main() -> None:
    parser = argparse.ArgumentParser(description="Captura continua del monitor serie del receptor ESP32")
    parser.add_argument("receiver_port", help="Puerto COM del receptor")
    parser.add_argument("workbook_path", type=Path, help="Archivo XLSX donde se guardaran los resultados")
    parser.add_argument("--baud-rate", type=int, default=115200)
    parser.add_argument("--output-directory", type=Path, default=None)
    args = parser.parse_args()

    root = Path(__file__).resolve().parents[1]
    workbook_path = args.workbook_path.resolve()
    workbook_path.parent.mkdir(parents=True, exist_ok=True)

    output_dir = args.output_directory or workbook_path.parent / "capturas"
    output_dir.mkdir(parents=True, exist_ok=True)
    stamp = datetime.now().strftime("%Y%m%d_%H%M%S")
    raw_path = output_dir / f"{stamp}_receptor_raw.log"
    csv_path = output_dir / f"{stamp}_registro.csv"
    sequence_re = re.compile(r"SEQ[=:](\d+)")
    rssi_re = re.compile(r"RSSI\s*[=:]\s*(-?\d+)\s*dBm", re.IGNORECASE)
    quality_re = re.compile(r"calidad\s*[=:]\s*(\d+)\s*%", re.IGNORECASE)
    channel_re = re.compile(r"canal\s*[=:]\s*(\d+)", re.IGNORECASE)
    bytes_re = re.compile(r"RX\s+(\d+)\s+bytes", re.IGNORECASE)
    ip_re = re.compile(r"(?:IP recibida:\s*|sta ip:\s*)(\d{1,3}(?:\.\d{1,3}){3})", re.IGNORECASE)
    source_ip_re = re.compile(r"Emisor conectado desde\s+(\d{1,3}(?:\.\d{1,3}){3})", re.IGNORECASE)
    batch: list[dict[str, str]] = []
    batch_lines: list[str] = []
    link_state = {"RSSI_dBm": "", "WiFiQuality_percent": "", "WiFiChannel": ""}

    fieldnames = [
        "Timestamp", "Device", "Level", "Sequence", "Message",
        "Event", "SourceIP", "Bytes", "RSSI_dBm",
        "WiFiQuality_percent", "WiFiChannel",
    ]

    def make_row(line: str) -> dict[str, str]:
        nonlocal link_state
        rssi = rssi_re.search(line)
        quality = quality_re.search(line)
        channel = channel_re.search(line)
        if rssi:
            link_state["RSSI_dBm"] = rssi.group(1)
        if quality:
            link_state["WiFiQuality_percent"] = quality.group(1)
        if channel:
            link_state["WiFiChannel"] = channel.group(1)

        sequence = sequence_re.search(line)
        byte_count = bytes_re.search(line)
        ip_match = ip_re.search(line)
        source_ip = source_ip_re.search(line)
        if "Enlace WiFi:" in line:
            event = "wifi_quality"
        elif "RX " in line and "bytes" in line:
            event = "rx"
        elif "Emisor conectado desde" in line:
            event = "tcp_connected"
        elif "IP recibida:" in line or "sta ip:" in line:
            event = "ip_received"
        elif "desconect" in line.lower():
            event = "wifi_disconnected"
        else:
            event = "log"

        return {
            "Timestamp": datetime.now().isoformat(timespec="milliseconds"),
            "Device": "receptor",
            "Level": level_for_line(line),
            "Sequence": sequence.group(1) if sequence else "",
            "Message": line,
            "Event": event,
            "SourceIP": (source_ip or ip_match).group(1) if (source_ip or ip_match) else "",
            "Bytes": byte_count.group(1) if byte_count else "",
            "RSSI_dBm": rssi.group(1) if rssi else link_state["RSSI_dBm"],
            "WiFiQuality_percent": quality.group(1) if quality else link_state["WiFiQuality_percent"],
            "WiFiChannel": channel.group(1) if channel else link_state["WiFiChannel"],
        }

    def level_for_line(line: str) -> str:
        if re.search(r"\bE\b|ERROR|Fallo|fallo", line):
            return "ERROR"
        if re.search(r"\bW\b|WARN|desconect", line):
            return "WARN"
        return "INFO"

    def save_batch(raw_file, writer, csv_file) -> None:
        if not batch:
            return
        raw_file.writelines(line + "\n" for line in batch_lines)
        raw_file.flush()
        writer.writerows(batch)
        csv_file.flush()
        try:
            from import_csv_to_excel import import_rows_to_excel
            import_rows_to_excel(batch, workbook_path)
        except Exception as exc:
            print(f"Aviso: el lote quedó guardado en CSV, pero no se actualizó Excel: {exc}")
        print(f"Guardadas {len(batch)} muestras en CSV y Excel")
        batch.clear()
        batch_lines.clear()

    try:
        with serial.Serial(args.receiver_port, args.baud_rate, timeout=0.2) as port, raw_path.open("w", encoding="utf-8") as raw_file, csv_path.open("w", encoding="utf-8-sig", newline="") as csv_file:
            writer = csv.DictWriter(csv_file, fieldnames=fieldnames)
            writer.writeheader()
            csv_file.flush()
            print(f"Capturando continuamente el receptor en {args.receiver_port}")
            print(f"Guardado automático cada {BATCH_SIZE} muestras. Presiona Ctrl+C para terminar.")
            while True:
                raw_line = port.readline()
                if not raw_line:
                    continue
                line = raw_line.decode("utf-8", errors="replace").rstrip("\r\n")
                batch_lines.append(line)
                batch.append(make_row(line))
                print(line)
                if len(batch) >= BATCH_SIZE:
                    save_batch(raw_file, writer, csv_file)
    except KeyboardInterrupt:
        print("Captura detenida por el usuario.")
        if batch:
            print(f"Guardando lote final de {len(batch)} muestras...")
            try:
                with raw_path.open("a", encoding="utf-8") as raw_file, csv_path.open("a", encoding="utf-8-sig", newline="") as csv_file:
                    writer = csv.DictWriter(csv_file, fieldnames=fieldnames)
                    save_batch(raw_file, writer, csv_file)
            except Exception as exc:
                print(f"Aviso: no se pudo guardar el lote final: {exc}")
    except serial.SerialException as exc:
        raise SystemExit(f"No se pudo abrir o leer {args.receiver_port}: {exc}") from exc
    print(f"Log crudo: {raw_path}")
    print(f"CSV: {csv_path}")


if __name__ == "__main__":
    main()
