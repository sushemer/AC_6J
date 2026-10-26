param(
    [Parameter(Mandatory = $true)]
    [string]$InputFile,

    [Parameter(Mandatory = $true)]
    [ValidateSet("signal_on", "signal_off", "signal_temp_up", "signal_temp_down")]
    [string]$SignalName,

    [string]$SourceFile = (Join-Path $PSScriptRoot "main\emisor.c")
)

if (-not (Test-Path -LiteralPath $InputFile)) {
    throw "No existe el archivo de captura: $InputFile"
}
if (-not (Test-Path -LiteralPath $SourceFile)) {
    throw "No existe el emisor: $SourceFile"
}

$capture = Get-Content -LiteralPath $InputFile -Raw
$match = [regex]::Match($capture, 'static const rmt_symbol_word_t ir_signal\[\] = \{[\s\S]*?\n\};')
if (-not $match.Success) {
    throw "No se encontró un arreglo ir_signal[] completo en la captura."
}

$replacement = $match.Value -replace 'ir_signal\[\]', "$SignalName[]"
# El receptor demodulado entrega la marca invertida. En este emisor NPN,
# la marca debe ser HIGH y el espacio LOW.
$replacement = $replacement -replace '\.level0\s*=\s*[01]', '.level0 = 1'
$replacement = $replacement -replace '\.level1\s*=\s*[01]', '.level1 = 0'

$source = Get-Content -LiteralPath $SourceFile -Raw
$target = [regex]::Match($source, "static const rmt_symbol_word_t $SignalName\[\] = \{[\s\S]*?\n\};")
if (-not $target.Success) {
    throw "No se encontró el arreglo $SignalName[] en $SourceFile"
}

$symbolCount = ([regex]::Matches($replacement, 'duration0')).Count
if ($symbolCount -lt 20) {
    throw "La captura solo tiene $symbolCount símbolos; parece incompleta."
}

$backup = "$SourceFile.bak"
Copy-Item -LiteralPath $SourceFile -Destination $backup -Force
$updated = $source.Substring(0, $target.Index) + $replacement + $source.Substring($target.Index + $target.Length)
[System.IO.File]::WriteAllText($SourceFile, $updated, [System.Text.UTF8Encoding]::new($false))

Write-Host "OK: $symbolCount símbolos copiados a $SignalName[]"
Write-Host "Respaldo creado: $backup"
