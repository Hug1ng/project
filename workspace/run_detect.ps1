$env:STEEL_PRT_PATH = "E:\asd44\Documents\UI\考核\错误分型面1已带产品.prt"
$env:STEEL_OUTPUT_PATH = "E:\AAAA\workspace\reports\diagnosis_20260721_223128.json"

# Add NX bin and UGOPEN to PATH for DLL dependencies
$nxBase = "E:\Program Files\UG_NX2306\Siemens\NX2306"
$env:PATH = "$nxBase\NXBIN;$nxBase\UGOPEN;$env:PATH"

$playdll = "E:\AAAA\knowledge\standards\playdll_v2.exe"
$dll = "E:\AAAA\knowledge\standards\detect_sharp_steel.dll"

Write-Output "PRT: $env:STEEL_PRT_PATH"
Write-Output "Output: $env:STEEL_OUTPUT_PATH"
Write-Output "PATH includes: $nxBin"
Write-Output "Running: $playdll $dll"

& $playdll $dll

if ($LASTEXITCODE -ne 0) {
    Write-Error "playdll exited with code $LASTEXITCODE"
    exit $LASTEXITCODE
}

Write-Output "Detection completed."
Write-Output "Report: $env:STEEL_OUTPUT_PATH"
