 //==============================================================
 // 分型面尖钢修复 — 自动生成代码
 // 由 AI 基于诊断报告动态生成，每轮循环重新生成
 // 生成时间：{{TIMESTAMP}}
 // 修复轮次：第 {{ROUND}} 轮
 //==============================================================

 #include <uf.h>
 #include <uf_part.h>
 #include <uf_modl.h>
 #include <uf_obj.h>
 #include <uf_ui.h>
 #include <string>
 #include <vector>
 #include <fstream>
 #include <sstream>
 #include <cmath>

 //=========== 修复动作列表（由 AI 根据诊断填充） ============

 {{FIX_ACTIONS}}

 //=========== 主入口 ============

 extern "C" DllExport void ufusr(char* param, int* retcode, int rlen) {
    UF_initialize();

    const char* prtPath = getenv("STEEL_PRT_PATH");
    if (!prtPath) {
        UF_UI_open_listing_window();
        UF_UI_write_listing_window("ERROR: STEEL_PRT_PATH not set");
        UF_terminate();
        *retcode = 1;
        return;
    }

    // 打开 PRT
    tag_t partTag;
    UF_PART_load_status_t errorStatus;
    int rc = UF_PART_open(prtPath, &partTag, &errorStatus);
    if (rc != 0) {
        UF_UI_open_listing_window();
        UF_UI_write_listing_window("ERROR: Cannot open PRT");
        *retcode = 1;
        UF_terminate();
        return;
    }

    // 创建 Body 集合
    int nBodies;
    tag_t* bodies = nullptr;
    UF_MODL_ask_bodies(partTag, &nBodies, &bodies);

    UF_UI_open_listing_window();
    UF_UI_write_listing_window("===== 开始修复 =====");

    // ===== AI 在此处填充修复逻辑 =====

    {{REPAIR_LOGIC}}

    // ===== 修复结束 =====

    UF_free(bodies);

    // 保存
    UF_PART_save(partTag);

    UF_UI_write_listing_window("===== 修复完成 =====");

    UF_PART_close(partTag, 1, 1);
    UF_terminate();
    *retcode = 0;
 }

 extern "C" DllExport int ufsta(char* param, int* retcode, int rlen) {
    const char* prtPath = getenv("STEEL_PRT_PATH");
    if (!prtPath) return 1;

    std::string fullParam(prtPath);
    char* buf = (char*)fullParam.c_str();
    ufusr(buf, retcode, (int)fullParam.length());
    return 0;
 }
