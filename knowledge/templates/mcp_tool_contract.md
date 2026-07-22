 # 分型面尖钢审查 — MCP 工具接口契约

 ## 概述

 本文件定义分型面尖钢审查闭环工作流暴露为 MCP 工具时的接口契约。
 当前阶段通过 Claude Code 的自定义命令（steel-review / steel-fix）间接实现工具调用，
 后续可直接封装为独立 MCP Server，由 AI Agent 自动路由。

 当前调用链路：

 ```text
 Agent (Claude Code)
   │
   ├─ /steel-review run <prt>  →  compile_nx_code (MCP) → detect_sharp_steel.dll → JSON
   │
   └─ /steel-fix plan <json>   →  fix_sharp_steel.dll → playdll → 固定PRT → 重审
 ```

 ## 工具接口定义

 ### 工具 1：detect_sharp_steel

 **功能描述**：对指定 PRT 文件执行分型面尖钢检测，输出结构化诊断报告。

 **MCP 工具名**：`detect_sharp_steel`

 **参数（JSON Schema）**：

 ```json
 {
   "name": "detect_sharp_steel",
   "description": "对 NX 分型面 PRT 执行尖钢检测",
   "parameters": {
     "type": "object",
     "properties": {
       "prt_path": {
         "type": "string",
         "description": "要检测的 PRT 文件绝对路径"
       },
       "output_path": {
         "type": "string",
         "description": "诊断报告输出路径（可选，默认 workspace/reports/diagnosis_{timestamp}.json）"
       },
       "threshold_mm": {
         "type": "number",
         "description": "自定义阈值（可选，默认 2.5mm）"
       }
     },
     "required": ["prt_path"]
   }
 }
 ```

 **返回结果**：

 ```json
 {
   "status": "success",
   "result": {
     "prt_path": "E:\\models\\test.prt",
     "timestamp": "2026-07-22T12:00:00",
     "analysis_summary": {
       "total_faces": 10153,
       "flagged_faces": 655,
       "sharp_steel_regions": 143,
       "overall_verdict": "FAIL"
     },
     "sharp_steel_regions": [
       {
         "region_id": "SS-001",
         "severity": "CRITICAL",
         "aspect_ratio": 13637,
         "thinness": 129.4,
         "face_count": 492,
         "centroid": [323.9, -214.6, -81.7],
         "fix_centroid": [318.2, -210.3, -79.5],
         "suggested_fix": "add_material_patch"
       }
     ],
     "face_statistics": {
       "pass": 7929,
       "info": 1569,
       "warning": 396,
       "critical": 259
     }
   }
 }
 ```

 **异常返回**：

 ```json
 {
   "status": "error",
   "error": {
     "code": "FILE_NOT_FOUND",
     "message": "PRT 文件不存在：E:\\models\\not_found.prt"
   }
 }
 ```

 | 错误码 | 说明 |
 |--------|------|
 | FILE_NOT_FOUND | PRT 路径不存在 |
 | OPEN_FAILED | UF_PART_open 失败 |
 | NO_BODIES | PRT 中无实体可扫描 |
 | INTERNAL_ERROR | NXOpen C++ 异常 |

 ---

 ### 工具 2：fix_sharp_steel

 **功能描述**：基于诊断报告，对尖钢区域执行面偏置修复，输出修复报告。

 **MCP 工具名**：`fix_sharp_steel`

 **参数（JSON Schema）**：

 ```json
 {
   "name": "fix_sharp_steel",
   "description": "基于诊断报告执行尖钢区域修复",
   "parameters": {
     "type": "object",
     "properties": {
       "prt_path": {
         "type": "string",
         "description": "要修复的 PRT 文件绝对路径"
       },
       "diagnosis_path": {
         "type": "string",
         "description": "检测阶段输出的诊断 JSON 路径"
       },
       "output_path": {
         "type": "string",
         "description": "修复后 PRT 输出路径（可选，默认另存为新文件）"
       },
       "offset_amount": {
         "type": "number",
         "description": "面偏置量（mm，可选，默认 0.5）"
       },
       "fix_level": {
         "type": "string",
         "enum": ["CRITICAL", "WARNING", "ALL"],
         "description": "修复级别（可选，默认 CRITICAL）"
       }
     },
     "required": ["prt_path", "diagnosis_path"]
   }
 }
 ```

 **返回结果**：

 ```json
 {
   "status": "success",
   "result": {
     "original_prt": "E:\\models\\test.prt",
     "fixed_prt": "E:\\AAA\\workspace\\fixed_r3_20260722_095433.prt",
     "fix_summary": {
       "total_regions": 20,
       "successful": 12,
       "failed": 8
     },
     "verdict": "部分修复"
   }
 }
 ```

 **异常返回**：

 ```json
 {
   "status": "error",
   "error": {
     "code": "DIAGNOSIS_PARSE_ERROR",
     "message": "诊断 JSON 解析失败，缺少 region_id 字段"
   }
 }
 ```

 | 错误码 | 说明 |
 |--------|------|
 | DIAGNOSIS_NOT_FOUND | 诊断 JSON 路径不存在 |
 | DIAGNOSIS_PARSE_ERROR | JSON 格式错误 |
 | PRT_NOT_FOUND | PRT 文件不存在 |
 | ALL_FIX_FAILED | 所有修复操作都失败 |

 ---

 ### 工具 3：steel_review_loop

 **功能描述**：组合工具 1 + 工具 2，自动执行"检测→修复→重审"循环。

 **MCP 工具名**：`steel_review_loop`

 **参数（JSON Schema）**：

 ```json
 {
   "name": "steel_review_loop",
   "description": "自动执行检测→修复→重审循环",
   "parameters": {
     "type": "object",
     "properties": {
       "prt_path": {
         "type": "string",
         "description": "要处理的 PRT 文件绝对路径"
       },
       "max_rounds": {
         "type": "integer",
         "description": "最大循环轮数（可选，默认 3）"
       },
       "stop_on_pass": {
         "type": "boolean",
         "description": "通过后自动停止（可选，默认 true）"
       }
     },
     "required": ["prt_path"]
   }
 }
 ```

 **返回结果**：

 ```json
 {
   "status": "success",
   "result": {
     "original_prt": "E:\\models\\test.prt",
     "rounds_completed": 3,
     "rounds": [
       {
         "round": 1,
         "verdict": "FAIL",
         "critical_start": 415,
         "critical_end": 259,
         "regions": 143
       },
       {
         "round": 2,
         "verdict": "FAIL",
         "critical_start": 259,
         "critical_end": 259,
         "regions": 142
       }
     ],
     "final_verdict": "PARTIAL",
     "critical_remaining": 259,
     "recommendation": "剩余 259 个 CRITICAL 面不可自动修复，建议人工评审"
   }
 }
 ```

 ---

 ## AIAMD 集成说明

 ### 当前集成方式

 当前通过 Claude Code 的 `steel-review` 和 `steel-fix` 自定义命令实现 Agent 闭环：

 ```text
 用户需求
   ↓
 Agent 理解需求（"检查我的分型面有没有尖钢"）
   ↓
 Agent 调用 steel-review 命令 → 内部调用 detect_sharp_steel.dll
   ↓
 Agent 读取诊断 JSON，判断结果
   ↓
 Agent 调用 steel-fix 命令 → 内部调用 fix_sharp_steel.dll
   ↓
 Agent 读取修复报告，触发重审
   ↓
 Agent 判断是否继续循环或结束
 ```

 ### 升级路径（未来）

 将 detect_sharp_steel 和 fix_sharp_steel 封装为独立 MCP Server：

 ```
 MCP Server: sharp-steel-service
   │
   ├─ tools/detect_sharp_steel   ← 调用 detect DLL
   ├─ tools/fix_sharp_steel      ← 调用 fix DLL
   └─ resources/diagnosis/{id}   ← 诊断报告资源

 注册到 Claude Code 或其他 AI Agent 后，
 可通过自然语言直接触发： "检查这个分型面有没有尖钢"
 ```

 ---

 ## 工具依赖

 | 依赖 | 版本 | 说明 |
 |------|------|------|
 | NX | 2306 | 运行环境 |
 | NXOpen C++ SDK | UGOPEN | 编译与运行 |
 | playdll_v2.exe | — | DLL 执行器 |
 | compile_nx_code | — | 远程编译 MCP 服务 |
 | Visual Studio | 2022 (v143) | 本地编译 |
 | 内网 VPN | — | MCP 服务可达性 |
