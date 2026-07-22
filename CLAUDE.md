 # 分型面尖钢审查 Harness — AI 协作指引

 ## 项目定位

 这个 harness 的目标非常单一：

 > **对给定的分型面 PRT 文件，自动完成"审查 → 修复 → 再审 → 再修 …… 直到合格"的闭环。**

 这里的闭环包括：
 1. **尖钢检测**：读取 PRT，识别分型面上的尖钢/薄钢区域
 2. **诊断输出**：生成结构化 JSON 报告，标注每个问题区域的位置、严重程度、几何指标
 3. **自动修复**：根据诊断结果生成修复方案，编译修复 DLL，在 NX 中执行
 4. **循环验证**：修复后重新检测，直到无尖钢问题或达到终止条件

 ## AI 的角色

 AI 在这个 harness 中的角色是**循环闭环调度者 + 决策者**：

 1. 判断当前处于循环的哪一步
 2. 驱动检测 DLL 编译和执行
 3. 读取诊断 JSON，理解尖钢问题分布
 4. 制定修复策略，生成修复 C++ 代码
 5. 驱动修复 DLL 编译和执行
 6. 判断何时终止循环（全部通过/不可修复/超过迭代次数）

 ## 循环工作流

 ```text
 [开始] 用户提供 PRT 路径
   │
   ├─ 步骤1: /steel-review run <prt_path>
   │   ├─ 编译 detect_sharp_steel.dll
   │   ├─ 在 NX 中执行检测
   │   ├─ 读取输出 JSON 诊断报告
   │   └─ 判断是否存在尖钢问题
   │
   ├─ [无问题] → 输出审查报告 → 完成
   │
   ├─ [有问题] → 步骤2: /steel-fix plan
   │   ├─ AI 制定修复方案
   │   ├─ 生成修复 C++ 代码
   │   ├─ 编译 fix_sharp_steel.dll
   │   └─ 在 NX 中执行修复
   │
   └─ → 回到步骤1（重新检测）
 ```

 ## 目录结构

 ```
 E:\AAAA/
 ├── CLAUDE.md
 ├── .claude/settings.json
 ├── .claude/commands/steel-review.md
 ├── .claude/commands/steel-fix.md
 ├── knowledge/standards/detect_sharp_steel.cpp
 ├── knowledge/standards/fix_sharp_steel.cpp
 ├── knowledge/standards/sharp_steel_criteria.md
 ├── knowledge/standards/repair_strategies.md
 ├── knowledge/templates/fix_code_template.cpp
 ├── docs/操作指南.md
 ├── scripts/compile-and-run.ps1
 └── workspace/reports/
 ```

 ## 尖钢判定标准（内置）

 | 严重程度 | 条件 | 处理 |
 |---------|------|------|
 | CRITICAL | 最小厚度 < 0.8mm，且为关键受力区 | 必须修复 |
 | WARNING | 0.8mm ≤ 厚度 < 1.5mm，或长宽比 > 6:1 | 建议修复 |
 | INFO | 1.5mm ≤ 厚度 < 2.5mm，或存在锐边 | 记录观察 |
 | PASS | 厚度 ≥ 2.5mm | 无需处理 |

 ## 问题分类

 - **A类（可自动修复）**：薄钢、锐角 → AI 直接生成修复代码
 - **B类（需人工确认）**：修复影响其他特征 → 暂停输出方案
 - **C类（不可自动修复）**：需整体设计变更 → 标记并报告

 ## 终止条件

 - 所有问题 severity 均为 PASS → ✅ 完成
 - 连续 5 轮修复后问题数无减少 → ⚠️ 暂停
 - 出现 C 类问题 → 🚫 标记
 - 用户手动终止 → ✋

 ## AI 默认行为

 - 用户刚打开 → 检查 workspace/ 下 PRT 和报告，建议运行 /steel-review
 - 检测已完成 → 展示问题清单，询问是否修复
 - 修复已完成 → 自动触发新一轮检测，对比报告
 - 修复无进展 → 根因分析，检查是否属 B/C 类问题

 ## 已验证经验

 - 尖钢检测不能只看 faces 数量，要结合面几何形状
 - 同一区域可能前后检测结果不同 → 以最新为准
 - 修复可能引入新问题 → 必须循环验证
 - 多次修复无进展 → 优先怀疑判定标准是否过严
