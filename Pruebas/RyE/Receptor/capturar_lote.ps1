param(
    [Parameter(Mandatory = $true)] [string]$Port,
    [int]$BaudRate = 115200,
    [Parameter(Mandatory = $true)] [ValidateSet('control1','control2','control3')] [string]$Control,
    [Parameter(Mandatory = $true)] [ValidateSet('on','off','auto','temp_up','temp_down','temp_base','temp_siguiente')] [string]$Accion,
    [string]$OutputDirectory = (Join-Path $PSScriptRoot 'capturas')
)

$ErrorActionPreference = 'Stop'
New-Item -ItemType Directory -Path $OutputDirectory -Force | Out-Null
$safeName = "${Control}_${Accion}"
$outputFile = Join-Path $OutputDirectory ("{0}_{1:yyyyMMdd_HHmmss}.txt" -f $safeName, (Get-Date))

$serial = [System.IO.Ports.SerialPort]::new($Port, $BaudRate, [System.IO.Ports.Parity]::None, 8, [System.IO.Ports.StopBits]::One)
$serial.NewLine = "`n"
$serial.ReadTimeout = 250
$serial.Open()
try {
    Start-Sleep -Milliseconds 500
    $serial.DiscardInBuffer()
    $serial.WriteLine('r')
    Write-Host "Captura armada para $Control / $Accion. Presiona ahora el boton del control." -ForegroundColor Cyan

    $lines = [System.Collections.Generic.List[string]]::new()
    $deadline = (Get-Date).AddSeconds(15)
    while ((Get-Date) -lt $deadline) {
        try { $line = $serial.ReadLine() } catch [TimeoutException] { continue }
        $lines.Add($line)
        if ($line -match 'Captura lista') { break }
    }
    if ($lines.Count -eq 0 -or -not ($lines -match 'ir_signal')) {
        throw 'No se recibio un arreglo ir_signal completo dentro del tiempo limite.'
    }
    [System.IO.File]::WriteAllLines($outputFile, $lines, [System.Text.UTF8Encoding]::new($false))
    Write-Host "OK: captura guardada en $outputFile" -ForegroundColor Green
}
finally {
    $serial.Close()
    $serial.Dispose()
}
