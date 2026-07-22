 [CmdletBinding()]
 param(
     [Parameter(Mandatory = $true)]
     [string]$Action,         # "detect" or "fix"

     [Parameter(Mandatory = $true)]
     [string]$PrtPath,

     [string]$DiagnosisPath = "",

     [string]$OutputDir = "",

     [string]$Threshold = "2.5",

     [string]$UgVersion = "UG2306",

     [string]$McpUrl = "http://112.132.229.234:8017/nxcdd"
 )

 Set-StrictMode -Version Latest
 $ErrorActionPreference = "Stop"

 # ===== 路径配置 =====
 $HarnessRoot = Split-Path -Parent $PSScriptRoot
 $StandardsDir = Join-Path $HarnessRoot "knowledge\standards"
 $ReportsDir = Join-Path $HarnessRoot "workspace\reports"
 $ScriptsDir = Join-Path $HarnessRoot "scripts"

 if ([string]::IsNullOrWhiteSpace($OutputDir)) {
     $OutputDir = $ReportsDir
 }
 [void][System.IO.Directory]::CreateDirectory($OutputDir)

 # ===== 编译函数 =====
 function Compile-Dll {
     param(
         [string]$SourceFile,
         [string]$OutputName
     )

     $dllPath = Join-Path $StandardsDir $OutputName

     # 读取源码
     if (-not (Test-Path -LiteralPath $SourceFile -PathType Leaf)) {
         throw "Source file not found: $SourceFile"
     }
     $sourceCode = Get-Content -LiteralPath $SourceFile -Raw -Encoding UTF8

     # MCP 编译请求
     $compilePayload = @{
         jsonrpc = "2.0"
         id = 1
         method = "tools/call"
         params = @{
             name = "compile_nx_code"
             arguments = @{
                 code = $sourceCode
                 ugVersion = $UgVersion
             }
         }
     }

     $headers = @{
         "Accept" = "application/json, text/event-stream"
         "Content-Type" = "application/json"
     }

     try {
         $response = Invoke-WebRequest -UseBasicParsing -Method Post `
             -Uri $McpUrl -Headers $headers `
             -Body ($compilePayload | ConvertTo-Json -Depth 10 -Compress) `
             -TimeoutSec 120

         $content = $response.Content
         Write-Output "MCP Response: $content"

         # 解析结果（简化版）
         if ($content -match '"status"\s*:\s*"success"') {
             Write-Output "Compilation succeeded"

             # 尝试从 response 中提取 dllPath
             $dllPathMatch = [regex]::Match($content, '"dllPath"\s*:\s*"([^"]+)"')
             if ($dllPathMatch.Success) {
                 $remoteDll = $dllPathMatch.Groups[1].Value
                 Write-Output "Remote DLL: $remoteDll"

                 # 使用 download_dll.exe 下载
                 $downloader = Join-Path $ScriptsDir "download_dll.exe"
                 if (Test-Path $downloader) {
                     & $downloader $remoteDll $StandardsDir
                     Write-Output "DLL downloaded to $StandardsDir"
                 } else {
                     Write-Warning "download_dll.exe not found, assuming DLL is at $dllPath"
                 }
             }

             return $true
         } else {
             Write-Error "Compilation failed"
             Write-Output $content
             return $false
         }
     }
     catch {
         Write-Error "MCP call failed: $_"
         return $false
     }
 }

 # ===== 执行函数 =====
 function Run-Detect {
     param([string]$Prt, [string]$OutDir)

     $timestamp = Get-Date -Format "yyyyMMdd_HHmmss"
     $outputJson = Join-Path $OutDir "diagnosis_$timestamp.json"
     $dllPath = Join-Path $StandardsDir "detect_sharp_steel.dll"

     if (-not (Test-Path $dllPath)) {
         throw "Detection DLL not found. Run with -Action compile first."
     }

     # 设置环境变量
     $env:STEEL_PRT_PATH = $Prt
     $env:STEEL_OUTPUT_PATH = $outputJson

     # 执行 DLL — 使用 playdll_v2.exe
     $playdll = Join-Path $StandardsDir "playdll_v2.exe"
     if (Test-Path $playdll) {
         Write-Output "Executing: $playdll $dllPath"
         & $playdll $dllPath
     } else {
         Write-Warning "playdll_v2.exe not found. DLL must be executed in NX manually."
         Write-Output "Set env STEEL_PRT_PATH=$Prt"
         Write-Output "Set env STEEL_OUTPUT_PATH=$outputJson"
         Write-Output "Run NX and execute: $dllPath"
     }

     Write-Output "Diagnosis output: $outputJson"
     return $outputJson
 }

 function Run-Fix {
     param([string]$Prt, [string]$Diag, [string]$OutDir)

     $timestamp = Get-Date -Format "yyyyMMdd_HHmmss"
     $outputPrt = Join-Path $OutDir "fixed_${timestamp}.prt"
     $dllPath = Join-Path $StandardsDir "fix_sharp_steel.dll"

     if (-not (Test-Path $dllPath)) {
         throw "Fix DLL not found. Compile first."
     }

     $env:STEEL_PRT_PATH = $Prt
     $env:STEEL_DIAG_PATH = $Diag
     $env:STEEL_OUTPUT_PATH = $outputPrt

     $playdll = Join-Path $StandardsDir "playdll_v2.exe"
     if (Test-Path $playdll) {
         Write-Output "Executing: $playdll $dllPath"
         & $playdll $dllPath
     } else {
         Write-Warning "playdll_v2.exe not found. Manual NX execution required."
     }

     Write-Output "Fixed PRT: $outputPrt"
     return $outputPrt
 }

 # ===== 主流程 =====

 Write-Output "===== Sharp Steel Review Harness ====="
 Write-Output "Action: $Action"
 Write-Output "PRT: $PrtPath"

 switch ($Action.ToLower()) {
     "compile-detect" {
         $srcFile = Join-Path $StandardsDir "detect_sharp_steel.cpp"
         Compile-Dll -SourceFile $srcFile -OutputName "detect_sharp_steel.dll"
     }
     "compile-fix" {
         $srcFile = Join-Path $StandardsDir "fix_sharp_steel.cpp"
         Compile-Dll -SourceFile $srcFile -OutputName "fix_sharp_steel.dll"
     }
     "detect" {
         $diag = Run-Detect -Prt $PrtPath -OutDir $OutputDir
         Write-Output "Detect completed. Report: $diag"
         # 输出JSON路径供AI读取
         $reportPathVar = "STEEL_REPORT_PATH=$diag"
         Write-Output $reportPathVar
     }
     "fix" {
         if ([string]::IsNullOrWhiteSpace($DiagnosisPath)) {
             # 找最新的诊断报告
             $latestDiag = Get-ChildItem -Path $ReportsDir -Filter "diagnosis_*.json" |
                           Sort-Object LastWriteTime -Descending |
                           Select-Object -First 1
             if ($latestDiag) {
                 $DiagnosisPath = $latestDiag.FullName
                 Write-Output "Using latest diagnosis: $DiagnosisPath"
             } else {
                 throw "No diagnosis found. Run detect first."
             }
         }
         $result = Run-Fix -Prt $PrtPath -Diag $DiagnosisPath -OutDir $OutputDir
         Write-Output "Fix completed. Result: $result"
     }
     default {
         Write-Error "Unknown action: $Action. Use: compile-detect, compile-fix, detect, fix"
     }
 }

 Write-Output "===== Done ====="
