# Faza C: include/mystl/vector.hpp üçün sətir coverage-i (gcov ilə).
#
# İşə salmaq (layihənin kök qovluğundan):
#   powershell -ExecutionPolicy Bypass -File scripts/coverage.ps1
#
# Necə işləyir:
#   1. Hər test faylı --coverage ilə ayrıca kompilyasiya olunur (build/coverage/).
#   2. Testlər işə salınır, gcov sayğacları (.gcda) yazılır.
#   3. Hər translation unit üçün gcov vector.hpp-nin sətir sayğaclarını verir.
#      Header bir neçə .cpp-yə daxil olduğu üçün nəticələr BİRLƏŞDİRİLİR:
#      sətir hər hansı bir TU-da icra olunubsa, "əhatə olunub" sayılır.
#
# Məhdudiyyət: template kodu yalnız instansiya olunduqda sətir sayılır.
# Heç bir testdə istifadə olunmayan template funksiyası hesabata düşmür.

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$out = Join-Path $root 'build/coverage'
New-Item -ItemType Directory -Force $out | Out-Null
Get-ChildItem $out -Include *.gcda, *.gcno, *.o, *.exe -Recurse | Remove-Item -Force

$sources = Get-ChildItem (Join-Path $root 'tests') -Filter 'test_*.cpp' | ForEach-Object FullName
$objects = @()
foreach ($src in $sources) {
    $obj = Join-Path $out ([IO.Path]::GetFileNameWithoutExtension($src) + '.o')
    & g++ -std=c++20 -O0 -g --coverage -I (Join-Path $root 'include') -c $src -o $obj
    if ($LASTEXITCODE -ne 0) { throw "compile failed: $src" }
    $objects += $obj
}
$exe = Join-Path $out 'tests_coverage.exe'
& g++ --coverage $objects -o $exe
if ($LASTEXITCODE -ne 0) { throw 'link failed' }

& $exe | Select-String 'test cases' | ForEach-Object { $_.Line.Trim() }

# Sətir -> sayğac (birləşdirilmiş). gcov -t nəticəni stdout-a yazır.
$lines = @{}   # sətir nömrəsi -> [bool] icra olunub
$text = @{}    # sətir nömrəsi -> mənbə mətni
Push-Location $out
try {
    foreach ($src in $sources) {
        $obj = Join-Path $out ([IO.Path]::GetFileNameWithoutExtension($src) + '.o')
        $inHeader = $false
        foreach ($row in (& gcov -t -o $obj $src 2>$null)) {
            if ($row -match '^\s*-:\s*0:Source:(.*)$') {
                $inHeader = $Matches[1] -match 'mystl[\\/]vector\.hpp$'
                continue
            }
            if (-not $inHeader) { continue }
            if ($row -match '^\s*([^:]+):\s*(\d+):(.*)$') {
                $count = $Matches[1].Trim()
                $n = [int]$Matches[2]
                $source = $Matches[3]   # DİQQƏT: növbəti -notmatch $Matches-i dəyişir
                if ($n -eq 0 -or $count -eq '-') { continue }
                $executed = $count -notmatch '^#+$|^=+$'   # ##### = icra olunmayıb
                $text[$n] = $source
                if ($lines.ContainsKey($n)) { $lines[$n] = $lines[$n] -or $executed }
                else { $lines[$n] = $executed }
            }
        }
    }
} finally {
    Pop-Location
}

$total = $lines.Count
$covered = ($lines.Values | Where-Object { $_ }).Count
$percent = if ($total) { [math]::Round(100.0 * $covered / $total, 1) } else { 0 }
"include/mystl/vector.hpp: $covered / $total executable lines covered ($percent%)"
$missed = $lines.Keys | Where-Object { -not $lines[$_] } | Sort-Object
if ($missed) {
    'Not covered:'
    foreach ($n in $missed) { '  {0,4}: {1}' -f $n, $text[$n].Trim() }
}
