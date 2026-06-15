/*
 * main.c - Llama GUI v2 主入口和核心模块
 *
 * 包含:
 *   - WinMain 入口点
 *   - 主窗口过程
 *   - 模型管理窗口过程
 *   - 模型列表 / 配置文件读写
 *   - 命令执行和工具函数
 */

#include "common.h"

/* ========== 全局变量定义 ========== */
ModelInfo     g_models[MAX_MODELS] = {0};
int           g_modelCount = 0;
RunConfig     g_runCfg = {0};
ConvertConfig g_convCfg = {0};
HINSTANCE     g_hInst = NULL;
HFONT         g_hFontNormal = NULL;
HFONT         g_hFontBold = NULL;
HFONT         g_hFontTitle = NULL;
BOOL           g_appExiting = FALSE;

/* 主窗口句柄 (全局，用于隐藏/显示) */
static HWND g_hMainWnd = NULL;

/* 参数帮助文本 */
const wchar_t* PARAM_HELP =
    L"可用参数说明:\r\n"
    L"--lora <路径>             启用 LoRA 模型\r\n"
    L"-b, --batch-size <N>      批处理大小，影响 Prompt 处理速度\r\n"
    L"--mlock                   锁定内存，防止被交换到虚拟内存\r\n"
    L"--rpc <地址>              分布式推理，连接其他机器后端\r\n"
    L"--top-k <N>               Top-K 采样 (默认: 40)\r\n"
    L"--top-p <0~1>             Top-P 核采样 (默认: 0.9)\r\n"
    L"--min-p <0~1>             Min-P 采样\r\n"
    L"--ignore-eos              忽略结束符，强制继续生成\r\n"
    L"--dry-multiplier <N>      DRY 采样倍率\r\n"
    L"-p, --prompt <文本>       设置初始 Prompt\r\n"
    L"-f, --file <路径>         从文件读取 Prompt\r\n"
    L"-sys, --system-prompt     设置系统提示词\r\n"
    L"-sysf, --system-prompt-file 从文件加载系统提示词\r\n"
    L"-mli, --multiline-input   多行输入模式\r\n"
    L"-r, --reverse-prompt      反向提示，遇到特定文字暂停\r\n"
    L"--chat-template <模板>    手动指定对话模板\r\n"
    L"--reasoning-format <fmt>  控制思考过程展示方式";

/* ========== 辅助转换函数 ========== */

void ToUTF8(const wchar_t* src, char* dst, int dstSize) {
    WideCharToMultiByte(CP_UTF8, 0, src, -1, dst, dstSize, NULL, NULL);
}

void ToWide(const char* src, wchar_t* dst, int dstSize) {
    MultiByteToWideChar(CP_UTF8, 0, src, -1, dst, dstSize);
}

/* ========== 应用程序目录 (基于 exe 位置) ========== */

static char g_appDir[PATH_LEN] = "";

static void InitAppDir(void) {
    char exePath[PATH_LEN];
    GetModuleFileNameA(NULL, exePath, PATH_LEN);
    /* 去掉文件名，保留目录 */
    char* lastSlash = strrchr(exePath, '\\');
    if (lastSlash) {
        *(lastSlash + 1) = '\0';
        strncpy(g_appDir, exePath, PATH_LEN - 1);
    } else {
        g_appDir[0] = '\0';
    }
}

/* 根据 exe 所在目录构造配置文件的完整路径 */
static void MakeConfigPath(const char* filename, char* out, int outSize) {
    if (g_appDir[0]) {
        snprintf(out, outSize, "%s%s", g_appDir, filename);
    } else {
        strncpy(out, filename, outSize - 1);
        out[outSize - 1] = '\0';
    }
}

/* ========== 模型列表读写 ========== */

void LoadModels(void) {
    char filePath[PATH_LEN];
    MakeConfigPath(MODELS_FILE, filePath, PATH_LEN);
    FILE* fp = fopen(filePath, "r");
    if (!fp) {
        g_modelCount = 0;
        SaveModels();
        return;
    }

    char line[MAX_BUF];
    if (!fgets(line, sizeof(line), fp)) {
        fclose(fp);
        g_modelCount = 0;
        return;
    }
    g_modelCount = atoi(line);
    if (g_modelCount > MAX_MODELS) g_modelCount = MAX_MODELS;
    if (g_modelCount < 0) g_modelCount = 0;

    for (int i = 0; i < g_modelCount; i++) {
        if (!fgets(line, sizeof(line), fp)) break;
        line[strcspn(line, "\r\n")] = '\0';
        strncpy(g_models[i].name, line, MAX_NAME - 1);
        g_models[i].name[MAX_NAME - 1] = '\0';

        if (!fgets(line, sizeof(line), fp)) break;
        line[strcspn(line, "\r\n")] = '\0';
        strncpy(g_models[i].path, line, PATH_LEN - 1);
        g_models[i].path[PATH_LEN - 1] = '\0';
    }
    fclose(fp);
}

void SaveModels(void) {
    char filePath[PATH_LEN];
    MakeConfigPath(MODELS_FILE, filePath, PATH_LEN);
    FILE* fp = fopen(filePath, "w");
    if (!fp) {
        wchar_t wpath[PATH_LEN];
        ToWide(filePath, wpath, PATH_LEN);
        wchar_t msg[PATH_LEN + 64];
        swprintf(msg, PATH_LEN + 64, L"无法写入模型文件:\r\n%s", wpath);
        MessageBoxW(NULL, msg, L"错误", MB_ICONERROR);
        return;
    }
    fprintf(fp, "%d\n", g_modelCount);
    for (int i = 0; i < g_modelCount; i++) {
        fprintf(fp, "%s\n", g_models[i].name);
        fprintf(fp, "%s\n", g_models[i].path);
    }
    fflush(fp);
    fclose(fp);
}

/* ========== 运行配置读写 ========== */

void LoadRunConfig(void) {
    /* 设置默认值 */
    strcpy(g_runCfg.mode, "命令行");
    g_runCfg.model[0] = '\0';
    strcpy(g_runCfg.ctxSize, "8192");
    strcpy(g_runCfg.ngl, "48");
    strcpy(g_runCfg.temp, "0.7");
    strcpy(g_runCfg.repeatPenalty, "1.1");
    strcpy(g_runCfg.n, "2048");
    strcpy(g_runCfg.port, "8081");
    g_runCfg.mli = TRUE;
    g_runCfg.extraEnabled = FALSE;
    g_runCfg.extraParams[0] = '\0';
    g_runCfg.mtp = FALSE;
    g_runCfg.useLlamaPath = FALSE;
    g_runCfg.llamaBinPath[0] = '\0';

    char filePath[PATH_LEN];
    MakeConfigPath(CONFIG_FILE, filePath, PATH_LEN);
    FILE* fp = fopen(filePath, "r");
    if (!fp) return;

    char line[MAX_BUF];
    int field = 0;
    while (fgets(line, sizeof(line), fp) && field < 14) {
        line[strcspn(line, "\r\n")] = '\0';
        switch (field) {
            case 0: strncpy(g_runCfg.mode, line, 31); break;
            case 1: strncpy(g_runCfg.model, line, 127); break;
            case 2: strncpy(g_runCfg.ctxSize, line, 31); break;
            case 3: strncpy(g_runCfg.ngl, line, 31); break;
            case 4: strncpy(g_runCfg.temp, line, 31); break;
            case 5: strncpy(g_runCfg.repeatPenalty, line, 31); break;
            case 6: strncpy(g_runCfg.n, line, 31); break;
            case 7: strncpy(g_runCfg.port, line, 31); break;
            case 8: g_runCfg.mli = (atoi(line) != 0); break;
            case 9: g_runCfg.extraEnabled = (atoi(line) != 0); break;
            case 10: strncpy(g_runCfg.extraParams, line, MAX_BUF - 1); break;
            case 11: g_runCfg.mtp = (atoi(line) != 0); break;
            case 12: g_runCfg.useLlamaPath = (atoi(line) != 0); break;
            case 13: strncpy(g_runCfg.llamaBinPath, line, PATH_LEN - 1); break;
        }
        field++;
    }
    fclose(fp);

    /* 确保字符串都以 null 结尾 */
    g_runCfg.mode[31] = '\0';
    g_runCfg.model[127] = '\0';
    g_runCfg.ctxSize[31] = '\0';
    g_runCfg.ngl[31] = '\0';
    g_runCfg.temp[31] = '\0';
    g_runCfg.repeatPenalty[31] = '\0';
    g_runCfg.n[31] = '\0';
    g_runCfg.port[31] = '\0';
    g_runCfg.extraParams[MAX_BUF - 1] = '\0';
}

void SaveRunConfig(void) {
    char filePath[PATH_LEN];
    MakeConfigPath(CONFIG_FILE, filePath, PATH_LEN);
    FILE* fp = fopen(filePath, "w");
    if (!fp) return;
    fprintf(fp, "%s\n", g_runCfg.mode);
    fprintf(fp, "%s\n", g_runCfg.model);
    fprintf(fp, "%s\n", g_runCfg.ctxSize);
    fprintf(fp, "%s\n", g_runCfg.ngl);
    fprintf(fp, "%s\n", g_runCfg.temp);
    fprintf(fp, "%s\n", g_runCfg.repeatPenalty);
    fprintf(fp, "%s\n", g_runCfg.n);
    fprintf(fp, "%s\n", g_runCfg.port);
    fprintf(fp, "%d\n", g_runCfg.mli ? 1 : 0);
    fprintf(fp, "%d\n", g_runCfg.extraEnabled ? 1 : 0);
    fprintf(fp, "%s\n", g_runCfg.extraParams);
    fprintf(fp, "%d\n", g_runCfg.mtp ? 1 : 0);
    fprintf(fp, "%d\n", g_runCfg.useLlamaPath ? 1 : 0);
    fprintf(fp, "%s\n", g_runCfg.llamaBinPath);
    fflush(fp);
    fclose(fp);
}

/* ========== 转换配置读写 ========== */

void LoadConvertConfig(void) {
    strcpy(g_convCfg.outtype, "f16");
    g_convCfg.llamaPath[0] = '\0';
    g_convCfg.modelSrcPath[0] = '\0';
    g_convCfg.loraSrcPath[0] = '\0';
    g_convCfg.modelDstPath[0] = '\0';

    char cvtPath[PATH_LEN];
    MakeConfigPath(CONVERT_CONFIG_FILE, cvtPath, PATH_LEN);
    FILE* fp = fopen(cvtPath, "r");
    if (!fp) return;

    char line[MAX_BUF];
    int field = 0;
    while (fgets(line, sizeof(line), fp)) {
        line[strcspn(line, "\r\n")] = '\0';
        /* 跳过注释行和空行 */
        if (line[0] == '#' || line[0] == '\0') continue;

        switch (field) {
            case 0: strncpy(g_convCfg.outtype, line, 63); break;
            case 1: strncpy(g_convCfg.llamaPath, line, PATH_LEN - 1); break;
            case 2: strncpy(g_convCfg.modelSrcPath, line, PATH_LEN - 1); break;
            case 3: strncpy(g_convCfg.modelDstPath, line, PATH_LEN - 1); break;
            case 4: strncpy(g_convCfg.loraSrcPath, line, PATH_LEN - 1); break;
        }
        field++;
    }
    fclose(fp);

    g_convCfg.outtype[63] = '\0';
    g_convCfg.llamaPath[PATH_LEN - 1] = '\0';
    g_convCfg.modelSrcPath[PATH_LEN - 1] = '\0';
    g_convCfg.modelDstPath[PATH_LEN - 1] = '\0';
    g_convCfg.loraSrcPath[PATH_LEN - 1] = '\0';
}

void SaveConvertConfig(void) {
    char cvtSavePath[PATH_LEN];
    MakeConfigPath(CONVERT_CONFIG_FILE, cvtSavePath, PATH_LEN);
    FILE* fp = fopen(cvtSavePath, "w");
    if (!fp) return;
    fprintf(fp, "# outtype 参数\n");
    fprintf(fp, "%s\n", g_convCfg.outtype);
    fprintf(fp, "# llama.cpp 路径 (文件夹)\n");
    fprintf(fp, "%s\n", g_convCfg.llamaPath);
    fprintf(fp, "# 模型源文件路径 (HF 格式文件夹)\n");
    fprintf(fp, "%s\n", g_convCfg.modelSrcPath);
    fprintf(fp, "# 转换后模型存储路径\n");
    fprintf(fp, "%s\n", g_convCfg.modelDstPath);
    fprintf(fp, "# LoRA 源文件路径 (可为空)\n");
    fprintf(fp, "%s\n", g_convCfg.loraSrcPath);
    fflush(fp);
    fclose(fp);
}

/* ========== 命令执行 ========== */

void ExecuteCommand(const char* cmd) {
    char fullCmd[MAX_CMD];
    snprintf(fullCmd, sizeof(fullCmd), "cmd /k \"%s\"", cmd);

    STARTUPINFOA si = {0};
    PROCESS_INFORMATION pi = {0};
    si.cb = sizeof(si);

    if (!CreateProcessA(NULL, fullCmd, NULL, NULL, FALSE,
                        CREATE_NEW_CONSOLE, NULL, NULL, &si, &pi)) {
        wchar_t wmsg[512];
        swprintf(wmsg, 512, L"无法启动命令!\r\n\r\n命令: %hs", cmd);
        MessageBoxW(NULL, wmsg, L"错误", MB_ICONERROR);
        return;
    }

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
}

void OpenBrowserAfterDelay(const char* port) {
    /* 在独立线程中延迟打开浏览器 */
    Sleep(3000);
    char url[256];
    snprintf(url, sizeof(url), "http://127.0.0.1:%s", port);
    ShellExecuteA(NULL, "open", url, NULL, NULL, SW_SHOWNORMAL);
}

/* ========== 文件/文件夹浏览对话框 ========== */

BOOL BrowseForFolder(HWND hwnd, const wchar_t* title, wchar_t* outPath, DWORD outSize) {
    BROWSEINFOW bi = {0};
    bi.hwndOwner = hwnd;
    bi.lpszTitle = title;
    bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;

    LPITEMIDLIST pidl = SHBrowseForFolderW(&bi);
    if (!pidl) return FALSE;

    BOOL result = SHGetPathFromIDListW(pidl, outPath);
    CoTaskMemFree(pidl);

    if (result) {
        /* 确保不截断 */
        outPath[outSize - 1] = L'\0';
    }
    return result;
}

BOOL BrowseForFile(HWND hwnd, const wchar_t* title, const wchar_t* filter,
                   wchar_t* outPath, DWORD outSize) {
    OPENFILENAMEW ofn = {0};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hwnd;
    ofn.lpstrTitle = title;
    ofn.lpstrFilter = filter;
    ofn.lpstrFile = outPath;
    ofn.nMaxFile = outSize;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_HIDEREADONLY | OFN_PATHMUSTEXIST;

    /* 确保第一个字符是 null 终止符 */
    if (outSize > 0) outPath[0] = L'\0';

    return GetOpenFileNameW(&ofn);
}

BOOL BrowseForSaveFile(HWND hwnd, const wchar_t* title, const wchar_t* filter,
                       const wchar_t* defExt, wchar_t* outPath, DWORD outSize) {
    OPENFILENAMEW ofn = {0};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hwnd;
    ofn.lpstrTitle = title;
    ofn.lpstrFilter = filter;
    ofn.lpstrDefExt = defExt;
    ofn.lpstrFile = outPath;
    ofn.nMaxFile = outSize;
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_HIDEREADONLY | OFN_PATHMUSTEXIST;

    if (outSize > 0) outPath[0] = L'\0';

    return GetSaveFileNameW(&ofn);
}

/* ========== 便捷控件创建函数 ========== */

HWND CreateLabel(HWND parent, const wchar_t* text,
                 int x, int y, int w, int height) {
    HWND hwndCtrl = CreateWindowExW(0, L"STATIC", text,
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        x, y, w, height, parent, NULL, g_hInst, NULL);
    if (g_hFontNormal) SendMessageW(hwndCtrl, WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);
    return hwndCtrl;
}

HWND CreateEdit(HWND parent, const wchar_t* defaultText,
                int x, int y, int w, int height, int id) {
    HWND hwndCtrl = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", defaultText,
        WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
        x, y, w, height, parent, (HMENU)(INT_PTR)id, g_hInst, NULL);
    if (g_hFontNormal) SendMessageW(hwndCtrl, WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);
    return hwndCtrl;
}

HWND CreateButton(HWND parent, const wchar_t* text,
                  int x, int y, int w, int height, int id) {
    HWND hwndCtrl = CreateWindowExW(0, L"BUTTON", text,
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        x, y, w, height, parent, (HMENU)(INT_PTR)id, g_hInst, NULL);
    if (g_hFontNormal) SendMessageW(hwndCtrl, WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);
    return hwndCtrl;
}

HWND CreateCombo(HWND parent, int x, int y, int w, int height,
                 int id, BOOL dropdownList) {
    DWORD style = WS_CHILD | WS_VISIBLE | CBS_AUTOHSCROLL | WS_VSCROLL;
    if (dropdownList) {
        style |= CBS_DROPDOWNLIST;
    } else {
        style |= CBS_DROPDOWN;
    }
    HWND hwndCtrl = CreateWindowExW(0, L"COMBOBOX", L"",
        style, x, y, w, height, parent, (HMENU)(INT_PTR)id, g_hInst, NULL);
    if (g_hFontNormal) SendMessageW(hwndCtrl, WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);
    return hwndCtrl;
}

HWND CreateGroupBox(HWND parent, const wchar_t* text,
                    int x, int y, int w, int height) {
    HWND hwndCtrl = CreateWindowExW(0, L"BUTTON", text,
        WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
        x, y, w, height, parent, NULL, g_hInst, NULL);
    if (g_hFontNormal) SendMessageW(hwndCtrl, WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);
    return hwndCtrl;
}

HWND CreateCheckbox(HWND parent, const wchar_t* text,
                    int x, int y, int w, int height, int id) {
    HWND hwndCtrl = CreateWindowExW(0, L"BUTTON", text,
        WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
        x, y, w, height, parent, (HMENU)(INT_PTR)id, g_hInst, NULL);
    if (g_hFontNormal) SendMessageW(hwndCtrl, WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);
    return hwndCtrl;
}

/* ========== 字体管理 ========== */

void InitFonts(void) {
    g_hFontNormal = CreateFontW(14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, L"微软雅黑");

    g_hFontBold = CreateFontW(14, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, L"微软雅黑");

    g_hFontTitle = CreateFontW(18, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, L"微软雅黑");

    /* 如果微软雅黑不可用，使用系统默认字体 */
    if (!g_hFontNormal) g_hFontNormal = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
    if (!g_hFontBold)   g_hFontBold   = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
    if (!g_hFontTitle)  g_hFontTitle  = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
}

void CleanupFonts(void) {
    if (g_hFontNormal && g_hFontNormal != (HFONT)GetStockObject(DEFAULT_GUI_FONT))
        DeleteObject(g_hFontNormal);
    if (g_hFontBold && g_hFontBold != (HFONT)GetStockObject(DEFAULT_GUI_FONT))
        DeleteObject(g_hFontBold);
    if (g_hFontTitle && g_hFontTitle != (HFONT)GetStockObject(DEFAULT_GUI_FONT))
        DeleteObject(g_hFontTitle);
}

/* ========== 窗口居中 ========== */

void CenterWindow(HWND hwnd) {
    RECT rc, rcOwner;
    HWND hOwner = GetWindow(hwnd, GW_OWNER);
    if (!hOwner) hOwner = GetDesktopWindow();

    GetWindowRect(hOwner, &rcOwner);
    GetWindowRect(hwnd, &rc);

    int w = rc.right - rc.left;
    int h = rc.bottom - rc.top;
    int x = rcOwner.left + ((rcOwner.right - rcOwner.left) - w) / 2;
    int y = rcOwner.top + ((rcOwner.bottom - rcOwner.top) - h) / 2;

    SetWindowPos(hwnd, NULL, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
}

/* ========== 模态子窗口消息循环 ========== */

void RunModalLoop(HWND hwnd) {
    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);
    CenterWindow(hwnd);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
}

/* ========== 模型管理窗口 ========== */

LRESULT CALLBACK ModelsWindowProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    static HWND hList, hEditName, hEditPath;

    switch (msg) {
    case WM_CREATE: {
        CreateGroupBox(hwnd, L"模型列表", 15, 10, 280, 310);

        hList = CreateWindowExW(WS_EX_CLIENTEDGE, L"LISTBOX", L"",
            WS_CHILD | WS_VISIBLE | WS_VSCROLL | LBS_NOTIFY,
            25, 30, 260, 280, hwnd, (HMENU)(INT_PTR)ID_LST_MODELS, g_hInst, NULL);
        SendMessageW(hList, WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);

        /* 填充模型列表 */
        for (int i = 0; i < g_modelCount; i++) {
            wchar_t wname[MAX_NAME];
            ToWide(g_models[i].name, wname, MAX_NAME);
            SendMessageW(hList, LB_ADDSTRING, 0, (LPARAM)wname);
        }

        CreateGroupBox(hwnd, L"编辑模型", 310, 10, 245, 210);

        CreateLabel(hwnd, L"名称:", 325, 40, 50, 23);
        hEditName = CreateEdit(hwnd, L"", 380, 38, 160, 23, ID_EDIT_MDL_NAME);

        CreateLabel(hwnd, L"路径:", 325, 75, 50, 23);
        hEditPath = CreateEdit(hwnd, L"", 325, 98, 215, 23, ID_EDIT_MDL_PATH);

        CreateButton(hwnd, L"浏览...", 430, 130, 100, 28, ID_BTN_MDL_BROWSE);

        CreateButton(hwnd, L"添加", 325, 170, 70, 30, ID_BTN_MDL_ADD);
        CreateButton(hwnd, L"更新", 405, 170, 70, 30, ID_BTN_MDL_UPDATE);
        CreateButton(hwnd, L"删除", 485, 170, 70, 30, ID_BTN_MDL_DEL);

        CreateButton(hwnd, L"返回", 310, 220, 120, 35, ID_BTN_BACK);
        CreateButton(hwnd, L"关闭", 440, 220, 115, 35, ID_BTN_MDL_CLOSE);

        /* 提示标签 */
        CreateLabel(hwnd, L"提示: 先在左侧选择模型进行编辑,\r\n或填写名称和路径后点击\"添加\"。",
                    310, 270, 245, 40);
        break;
    }

    case WM_COMMAND: {
        int id = LOWORD(wp);
        int code = HIWORD(wp);

        switch (id) {
        case ID_LST_MODELS:
            if (code == LBN_SELCHANGE) {
                int sel = SendMessageW(hList, LB_GETCURSEL, 0, 0);
                if (sel >= 0 && sel < g_modelCount) {
                    wchar_t wname[MAX_NAME], wpath[PATH_LEN];
                    ToWide(g_models[sel].name, wname, MAX_NAME);
                    ToWide(g_models[sel].path, wpath, PATH_LEN);
                    SetWindowTextW(hEditName, wname);
                    SetWindowTextW(hEditPath, wpath);
                }
            }
            break;

        case ID_BTN_MDL_BROWSE: {
            wchar_t wpath[PATH_LEN] = L"";
            if (BrowseForFile(hwnd,
                    L"选择模型文件",
                    L"GGUF 模型文件\0*.gguf\0所有文件\0*.*\0",
                    wpath, PATH_LEN)) {
                SetWindowTextW(hEditPath, wpath);
            }
            break;
        }

        case ID_BTN_MDL_ADD: {
            wchar_t wname[MAX_NAME], wpath[PATH_LEN];
            GetWindowTextW(hEditName, wname, MAX_NAME);
            GetWindowTextW(hEditPath, wpath, PATH_LEN);

            if (wcslen(wname) == 0) {
                MessageBoxW(hwnd, L"请输入模型名称!", L"提示", MB_ICONINFORMATION);
                break;
            }

            if (g_modelCount >= MAX_MODELS) {
                MessageBoxW(hwnd, L"模型数量已达上限!", L"提示", MB_ICONINFORMATION);
                break;
            }

            char name[MAX_NAME], path[PATH_LEN];
            ToUTF8(wname, name, MAX_NAME);
            ToUTF8(wpath, path, PATH_LEN);

            strncpy(g_models[g_modelCount].name, name, MAX_NAME - 1);
            strncpy(g_models[g_modelCount].path, path, PATH_LEN - 1);
            g_models[g_modelCount].name[MAX_NAME - 1] = '\0';
            g_models[g_modelCount].path[PATH_LEN - 1] = '\0';
            g_modelCount++;

            SaveModels();

            SendMessageW(hList, LB_ADDSTRING, 0, (LPARAM)wname);
            SendMessageW(hList, LB_SETCURSEL, g_modelCount - 1, 0);

            SetWindowTextW(hEditName, L"");
            SetWindowTextW(hEditPath, L"");
            MessageBoxW(hwnd, L"模型已添加!", L"提示", MB_OK);
            break;
        }

        case ID_BTN_MDL_UPDATE: {
            int sel = SendMessageW(hList, LB_GETCURSEL, 0, 0);
            if (sel < 0 || sel >= g_modelCount) {
                MessageBoxW(hwnd, L"请先在左侧列表中选择要更新的模型!", L"提示", MB_ICONINFORMATION);
                break;
            }

            wchar_t wname[MAX_NAME], wpath[PATH_LEN];
            GetWindowTextW(hEditName, wname, MAX_NAME);
            GetWindowTextW(hEditPath, wpath, PATH_LEN);

            if (wcslen(wname) == 0) {
                MessageBoxW(hwnd, L"模型名称不能为空!", L"提示", MB_ICONINFORMATION);
                break;
            }

            char name[MAX_NAME], path[PATH_LEN];
            ToUTF8(wname, name, MAX_NAME);
            ToUTF8(wpath, path, PATH_LEN);

            strncpy(g_models[sel].name, name, MAX_NAME - 1);
            strncpy(g_models[sel].path, path, PATH_LEN - 1);
            g_models[sel].name[MAX_NAME - 1] = '\0';
            g_models[sel].path[PATH_LEN - 1] = '\0';

            SaveModels();

            /* 更新列表项 */
            SendMessageW(hList, LB_DELETESTRING, sel, 0);
            SendMessageW(hList, LB_INSERTSTRING, sel, (LPARAM)wname);
            SendMessageW(hList, LB_SETCURSEL, sel, 0);

            MessageBoxW(hwnd, L"模型已更新!", L"提示", MB_OK);
            break;
        }

        case ID_BTN_MDL_DEL: {
            int sel = SendMessageW(hList, LB_GETCURSEL, 0, 0);
            if (sel < 0 || sel >= g_modelCount) {
                MessageBoxW(hwnd, L"请先在左侧列表中选择要删除的模型!", L"提示", MB_ICONINFORMATION);
                break;
            }

            wchar_t wname[MAX_NAME];
            ToWide(g_models[sel].name, wname, MAX_NAME);

            wchar_t wmsg[256];
            swprintf(wmsg, 256, L"确定要删除模型 \"%s\" 吗?", wname);
            if (MessageBoxW(hwnd, wmsg, L"确认删除", MB_YESNO | MB_ICONQUESTION) != IDYES)
                break;

            /* 移动后续模型 */
            for (int i = sel; i < g_modelCount - 1; i++) {
                g_models[i] = g_models[i + 1];
            }
            g_modelCount--;

            SaveModels();

            SendMessageW(hList, LB_DELETESTRING, sel, 0);
            SetWindowTextW(hEditName, L"");
            SetWindowTextW(hEditPath, L"");

            MessageBoxW(hwnd, L"模型已删除!", L"提示", MB_OK);
            break;
        }

        case ID_BTN_BACK:
        case ID_BTN_MDL_CLOSE:
            DestroyWindow(hwnd);   /* 返回主窗口 */
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

/* ========== 主窗口 ========== */

LRESULT CALLBACK MainWindowProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_CREATE: {
        /* 标题 */
        HWND hTitle = CreateWindowExW(0, L"STATIC", L"Llama 工具",
            WS_CHILD | WS_VISIBLE | SS_CENTER,
            100, 20, 240, 40, hwnd, NULL, g_hInst, NULL);
        SendMessageW(hTitle, WM_SETFONT, (WPARAM)g_hFontTitle, TRUE);

        /* 运行按钮 */
        HWND hRun = CreateWindowExW(0, L"BUTTON", L"🚀  运行",
            WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
            70, 85, 140, 55, hwnd, (HMENU)(INT_PTR)ID_BTN_RUN, g_hInst, NULL);
        SendMessageW(hRun, WM_SETFONT, (WPARAM)g_hFontBold, TRUE);

        /* 转化按钮 */
        HWND hConv = CreateWindowExW(0, L"BUTTON", L"🔄  转化",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            230, 85, 140, 55, hwnd, (HMENU)(INT_PTR)ID_BTN_CONVERT, g_hInst, NULL);
        SendMessageW(hConv, WM_SETFONT, (WPARAM)g_hFontBold, TRUE);

        /* 模型管理按钮 */
        HWND hModels = CreateWindowExW(0, L"BUTTON", L"📋  模型管理",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            70, 160, 300, 40, hwnd, (HMENU)(INT_PTR)ID_BTN_MODELS, g_hInst, NULL);
        SendMessageW(hModels, WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);

        /* 底部版本信息 */
        CreateLabel(hwnd, L"Llama GUI v2  |  纯 C + Win32 API", 90, 215, 260, 20);

        CenterWindow(hwnd);
        break;
    }

    case WM_COMMAND: {
        int id = LOWORD(wp);

        switch (id) {
        case ID_BTN_RUN: {
            /* 隐藏主窗口，打开运行窗口 */
            ShowWindow(hwnd, SW_HIDE);

            HWND hRun = CreateWindowExW(0, RUN_CLASS, L"Llama 运行",
                WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
                CW_USEDEFAULT, CW_USEDEFAULT, 600, 720,
                NULL, NULL, g_hInst, NULL);

            if (!hRun) {
                MessageBoxW(NULL, L"创建运行窗口失败!", L"错误", MB_ICONERROR);
                ShowWindow(hwnd, SW_SHOW);
                break;
            }

            RunModalLoop(hRun);

            /* 用户点 X 则退出整个程序 */
            if (g_appExiting) {
                PostMessageW(hwnd, WM_CLOSE, 0, 0);
                break;
            }

            /* 返回主窗口 */
            ShowWindow(hwnd, SW_SHOW);
            SetForegroundWindow(hwnd);
            break;
        }

        case ID_BTN_CONVERT: {
            ShowWindow(hwnd, SW_HIDE);

            HWND hConv = CreateWindowExW(0, CONVERT_CLASS, L"Llama 转换",
                WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
                CW_USEDEFAULT, CW_USEDEFAULT, 580, 340,
                NULL, NULL, g_hInst, NULL);

            if (!hConv) {
                MessageBoxW(NULL, L"创建转换窗口失败!", L"错误", MB_ICONERROR);
                ShowWindow(hwnd, SW_SHOW);
                break;
            }

            RunModalLoop(hConv);

            if (g_appExiting) {
                PostMessageW(hwnd, WM_CLOSE, 0, 0);
                break;
            }

            ShowWindow(hwnd, SW_SHOW);
            SetForegroundWindow(hwnd);
            break;
        }

        case ID_BTN_MODELS: {
            ShowWindow(hwnd, SW_HIDE);

            HWND hModels = CreateWindowExW(0, MODELS_CLASS, L"模型管理",
                WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
                CW_USEDEFAULT, CW_USEDEFAULT, 575, 370,
                NULL, NULL, g_hInst, NULL);

            if (!hModels) {
                MessageBoxW(NULL, L"创建模型管理窗口失败!", L"错误", MB_ICONERROR);
                ShowWindow(hwnd, SW_SHOW);
                break;
            }

            RunModalLoop(hModels);

            if (g_appExiting) {
                PostMessageW(hwnd, WM_CLOSE, 0, 0);
                break;
            }

            ShowWindow(hwnd, SW_SHOW);
            SetForegroundWindow(hwnd);
            break;
        }
        }
        break;
    }

    case WM_CLOSE:
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

/* ========== WinMain 入口点 ========== */

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
                   LPSTR lpCmdLine, int nCmdShow) {
    /* 避免未使用参数警告 */
    (void)hPrevInstance;
    (void)lpCmdLine;

    g_hInst = hInstance;

    /* 初始化 exe 目录（所有配置文件将保存在此） */
    InitAppDir();

    /* 加载数据 */
    LoadModels();
    LoadRunConfig();
    LoadConvertConfig();

    /* 初始化字体 */
    InitFonts();

    /* ---- 注册主窗口类 ---- */
    WNDCLASSEXW wcMain = {0};
    wcMain.cbSize        = sizeof(WNDCLASSEXW);
    wcMain.lpfnWndProc   = MainWindowProc;
    wcMain.hInstance     = hInstance;
    wcMain.hCursor       = LoadCursor(NULL, IDC_ARROW);
    wcMain.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wcMain.lpszClassName = MAIN_CLASS;

    if (!RegisterClassExW(&wcMain)) {
        MessageBoxW(NULL, L"注册主窗口类失败!", L"错误", MB_ICONERROR);
        return 1;
    }

    /* ---- 注册运行窗口类 ---- */
    WNDCLASSEXW wcRun = {0};
    wcRun.cbSize        = sizeof(WNDCLASSEXW);
    wcRun.lpfnWndProc   = RunWindowProc;
    wcRun.hInstance     = hInstance;
    wcRun.hCursor       = LoadCursor(NULL, IDC_ARROW);
    wcRun.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wcRun.lpszClassName = RUN_CLASS;

    if (!RegisterClassExW(&wcRun)) {
        MessageBoxW(NULL, L"注册运行窗口类失败!", L"错误", MB_ICONERROR);
        return 1;
    }

    /* ---- 注册转换窗口类 ---- */
    WNDCLASSEXW wcConv = {0};
    wcConv.cbSize        = sizeof(WNDCLASSEXW);
    wcConv.lpfnWndProc   = ConvertWindowProc;
    wcConv.hInstance     = hInstance;
    wcConv.hCursor       = LoadCursor(NULL, IDC_ARROW);
    wcConv.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wcConv.lpszClassName = CONVERT_CLASS;

    if (!RegisterClassExW(&wcConv)) {
        MessageBoxW(NULL, L"注册转换窗口类失败!", L"错误", MB_ICONERROR);
        return 1;
    }

    /* ---- 注册模型管理窗口类 ---- */
    WNDCLASSEXW wcModels = {0};
    wcModels.cbSize        = sizeof(WNDCLASSEXW);
    wcModels.lpfnWndProc   = ModelsWindowProc;
    wcModels.hInstance     = hInstance;
    wcModels.hCursor       = LoadCursor(NULL, IDC_ARROW);
    wcModels.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wcModels.lpszClassName = MODELS_CLASS;

    if (!RegisterClassExW(&wcModels)) {
        MessageBoxW(NULL, L"注册模型管理窗口类失败!", L"错误", MB_ICONERROR);
        return 1;
    }

    /* ---- 创建主窗口 ---- */
    g_hMainWnd = CreateWindowExW(0, MAIN_CLASS, L"Llama 工具",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, 460, 280,
        NULL, NULL, hInstance, NULL);

    if (!g_hMainWnd) {
        MessageBoxW(NULL, L"创建主窗口失败!", L"错误", MB_ICONERROR);
        return 1;
    }

    ShowWindow(g_hMainWnd, nCmdShow);
    UpdateWindow(g_hMainWnd);

    /* ---- 主消息循环 ---- */
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    /* 清理 */
    CleanupFonts();
    return (int)msg.wParam;
}
