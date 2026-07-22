@echo off
set "STEEL_PRT_PATH=E:\AAAA\workspace\temp_input.prt"
set "STEEL_OUTPUT_PATH=E:\AAAA\workspace\reports\diagnosis_20260721_230356.json"
set "PATH=E:\Program Files\UG_NX2306\Siemens\NX2306\NXBIN;E:\Program Files\UG_NX2306\Siemens\NX2306\UGOPEN;%PATH%"

echo PRT: %STEEL_PRT_PATH%
echo Output: %STEEL_OUTPUT_PATH%

"E:\AAAA\knowledge\standards\playdll_v2.exe" "E:\AAAA\knowledge\standards\detect_sharp_steel.dll"

echo Exit: %ERRORLEVEL%
