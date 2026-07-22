param([string]$BuildType = "detect")
$ErrorActionPreference = "Stop"
$NX_BASE = "E:\Program Files\UG_NX2306\Siemens\NX2306"
$VS_PATH = "E:\Program Files\Microsoft Visual Studio\2022\Community"
$SCRIPTDIR = Split-Path -Parent $PSCommandPath
if ($BuildType -eq "detect") {
    $src = Join-Path $SCRIPTDIR "..\knowledge\standards\detect_sharp_steel.cpp"
    $dll = Join-Path $SCRIPTDIR "..\knowledge\standards\detect_sharp_steel.dll"
} else {
    $src = Join-Path $SCRIPTDIR "..\knowledge\standards\fix_sharp_steel.cpp"
    $dll = Join-Path $SCRIPTDIR "..\knowledge\standards\fix_sharp_steel.dll"
}
Write-Output "===== NX C++ DLL Compile ====="
Write-Output "Source: $src"
Write-Output "Output: $dll"
if (!(Test-Path $src)) { Write-Error "NOT FOUND"; exit 1 }
$vcvars = "$VS_PATH\VC\Auxiliary\Build\vcvars64.bat"
if (!(Test-Path $vcvars)) { Write-Error "vcvars NOT FOUND"; exit 1 }
$bat = @"
call "$vcvars"
cl /LD /MD /O2 /W0 /utf-8 /I"$NX_BASE\UGOPEN" /I"$NX_BASE\UGOPEN\NXOpen" /Fe"$dll" "$src" /link /LIBPATH:"$NX_BASE\UGOPEN" libufun.lib libnxopencpp.lib
if errorlevel 1 exit /b 1
"@
$batFile = "$env:TEMP\build_nx_dll.bat"
$bat | Out-File $batFile -Encoding ASCII
Write-Output "Compiling..."
$out = cmd /c $batFile
Remove-Item $batFile -ErrorAction SilentlyContinue
Write-Output $out
if (Test-Path $dll) {
    $kb = [Math]::Round((Get-Item $dll).Length / 1KB)
    Write-Output "`n===== SUCCESS =====`nDLL: $dll ($kb KB)"
} else {
    Write-Error "FAILED"
    exit 1
}