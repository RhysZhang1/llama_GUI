/*
 * run_window.c - Llama GUI v2 运行配置窗口
 *
 * "预览命令" → 显示可编辑的完整命令 → "确认运行" → 执行并关闭
 * "返回" → 回到主界面，X → 退出程序
 */

#include "common.h"

/* 参数帮助文本 (定义在 main.c) */
extern const wchar_t* PARAM_HELP;

/* 保存已生成的命令文本，供"确认运行"使用 */
static char g_generatedCmd[MAX_CMD] = "";

LRESULT CALLBACK RunWindowProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    static HWND hComboMode, hComboModel;
    static HWND hEditCtx, hEditNgl, hEditTemp, hEditRepeat, hEditN, hEditPort;
    static HWND hChkMli, hChkMtp, hChkExtra, hEditExtra;
    static HWND hBtnPreview, hBtnExecute, hBtnBack, hBtnHelp;
    static HWND hEditCmdPreview;
    static HWND hLabelTemp, hLabelRepeat, hLabelN;
    static HWND hLabelPort;
    static HWND hHelpText;
    static HWND hChkLlamaPath, hEditLlamaBin;
    static BOOL s_previewShown = FALSE;

    switch (msg) {
    case WM_CREATE: {
        s_previewShown = FALSE;
        g_generatedCmd[0] = '\0';

        /* ======== 运行模式 ======== */
        CreateGroupBox(hwnd, L"运行模式", 15, 10, 560, 55);
        CreateLabel(hwnd, L"模式:", 25, 33, 38, 23);
        hComboMode = CreateCombo(hwnd, 62, 31, 115, 200, ID_CBO_MODE, TRUE);
        SendMessageW(hComboMode, CB_ADDSTRING, 0, (LPARAM)L"命令行");
        SendMessageW(hComboMode, CB_ADDSTRING, 0, (LPARAM)L"网页");

        /* llama.cpp 路径选项 (两个模式都可用) */
        hChkLlamaPath = CreateCheckbox(hwnd, L"llama.cpp 路径:", 195, 33, 130, 23, ID_CHK_LLAMA_PATH);
        wchar_t wBinPath[PATH_LEN];
        ToWide(g_runCfg.llamaBinPath, wBinPath, PATH_LEN);
        hEditLlamaBin = CreateEdit(hwnd, wBinPath, 325, 31, 160, 23, ID_EDIT_LLAMA_BIN);
        CreateButton(hwnd, L"...", 490, 30, 35, 25, ID_BTN_BROWSE_LLAMA_BIN);

        SendMessageW(hChkLlamaPath, BM_SETCHECK,
            g_runCfg.useLlamaPath ? BST_CHECKED : BST_UNCHECKED, 0);
        ShowWindow(hEditLlamaBin, g_runCfg.useLlamaPath ? SW_SHOW : SW_HIDE);

        /* ======== 模型选择 ======== */
        CreateGroupBox(hwnd, L"模型选择", 15, 65, 560, 55);
        CreateLabel(hwnd, L"模型:", 25, 88, 45, 23);
        hComboModel = CreateCombo(hwnd, 75, 86, 480, 300, ID_CBO_MODEL, FALSE);
        for (int i = 0; i < g_modelCount; i++) {
            wchar_t wname[MAX_NAME];
            ToWide(g_models[i].name, wname, MAX_NAME);
            SendMessageW(hComboModel, CB_ADDSTRING, 0, (LPARAM)wname);
        }

        /* ======== 基本参数 ======== */
        CreateGroupBox(hwnd, L"基本参数", 15, 125, 560, 155);

        /* 左列 */
        CreateLabel(hwnd, L"ctx-size (上下文大小):", 25, 152, 150, 23);
        hEditCtx = CreateEdit(hwnd, L"8192", 180, 150, 80, 23, ID_EDIT_CTX);

        CreateLabel(hwnd, L"ngl (GPU 层数):", 25, 185, 120, 23);
        hEditNgl = CreateEdit(hwnd, L"48", 180, 183, 80, 23, ID_EDIT_NGL);

        hLabelTemp = CreateLabel(hwnd, L"温度 (temp):", 25, 218, 100, 23);
        hEditTemp = CreateEdit(hwnd, L"0.7", 180, 216, 80, 23, ID_EDIT_TEMP);

        hLabelRepeat = CreateLabel(hwnd, L"重复惩罚:", 25, 251, 100, 23);
        hEditRepeat = CreateEdit(hwnd, L"1.1", 180, 249, 80, 23, ID_EDIT_REPEAT);

        /* 右列 */
        hLabelN = CreateLabel(hwnd, L"最大生成 (-n):", 300, 152, 110, 23);
        hEditN = CreateEdit(hwnd, L"2048", 420, 150, 80, 23, ID_EDIT_N);

        hLabelPort = CreateLabel(hwnd, L"端口:", 300, 185, 80, 23);
        hEditPort = CreateEdit(hwnd, L"8081", 420, 183, 80, 23, ID_EDIT_PORT);

        /* ======== 选项 ======== */
        CreateGroupBox(hwnd, L"选项", 15, 285, 560, 80);

        hChkMli = CreateCheckbox(hwnd, L"多行输入模式 (-mli)", 25, 308, 175, 23, ID_CHK_MLI);
        hChkMtp = CreateCheckbox(hwnd, L"MTP 推测解码", 210, 308, 130, 23, ID_CHK_MTP);
        hChkExtra = CreateCheckbox(hwnd, L"启用额外参数:", 355, 308, 115, 23, ID_CHK_EXTRA);
        hEditExtra = CreateEdit(hwnd, L"", 470, 306, 90, 23, ID_EDIT_EXTRA);

        /* ======== 帮助文本 / 命令预览 (同一位置, 切换显示) ======== */
        hHelpText = CreateWindowExW(0, L"STATIC", PARAM_HELP,
            WS_CHILD | WS_VISIBLE | SS_LEFT,
            15, 375, 560, 155, hwnd, NULL, g_hInst, NULL);
        SendMessageW(hHelpText, WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);

        /* 命令预览编辑框 (与帮助文本同位置, 初始隐藏) */
        hEditCmdPreview = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
            WS_CHILD | WS_BORDER | ES_MULTILINE |
            ES_AUTOVSCROLL | WS_VSCROLL,
            15, 375, 560, 145, hwnd,
            (HMENU)(INT_PTR)ID_EDIT_CMD_PREVIEW, g_hInst, NULL);
        SendMessageW(hEditCmdPreview, WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);
        ShowWindow(hEditCmdPreview, SW_HIDE);

        /* ======== "确认运行" 按钮 (初始隐藏) ======== */
        hBtnExecute = CreateWindowExW(0, L"BUTTON", L"▶  确认运行",
            WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
            160, 530, 260, 40, hwnd,
            (HMENU)(INT_PTR)ID_BTN_EXECUTE, g_hInst, NULL);
        SendMessageW(hBtnExecute, WM_SETFONT, (WPARAM)g_hFontBold, TRUE);
        ShowWindow(hBtnExecute, SW_HIDE);

        /* ======== 底部按钮行 ======== */
        hBtnBack = CreateWindowExW(0, L"BUTTON", L"←  返回",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            20, 585, 110, 40, hwnd,
            (HMENU)(INT_PTR)ID_BTN_BACK, g_hInst, NULL);
        SendMessageW(hBtnBack, WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);

        hBtnPreview = CreateWindowExW(0, L"BUTTON", L"📋  预览命令",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            160, 585, 160, 40, hwnd,
            (HMENU)(INT_PTR)ID_BTN_PREVIEW, g_hInst, NULL);
        SendMessageW(hBtnPreview, WM_SETFONT, (WPARAM)g_hFontBold, TRUE);

        CreateLabel(hwnd, L"llama-cli --help 查看全部参数", 340, 595, 200, 23);

        hBtnHelp = CreateWindowExW(0, L"BUTTON", L"?",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            530, 585, 40, 40, hwnd,
            (HMENU)(INT_PTR)ID_BTN_HELP, g_hInst, NULL);
        SendMessageW(hBtnHelp, WM_SETFONT, (WPARAM)g_hFontBold, TRUE);

        /* ======== 加载上次配置 ======== */
        BOOL isCli = (strcmp(g_runCfg.mode, "网页") != 0);
        SendMessageW(hComboMode, CB_SETCURSEL, isCli ? 0 : 1, 0);

        if (g_runCfg.model[0] != '\0') {
            int found = 0;
            for (int i = 0; i < g_modelCount; i++) {
                if (strcmp(g_runCfg.model, g_models[i].name) == 0) {
                    SendMessageW(hComboModel, CB_SETCURSEL, i, 0);
                    found = 1;
                    break;
                }
            }
            if (!found) {
                wchar_t wmodel[MAX_NAME];
                ToWide(g_runCfg.model, wmodel, MAX_NAME);
                SetWindowTextW(hComboModel, wmodel);
            }
        }

        {
            wchar_t wbuf[PATH_LEN];
            ToWide(g_runCfg.ctxSize, wbuf, PATH_LEN);
            SetWindowTextW(hEditCtx, wbuf);
            ToWide(g_runCfg.ngl, wbuf, PATH_LEN);
            SetWindowTextW(hEditNgl, wbuf);
            ToWide(g_runCfg.temp, wbuf, PATH_LEN);
            SetWindowTextW(hEditTemp, wbuf);
            ToWide(g_runCfg.repeatPenalty, wbuf, PATH_LEN);
            SetWindowTextW(hEditRepeat, wbuf);
            ToWide(g_runCfg.n, wbuf, PATH_LEN);
            SetWindowTextW(hEditN, wbuf);
            ToWide(g_runCfg.port, wbuf, PATH_LEN);
            SetWindowTextW(hEditPort, wbuf);
            SendMessageW(hChkMli, BM_SETCHECK,
                g_runCfg.mli ? BST_CHECKED : BST_UNCHECKED, 0);
            SendMessageW(hChkMtp, BM_SETCHECK,
                g_runCfg.mtp ? BST_CHECKED : BST_UNCHECKED, 0);
            SendMessageW(hChkExtra, BM_SETCHECK,
                g_runCfg.extraEnabled ? BST_CHECKED : BST_UNCHECKED, 0);
            ToWide(g_runCfg.extraParams, wbuf, MAX_BUF);
            SetWindowTextW(hEditExtra, wbuf);
        }

        /* 初始化模式 UI */
        {
            int showCli = isCli ? SW_SHOW : SW_HIDE;
            int showWeb = isCli ? SW_HIDE : SW_SHOW;
            ShowWindow(hLabelTemp, showCli);
            ShowWindow(hEditTemp, showCli);
            ShowWindow(hLabelRepeat, showCli);
            ShowWindow(hEditRepeat, showCli);
            ShowWindow(hLabelN, showCli);
            ShowWindow(hEditN, showCli);
            ShowWindow(hChkMli, showCli);
            ShowWindow(hChkMtp, showCli);
            ShowWindow(hChkExtra, showCli);
            ShowWindow(hEditExtra, isCli && g_runCfg.extraEnabled ? SW_SHOW : SW_HIDE);
            ShowWindow(hHelpText, showCli);
            ShowWindow(hBtnHelp, showCli);
            ShowWindow(hLabelPort, showWeb);
            ShowWindow(hEditPort, showWeb);
        }

        break;
    }

    case WM_COMMAND: {
        int id = LOWORD(wp);
        int code = HIWORD(wp);

        switch (id) {

        /* ---- 模式切换 ---- */
        case ID_CBO_MODE:
            if (code == CBN_SELCHANGE) {
                int sel = SendMessageW(hComboMode, CB_GETCURSEL, 0, 0);
                BOOL isCli = (sel == 0);
                int sc = isCli ? SW_SHOW : SW_HIDE;
                int sw = isCli ? SW_HIDE : SW_SHOW;

                ShowWindow(hLabelTemp, sc);
                ShowWindow(hEditTemp, sc);
                ShowWindow(hLabelRepeat, sc);
                ShowWindow(hEditRepeat, sc);
                ShowWindow(hLabelN, sc);
                ShowWindow(hEditN, sc);
                ShowWindow(hChkMli, sc);
                ShowWindow(hChkMtp, sc);
                ShowWindow(hChkExtra, sc);
                ShowWindow(hHelpText, sc);
                ShowWindow(hBtnHelp, sc);

                BOOL extraVis = isCli &&
                    (SendMessageW(hChkExtra, BM_GETCHECK, 0, 0) == BST_CHECKED);
                ShowWindow(hEditExtra, extraVis ? SW_SHOW : SW_HIDE);

                ShowWindow(hLabelPort, sw);
                ShowWindow(hEditPort, sw);

                /* 切换模式时关闭预览 */
                if (s_previewShown) {
                    s_previewShown = FALSE;
                    ShowWindow(hEditCmdPreview, SW_HIDE);
                    ShowWindow(hBtnExecute, SW_HIDE);
                    ShowWindow(hHelpText, SW_SHOW);
                    SetWindowTextW(hBtnPreview, L"📋  预览命令");
                }
            }
            break;

        /* ---- llama.cpp 路径复选框 ---- */
        case ID_CHK_LLAMA_PATH:
            if (code == BN_CLICKED) {
                BOOL checked = (SendMessageW(hChkLlamaPath, BM_GETCHECK, 0, 0) == BST_CHECKED);
                ShowWindow(hEditLlamaBin, checked ? SW_SHOW : SW_HIDE);
            }
            break;

        /* ---- 浏览 llama.cpp 文件夹 ---- */
        case ID_BTN_BROWSE_LLAMA_BIN:
            if (code == BN_CLICKED) {
                wchar_t wpath[PATH_LEN] = L"";
                if (BrowseForFolder(hwnd, L"选择 llama.cpp 所在文件夹", wpath, PATH_LEN)) {
                    SetWindowTextW(hEditLlamaBin, wpath);
                }
            }
            break;

        /* ---- 额外参数复选框 ---- */
        case ID_CHK_EXTRA:
            if (code == BN_CLICKED) {
                BOOL checked = (SendMessageW(hChkExtra, BM_GETCHECK, 0, 0) == BST_CHECKED);
                ShowWindow(hEditExtra, checked ? SW_SHOW : SW_HIDE);
            }
            break;

        /* ---- 帮助按钮 ---- */
        case ID_BTN_HELP:
            if (code == BN_CLICKED) {
                ExecuteCommand("llama-cli --help");
            }
            break;

        /* ---- 预览命令 ---- */
        case ID_BTN_PREVIEW:
            if (code == BN_CLICKED) {
                if (s_previewShown) {
                    /* 如果已经显示预览, 取消预览回到参数模式 */
                    s_previewShown = FALSE;
                    ShowWindow(hEditCmdPreview, SW_HIDE);
                    ShowWindow(hBtnExecute, SW_HIDE);
                    ShowWindow(hHelpText, SW_SHOW);
                    SetWindowTextW(hBtnPreview, L"📋  预览命令");
                    break;
                }

                /* 检查模型 */
                if (g_modelCount == 0) {
                    MessageBoxW(hwnd,
                        L"模型列表为空!\r\n\r\n请先添加模型。",
                        L"提示", MB_ICONWARNING);
                    break;
                }

                /* ---- 收集参数 ---- */
                int modeSel = SendMessageW(hComboMode, CB_GETCURSEL, 0, 0);
                BOOL isCliMode = (modeSel == 0);

                wchar_t wmodel[MAX_NAME] = L"";
                GetWindowTextW(hComboModel, wmodel, MAX_NAME);
                char model[MAX_NAME];
                ToUTF8(wmodel, model, MAX_NAME);

                /* 查找模型路径 */
                char modelPath[PATH_LEN] = "";
                BOOL found = FALSE;
                for (int i = 0; i < g_modelCount; i++) {
                    if (strcmp(model, g_models[i].name) == 0) {
                        strncpy(modelPath, g_models[i].path, PATH_LEN - 1);
                        modelPath[PATH_LEN - 1] = '\0';
                        found = TRUE;
                        break;
                    }
                }
                if (!found) {
                    strncpy(modelPath, model, PATH_LEN - 1);
                    modelPath[PATH_LEN - 1] = '\0';
                }

                wchar_t wbuf[PATH_LEN];
                char ctxSize[32], ngl[32], temp[32], repeatPenalty[32];
                char n[32], port[32], extraParams[MAX_BUF];

                GetWindowTextW(hEditCtx, wbuf, PATH_LEN);
                ToUTF8(wbuf, ctxSize, 32);
                GetWindowTextW(hEditNgl, wbuf, PATH_LEN);
                ToUTF8(wbuf, ngl, 32);
                GetWindowTextW(hEditTemp, wbuf, PATH_LEN);
                ToUTF8(wbuf, temp, 32);
                GetWindowTextW(hEditRepeat, wbuf, PATH_LEN);
                ToUTF8(wbuf, repeatPenalty, 32);
                GetWindowTextW(hEditN, wbuf, PATH_LEN);
                ToUTF8(wbuf, n, 32);
                GetWindowTextW(hEditPort, wbuf, PATH_LEN);
                ToUTF8(wbuf, port, 32);
                GetWindowTextW(hEditExtra, wbuf, MAX_BUF);
                ToUTF8(wbuf, extraParams, MAX_BUF);

                BOOL mli = (SendMessageW(hChkMli, BM_GETCHECK, 0, 0) == BST_CHECKED);
                BOOL mtp = (SendMessageW(hChkMtp, BM_GETCHECK, 0, 0) == BST_CHECKED);
                BOOL extra = (SendMessageW(hChkExtra, BM_GETCHECK, 0, 0) == BST_CHECKED);
                BOOL useLlama = (SendMessageW(hChkLlamaPath, BM_GETCHECK, 0, 0) == BST_CHECKED);

                wchar_t wLlamaBin[PATH_LEN];
                GetWindowTextW(hEditLlamaBin, wLlamaBin, PATH_LEN);
                char llamaBin[PATH_LEN];
                ToUTF8(wLlamaBin, llamaBin, PATH_LEN);

                /* 保存配置 */
                if (isCliMode)
                    strcpy(g_runCfg.mode, "命令行");
                else
                    strcpy(g_runCfg.mode, "网页");
                strncpy(g_runCfg.model, model, 127);
                strncpy(g_runCfg.ctxSize, ctxSize, 31);
                strncpy(g_runCfg.ngl, ngl, 31);
                strncpy(g_runCfg.temp, temp, 31);
                strncpy(g_runCfg.repeatPenalty, repeatPenalty, 31);
                strncpy(g_runCfg.n, n, 31);
                strncpy(g_runCfg.port, port, 31);
                g_runCfg.mli = mli;
                g_runCfg.mtp = mtp;
                g_runCfg.extraEnabled = extra;
                strncpy(g_runCfg.extraParams, extraParams, MAX_BUF - 1);
                g_runCfg.useLlamaPath = useLlama;
                strncpy(g_runCfg.llamaBinPath, llamaBin, PATH_LEN - 1);
                SaveRunConfig();

                /* 构建可执行文件路径前缀 */
                char cliExe[PATH_LEN + 32], serverExe[PATH_LEN + 32];
                if (useLlama && llamaBin[0]) {
                    /* 确保路径以 \ 结尾 */
                    char binDir[PATH_LEN];
                    strncpy(binDir, llamaBin, PATH_LEN - 1);
                    binDir[PATH_LEN - 1] = '\0';
                    int len = strlen(binDir);
                    if (len > 0 && binDir[len - 1] != '\\' && binDir[len - 1] != '/') {
                        binDir[len] = '\\';
                        binDir[len + 1] = '\0';
                    }
                    snprintf(cliExe, sizeof(cliExe), "\"%sllama-cli\"", binDir);
                    snprintf(serverExe, sizeof(serverExe), "\"%sllama-server\"", binDir);
                } else {
                    strcpy(cliExe, "llama-cli");
                    strcpy(serverExe, "llama-server");
                }

                /* ---- 生成命令 ---- */
                if (isCliMode) {
                    const char* mliStr = mli ? "-mli" : "";
                    const char* mtpStr = mtp ? "--spec-type draft-mtp --spec-draft-n-max 4" : "";
                    const char* extraStr = (extra && extraParams[0]) ? extraParams : "";
                    snprintf(g_generatedCmd, sizeof(g_generatedCmd),
                        "%s -m \"%s\" -cnv --color auto "
                        "-ngl %s --ctx-size %s --flash-attn on "
                        "--temp %s --repeat-penalty %s -n %s %s %s %s",
                        cliExe, modelPath, ngl, ctxSize, temp, repeatPenalty, n,
                        mliStr, mtpStr, extraStr);
                } else {
                    snprintf(g_generatedCmd, sizeof(g_generatedCmd),
                        "%s -m \"%s\" --ctx-size %s "
                        "-ngl %s --flash-attn on --webui --port %s",
                        serverExe, modelPath, ctxSize, ngl, port);
                }

                /* ---- 显示预览 ---- */
                s_previewShown = TRUE;
                ShowWindow(hHelpText, SW_HIDE);

                wchar_t wcmd[MAX_CMD];
                ToWide(g_generatedCmd, wcmd, MAX_CMD);
                SetWindowTextW(hEditCmdPreview, wcmd);
                ShowWindow(hEditCmdPreview, SW_SHOW);

                ShowWindow(hBtnExecute, SW_SHOW);
                SetWindowTextW(hBtnPreview, L"↺  重新生成");
            }
            break;

        /* ---- 确认运行 ---- */
        case ID_BTN_EXECUTE:
            if (code == BN_CLICKED) {
                /* 从预览编辑框读取（用户可能修改过） */
                wchar_t wcmd[MAX_CMD];
                GetWindowTextW(hEditCmdPreview, wcmd, MAX_CMD);
                ToUTF8(wcmd, g_generatedCmd, MAX_CMD);

                if (strlen(g_generatedCmd) == 0) {
                    MessageBoxW(hwnd, L"命令为空, 请先生成预览!",
                        L"提示", MB_ICONWARNING);
                    break;
                }

                /* 执行命令 */
                ExecuteCommand(g_generatedCmd);

                /* 网页模式: 延迟打开浏览器 */
                int modeSel = SendMessageW(hComboMode, CB_GETCURSEL, 0, 0);
                if (modeSel == 1) {  /* 网页模式 */
                    wchar_t wport[PATH_LEN];
                    GetWindowTextW(hEditPort, wport, PATH_LEN);
                    char port[32];
                    ToUTF8(wport, port, 32);
                    OpenBrowserAfterDelay(port);
                }

                /* 执行后退出整个程序 */
                g_appExiting = TRUE;
                DestroyWindow(hwnd);
            }
            break;

        /* ---- 返回按钮 ---- */
        case ID_BTN_BACK:
            DestroyWindow(hwnd);  /* 返回主窗口 */
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
