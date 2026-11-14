# HTML -> PDF with Microsoft Edge in headless mode (no extra software needed).
#
#   powershell -ExecutionPolicy Bypass -File scripts/make_pdf.ps1 docs/report/hesabat.html
#
# The PDF is written next to the HTML file, with the same name.
# A temporary Edge profile is used so the normal Edge profile is not touched.
# (ASCII only: Windows PowerShell 5.1 reads BOM-less scripts in the ANSI code page.)

param([Parameter(Mandatory = $true, ValueFromRemainingArguments = $true)][string[]]$Html)

$edge = "${env:ProgramFiles(x86)}\Microsoft\Edge\Application\msedge.exe"
if (-not (Test-Path $edge)) { $edge = "$env:ProgramFiles\Microsoft\Edge\Application\msedge.exe" }
if (-not (Test-Path $edge)) { throw 'Microsoft Edge not found' }

$profileDir = Join-Path $env:TEMP 'mystl-edge-pdf-profile'
foreach ($file in $Html) {
    $src = (Resolve-Path $file).Path
    $pdf = [IO.Path]::ChangeExtension($src, '.pdf')
    $url = ([Uri]$src).AbsoluteUri
    if (Test-Path $pdf) { Remove-Item $pdf -Force }
    # Start-Process does not quote arguments: paths with spaces must be quoted by hand.
    $arguments = "--headless --disable-gpu --no-first-run --no-pdf-header-footer " +
                 "--user-data-dir=`"$profileDir`" --print-to-pdf=`"$pdf`" `"$url`""
    $p = Start-Process -FilePath $edge -ArgumentList $arguments -Wait -PassThru -WindowStyle Hidden
    for ($i = 0; $i -lt 20 -and -not (Test-Path $pdf); $i++) { Start-Sleep -Milliseconds 500 }
    if (Test-Path $pdf) {
        '{0}  ({1:N0} KB)' -f $pdf, ((Get-Item $pdf).Length / 1KB)
    } else {
        throw "PDF was not created: $src (Edge exit code $($p.ExitCode))"
    }
}
