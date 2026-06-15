/*
 * convert_window.c - Llama GUI v2 模型转换窗口
 *
 * 提供 HF -> GGUF 模型转换和 LoRA -> GGUF 转换的图形界面。
 * 窗口过程由 main.c 通过模态消息循环调用。
 */

#include "common.h"

LRESULT CALLBACK ConvertWindowProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    static HWND hEditLlama, hEditSrcModel, hEditSrcLora;
    static HWND hEditDstModel, hEditOuttype;
    static HWND hBtnConvModel, hBtnConvLora;

    switch (msg) {
    case WM_CREATE: {
        /* ---- 路径设置 ---- */
        CreateGroupBox(hwnd, L"路径设置", 15, 10, 540, 230);

        /* 第1行: llama.cpp 路径 */
        CreateLabel(hwnd, L"llama.cpp 路径:", 25, 38, 115, 23);

        wchar_t wLlamaPath[PATH_LEN];
        ToWide(g_convCfg.llamaPath, wLlamaPath, PATH_LEN);
        hEditLlama = CreateEdit(hwnd, wLlamaPath, 145, 36, 320, 23, ID_EDIT_LLAMA_PATH);

        CreateButton(hwnd, L"浏览...", 475, 35, 65, 25, ID_BTN_BROWSE_LLAMA);

        /* 第2行: 模型源文件路径 */
        CreateLabel(hwnd, L"模型源文件路径:", 25, 73, 115, 23);

        wchar_t wSrcPath[PATH_LEN];
        ToWide(g_convCfg.modelSrcPath, wSrcPath, PATH_LEN);
        hEditSrcModel = CreateEdit(hwnd, wSrcPath, 145, 71, 320, 23, ID_EDIT_SRC_MODEL);

        CreateButton(hwnd, L"浏览...", 475, 70, 65, 25, ID_BTN_BROWSE_SRC);

        /* 第3行: LoRA 源文件路径 */
        CreateLabel(hwnd, L"LoRA 源文件路径:", 25, 108, 115, 23);

        wchar_t wLoraPath[PATH_LEN];
        ToWide(g_convCfg.loraSrcPath, wLoraPath, PATH_LEN);
        hEditSrcLora = CreateEdit(hwnd, wLoraPath, 145, 106, 320, 23, ID_EDIT_SRC_LORA);

        CreateButton(hwnd, L"浏览...", 475, 105, 65, 25, ID_BTN_BROWSE_LORA);

        /* 第4行: 输出文件路径 */
        CreateLabel(hwnd, L"输出文件路径:", 25, 143, 115, 23);

        wchar_t wDstPath[PATH_LEN];
        ToWide(g_convCfg.modelDstPath, wDstPath, PATH_LEN);
        hEditDstModel = CreateEdit(hwnd, wDstPath, 145, 141, 320, 23, ID_EDIT_DST_MODEL);

        CreateButton(hwnd, L"浏览...", 475, 140, 65, 25, ID_BTN_BROWSE_DST);

        /* 第5行: --outtype 参数 */
        CreateLabel(hwnd, L"--outtype:", 25, 178, 115, 23);

        wchar_t wOuttype[64];
        ToWide(g_convCfg.outtype, wOuttype, 64);
        hEditOuttype = CreateEdit(hwnd, wOuttype, 145, 176, 100, 23, ID_EDIT_OUTTYPE);

        CreateLabel(hwnd, L"(如: f16, f32, q8_0, q4_K_M 等)", 255, 178, 260, 23);

        /* 说明文字 */
        CreateLabel(hwnd,
            L"说明: 转换 HF 格式模型为 GGUF 格式。\r\n"
            L"需要 Python 环境和 llama.cpp 转换脚本。\r\n"
            L"LoRA 路径可留空，不影响转换模型功能。",
            25, 208, 400, 40);

        /* ---- 按钮 ---- */
        CreateButton(hwnd, L"←  返回", 20, 258, 110, 40, ID_BTN_BACK);

        hBtnConvModel = CreateWindowExW(0, L"BUTTON", L"🔄  转换模型",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            145, 258, 190, 40, hwnd, (HMENU)(INT_PTR)ID_BTN_CONV_MODEL, g_hInst, NULL);
        SendMessageW(hBtnConvModel, WM_SETFONT, (WPARAM)g_hFontBold, TRUE);

        hBtnConvLora = CreateWindowExW(0, L"BUTTON", L"🔧  转换 LoRA",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            350, 258, 190, 40, hwnd, (HMENU)(INT_PTR)ID_BTN_CONV_LORA, g_hInst, NULL);
        SendMessageW(hBtnConvLora, WM_SETFONT, (WPARAM)g_hFontBold, TRUE);

        break;
    }

    case WM_COMMAND: {
        int id = LOWORD(wp);
        int code = HIWORD(wp);

        switch (id) {

        /* ---- 返回按钮 ---- */
        case ID_BTN_BACK:
            if (code == BN_CLICKED) {
                DestroyWindow(hwnd);  /* 返回主窗口 */
            }
            break;

        /* ---- 浏览 llama.cpp 路径 (文件夹) ---- */
        case ID_BTN_BROWSE_LLAMA:
            if (code == BN_CLICKED) {
                wchar_t wpath[PATH_LEN] = L"";
                if (BrowseForFolder(hwnd, L"选择 llama.cpp 文件夹", wpath, PATH_LEN)) {
                    SetWindowTextW(hEditLlama, wpath);
                }
            }
            break;

        /* ---- 浏览模型源文件路径 (文件夹) ---- */
        case ID_BTN_BROWSE_SRC:
            if (code == BN_CLICKED) {
                wchar_t wpath[PATH_LEN] = L"";
                if (BrowseForFolder(hwnd, L"选择 HF 格式模型文件夹", wpath, PATH_LEN)) {
                    SetWindowTextW(hEditSrcModel, wpath);
                }
            }
            break;

        /* ---- 浏览 LoRA 源文件路径 (文件夹) ---- */
        case ID_BTN_BROWSE_LORA:
            if (code == BN_CLICKED) {
                wchar_t wpath[PATH_LEN] = L"";
                if (BrowseForFolder(hwnd, L"选择 LoRA 适配器文件夹", wpath, PATH_LEN)) {
                    SetWindowTextW(hEditSrcLora, wpath);
                }
            }
            break;

        /* ---- 浏览输出文件路径 (保存为 GGUF 文件) ---- */
        case ID_BTN_BROWSE_DST:
            if (code == BN_CLICKED) {
                wchar_t wpath[PATH_LEN] = L"";
                if (BrowseForSaveFile(hwnd,
                        L"选择输出文件位置",
                        L"GGUF 文件\0*.gguf\0所有文件\0*.*\0",
                        L"gguf", wpath, PATH_LEN)) {
                    SetWindowTextW(hEditDstModel, wpath);
                }
            }
            break;

        /* ---- 转换模型 ---- */
        case ID_BTN_CONV_MODEL:
            if (code == BN_CLICKED) {
                /* 读取所有输入值 */
                wchar_t wLlamaPath[PATH_LEN], wSrcPath[PATH_LEN];
                wchar_t wDstPath[PATH_LEN], wOuttype[64];

                GetWindowTextW(hEditLlama,   wLlamaPath, PATH_LEN);
                GetWindowTextW(hEditSrcModel, wSrcPath,  PATH_LEN);
                GetWindowTextW(hEditDstModel, wDstPath,  PATH_LEN);
                GetWindowTextW(hEditOuttype,  wOuttype,  64);

                /* 验证必填字段 */
                if (wcslen(wLlamaPath) == 0) {
                    MessageBoxW(hwnd, L"请输入 llama.cpp 路径!", L"提示", MB_ICONWARNING);
                    break;
                }
                if (wcslen(wSrcPath) == 0) {
                    MessageBoxW(hwnd, L"请输入模型源文件路径!", L"提示", MB_ICONWARNING);
                    break;
                }
                if (wcslen(wDstPath) == 0) {
                    MessageBoxW(hwnd, L"请输入输出文件路径!", L"提示", MB_ICONWARNING);
                    break;
                }

                /* 保存到全局配置 */
                ToUTF8(wLlamaPath, g_convCfg.llamaPath, PATH_LEN);
                ToUTF8(wSrcPath,   g_convCfg.modelSrcPath, PATH_LEN);
                ToUTF8(wDstPath,   g_convCfg.modelDstPath, PATH_LEN);
                ToUTF8(wOuttype,   g_convCfg.outtype, 64);
                SaveConvertConfig();

                /* 构建转换命令 */
                char llamaPath[PATH_LEN], srcPath[PATH_LEN];
                char dstPath[PATH_LEN], outtype[64];
                ToUTF8(wLlamaPath, llamaPath, PATH_LEN);
                ToUTF8(wSrcPath,   srcPath,   PATH_LEN);
                ToUTF8(wDstPath,   dstPath,   PATH_LEN);
                ToUTF8(wOuttype,   outtype,   64);

                char cmd[MAX_CMD];
                snprintf(cmd, sizeof(cmd),
                    "cd /d \"%s\" && python convert_hf_to_gguf.py \"%s\" "
                    "--outtype %s --outfile \"%s\"",
                    llamaPath, srcPath, outtype, dstPath);

                /* 确认执行 */
                wchar_t wMsg[512];
                swprintf(wMsg, 512,
                    L"即将执行模型转换:\r\n\r\n"
                    L"llama.cpp: %s\r\n"
                    L"源模型: %s\r\n"
                    L"输出: %s\r\n"
                    L"格式: %s\r\n\r\n"
                    L"是否继续?",
                    wLlamaPath, wSrcPath, wDstPath, wOuttype);

                if (MessageBoxW(hwnd, wMsg, L"确认转换", MB_YESNO | MB_ICONQUESTION) != IDYES)
                    break;

                ExecuteCommand(cmd);
                g_appExiting = TRUE;
                DestroyWindow(hwnd);
            }
            break;

        /* ---- 转换 LoRA ---- */
        case ID_BTN_CONV_LORA:
            if (code == BN_CLICKED) {
                /* 读取所有输入值 */
                wchar_t wLlamaPath[PATH_LEN], wSrcPath[PATH_LEN];
                wchar_t wLoraPath[PATH_LEN], wDstPath[PATH_LEN], wOuttype[64];

                GetWindowTextW(hEditLlama,   wLlamaPath, PATH_LEN);
                GetWindowTextW(hEditSrcModel, wSrcPath,  PATH_LEN);
                GetWindowTextW(hEditSrcLora,  wLoraPath, PATH_LEN);
                GetWindowTextW(hEditDstModel, wDstPath,  PATH_LEN);
                GetWindowTextW(hEditOuttype,  wOuttype,  64);

                /* 验证必填字段 */
                if (wcslen(wLlamaPath) == 0) {
                    MessageBoxW(hwnd, L"请输入 llama.cpp 路径!", L"提示", MB_ICONWARNING);
                    break;
                }
                if (wcslen(wLoraPath) == 0) {
                    MessageBoxW(hwnd, L"请输入 LoRA 源文件路径!", L"提示", MB_ICONWARNING);
                    break;
                }
                if (wcslen(wSrcPath) == 0) {
                    MessageBoxW(hwnd, L"请输入基础模型路径 (--base)!", L"提示", MB_ICONWARNING);
                    break;
                }
                if (wcslen(wDstPath) == 0) {
                    MessageBoxW(hwnd, L"请输入输出文件路径!", L"提示", MB_ICONWARNING);
                    break;
                }

                /* 保存到全局配置 */
                ToUTF8(wLlamaPath, g_convCfg.llamaPath, PATH_LEN);
                ToUTF8(wSrcPath,   g_convCfg.modelSrcPath, PATH_LEN);
                ToUTF8(wLoraPath,  g_convCfg.loraSrcPath, PATH_LEN);
                ToUTF8(wDstPath,   g_convCfg.modelDstPath, PATH_LEN);
                ToUTF8(wOuttype,   g_convCfg.outtype, 64);
                SaveConvertConfig();

                /* 构建转换命令 */
                char llamaPath[PATH_LEN], srcPath[PATH_LEN];
                char loraPath[PATH_LEN], dstPath[PATH_LEN], outtype[64];
                ToUTF8(wLlamaPath, llamaPath, PATH_LEN);
                ToUTF8(wSrcPath,   srcPath,   PATH_LEN);
                ToUTF8(wLoraPath,  loraPath,  PATH_LEN);
                ToUTF8(wDstPath,   dstPath,   PATH_LEN);
                ToUTF8(wOuttype,   outtype,   64);

                char cmd[MAX_CMD];
                snprintf(cmd, sizeof(cmd),
                    "cd /d \"%s\" && python convert_lora_to_gguf.py \"%s\" "
                    "--outfile \"%s\" --outtype %s --base \"%s\"",
                    llamaPath, loraPath, dstPath, outtype, srcPath);

                /* 确认执行 */
                wchar_t wMsg[512];
                swprintf(wMsg, 512,
                    L"即将执行 LoRA 转换:\r\n\r\n"
                    L"llama.cpp: %s\r\n"
                    L"LoRA 源: %s\r\n"
                    L"基础模型: %s\r\n"
                    L"输出: %s\r\n"
                    L"格式: %s\r\n\r\n"
                    L"是否继续?",
                    wLlamaPath, wLoraPath, wSrcPath, wDstPath, wOuttype);

                if (MessageBoxW(hwnd, wMsg, L"确认转换 LoRA", MB_YESNO | MB_ICONQUESTION) != IDYES)
                    break;

                ExecuteCommand(cmd);
                g_appExiting = TRUE;
                DestroyWindow(hwnd);
            }
            break;
        }
        break;
    }

    case WM_CLOSE:
        g_appExiting = TRUE;   /* 点 X 退出整个程序 */
        DestroyWindow(hwnd);
        break;

    case WM_DESTROY:
        PostQuitMessage(0);
        break;

    default:
        return DefWindowProcW(hwnd, msg, wp, lp);
    }
    return 0;
}
