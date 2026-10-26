"""Import receiver CSV events into the Registro sheet of the test workbook."""

from __future__ import annotations

import argparse
import csv
from pathlib import Path


HEADERS = [
    "Timestamp", "Device", "Level", "Sequence", "Message",
    "Event", "SourceIP", "Bytes", "RSSI_dBm",
    "WiFiQuality_percent", "WiFiChannel",
]


def create_workbook(workbook_path: Path) -> None:
    try:
        import win32com.client
    except ImportError as exc:
        raise RuntimeError("Falta pywin32. Instala con: python -m pip install pywin32") from exc

    excel = win32com.client.Dispatch("Excel.Application")
    workbook = None
    try:
        workbook = excel.Workbooks.Add()
        sheet = workbook.Worksheets(1)
        sheet.Name = "Registro"
        sheet.Range("A5:K5").Value = (tuple(HEADERS),)
        sheet.Range("A5:K5").Font.Bold = True
        sheet.Range("A5:K5").Interior.Color = 0xD9EAF7
        sheet.Columns("A:K").AutoFit()
        workbook.SaveAs(str(workbook_path.resolve()), FileFormat=51)
        print(f"Creado libro Excel: {workbook_path}")
    finally:
        if workbook is not None:
            workbook.Close(SaveChanges=True)
        excel.Quit()


def import_rows_to_excel(rows: list[dict[str, str]], workbook_path: Path) -> None:
    if not rows:
        return
    if not workbook_path.exists():
        workbook_path.parent.mkdir(parents=True, exist_ok=True)
        create_workbook(workbook_path)
    try:
        import win32com.client
    except ImportError as exc:
        raise RuntimeError("Falta pywin32. Instala con: python -m pip install pywin32") from exc
    excel = win32com.client.Dispatch("Excel.Application")
    excel.Visible = True
    workbook = None
    try:
        workbook = excel.Workbooks.Open(str(workbook_path.resolve()))
        sheet = workbook.Worksheets("Registro")
        for column, header in enumerate(HEADERS, start=1):
            if not sheet.Cells(5, column).Value:
                sheet.Cells(5, column).Value = header
        row = max(6, sheet.Cells(sheet.Rows.Count, 1).End(-4162).Row + 1)
        for item in rows:
            sheet.Cells(row, 1).Value = item.get("Timestamp", "")
            sheet.Cells(row, 2).Value = "receptor"
            sheet.Cells(row, 3).Value = item.get("Level", "")
            if item.get("Sequence", ""):
                sheet.Cells(row, 4).Value = int(item["Sequence"])
            sheet.Cells(row, 5).Value = item.get("Message", "")
            sheet.Cells(row, 6).Value = item.get("Event", "")
            sheet.Cells(row, 7).Value = item.get("SourceIP", "")
            if item.get("Bytes", ""):
                sheet.Cells(row, 8).Value = int(item["Bytes"])
            if item.get("RSSI_dBm", ""):
                sheet.Cells(row, 9).Value = int(item["RSSI_dBm"])
            if item.get("WiFiQuality_percent", ""):
                sheet.Cells(row, 10).Value = int(item["WiFiQuality_percent"])
            if item.get("WiFiChannel", ""):
                sheet.Cells(row, 11).Value = int(item["WiFiChannel"])
            row += 1
        workbook.Save()
    finally:
        if workbook is not None:
            workbook.Close(SaveChanges=True)
        excel.Quit()


def import_csv_to_excel(csv_path: Path, workbook_path: Path) -> None:
    if not csv_path.exists():
        raise FileNotFoundError(f"No existe el CSV: {csv_path}")
    with csv_path.open("r", encoding="utf-8-sig", newline="") as csv_file:
        rows = list(csv.DictReader(csv_file))
    import_rows_to_excel(rows, workbook_path)
    print(f"Importados {len(rows)} eventos en {workbook_path}")


def main() -> None:
    parser = argparse.ArgumentParser(description="Importa el CSV del receptor al Excel de pruebas")
    parser.add_argument("--csv-path", type=Path, required=True)
    parser.add_argument("--workbook-path", type=Path, default=None)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    import_csv_to_excel(args.csv_path, args.workbook_path or root / "resultados" / "wifi_pruebas.xlsx")


if __name__ == "__main__":
    main()
