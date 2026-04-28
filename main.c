// 编译命令: gcc -o llama_gui.exe main.c -mwindows -s -O2
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <shellapi.h>
#include <time.h>

// 配置文件路径
#define CONFIG_FILE "llama_config.txt"
#define CONVERT_CONFIG_FILE "llama_convert_config.txt"

// 控件 ID 定义
#define ID_COMBO_COMMAND 101
#define ID_COMBO_MODEL 102
#define ID_EDIT_CTX_SIZE 103
#define ID_EDIT_NGL 104
#define ID_EDIT_TEMP 105
#define ID_EDIT_REPEAT_PENALTY 106
#define ID_EDIT_N 107
#define ID_EDIT_PORT 108
#define ID_CHECKBOX_MLI 109
#define ID_BUTTON_RUN 110
#define ID_CHECKBOX_OTHER_PARAMS 111
#define ID_EDIT_OTHER_PARAMS 112
#define ID_BUTTON_HELP 113
#define ID_BUTTON_MAIN_RUN 201
#define ID_BUTTON_MAIN_CONVERT 202
#define ID_BUTTON_CONVERT_MODEL 301
#define ID_BUTTON_CONVERT_LORA 302
#define ID_EDIT_OUTTYPE 401

// 模型结构
typedef struct {
    char name[128];
    char path[512];
} ModelInfo;

// 模型列表
ModelInfo modelList[100];
int modelCount = 0;

// 主要窗口 - 主启动窗口
HWND hMainWindow;
// 运行窗口句柄
HWND hRunWindow;
// 转换窗口句柄
HWND hConvertWindow;

// 运行窗口控件
HWND hComboCommand, hComboModel, hEditCtxSize, hEditNgl, hEditTemp, hEditRepeatPenalty, hEditN, hEditPort, hCheckboxMli, hButtonRun, hCheckboxOtherParams, hEditOtherParams, hLabelHelp, hButtonHelp;

// 转换窗口控件
HWND hEditOuttype;

// 标签句柄
HWND hLabelCommand, hLabelModel, hLabelCtxSize, hLabelNgl, hLabelTemp, hLabelRepeatPenalty, hLabelN, hLabelPort, hLabelMli, hLabelOtherParams, hLabelParamHelp, hLabelHelp;

// 转换配置
char g_llamaPath[512] = "";
char g_modelSrcPath[512] = "";
char g_loraSrcPath[512] = "";
char g_modelDstPath[512] = "";
char g_outtype[64] = "f16";

// 参数注释文本
const wchar_t* PARAM_HELP_TEXT = 
    L"可用参数说明:\n"
    L"--lora                    启用 LORA 模型。(后加完整lora路径)\n"
    L"-b, --batch-size          批处理大小，影响 Prompt 处理速度。\n"
    L"--mlock                   锁定内存，防止模型被系统交换到虚拟内存，避免卡顿。\n"
    L"--rpc                     分布式推理，通过 RPC 连接其他机器的后端。\n"
    L"--top-k                   Top-K 采样，只从概率最高的 K 个候选词中挑选。\n"
    L"--top-p                   Top-P (核采样)，只从累积概率超过 P 的候选词中挑。\n"
    L"--min-p                   Min-P 采样，概率低于最高概率*min_p 的词被直接过滤。\n"
    L"--ignore-eos              忽略结束符，强制模型继续生成。\n"
    L"--dry-multiplier        DRY 采样倍率，用于更智能地惩罚重复内容。\n"
    L"-p, --prompt            设置初始 Prompt。\n"
    L"-f, --file              从文件读取 Prompt。\n"
    L"-sys, --system-prompt    设置系统提示词。\n"
    L"-sysf, --system-prompt-file 从文件加载系统提示词。\n"
    L"-mli, --multiline-input  多行输入模式，按两次回车发送。\n"
    L"-r, --reverse-prompt    反向提示，模型遇到特定文字就暂停（用于交互）。\n"
    L"--chat-template          手动指定对话模板（如 llama3, chatml, deepseek 等）。\n"
    L"--reasoning-format      控制 DeepSeek-R1 等模型的思考过程如何展示或隐藏。";

// 保存模型列表到文件
void SaveModelList() {
    FILE* fp = fopen("llama_models.txt", "w");
    if (fp) {
        fprintf(fp, "%d\n", modelCount);
        for (int i = 0; i < modelCount; i++) {
            fprintf(fp, "%s\n", modelList[i].name);
            fprintf(fp, "%s\n", modelList[i].path);
        }
        fclose(fp);
    }
}

// 加载模型列表从文件
void LoadModelList() {
    FILE* fp = fopen("llama_models.txt", "r");
    if (fp) {
        fscanf(fp, "%d\n", &modelCount);
        for (int i = 0; i < modelCount && i < 100; i++) {
            char buffer[512];
            fgets(buffer, sizeof(buffer), fp);
            buffer[strcspn(buffer, "\n")] = '\0';
            strcpy(modelList[i].name, buffer);
            
            fgets(buffer, sizeof(buffer), fp);
            buffer[strcspn(buffer, "\n")] = '\0';
            strcpy(modelList[i].path, buffer);
        }
        fclose(fp);
    } else {
        modelCount = 0;
        SaveModelList();
    }
}

// 保存配置到文件
void SaveConfig(const char* commandType, const char* model, const char* ctxSize, const char* ngl, 
               const char* temp, const char* repeatPenalty, const char* n, const char* port, BOOL mliEnabled, BOOL otherParamsEnabled, const char* otherParams) {
    FILE* fp = fopen(CONFIG_FILE, "w");
    if (fp) {
        fprintf(fp, "%s\n", commandType);
        fprintf(fp, "%s\n", model);
        fprintf(fp, "%s\n", ctxSize);
        fprintf(fp, "%s\n", ngl);
        fprintf(fp, "%s\n", temp);
        fprintf(fp, "%s\n", repeatPenalty);
        fprintf(fp, "%s\n", n);
        fprintf(fp, "%s\n", port);
        fprintf(fp, "%d\n", mliEnabled);
        fprintf(fp, "%d\n", otherParamsEnabled);
        fprintf(fp, "%s\n", otherParams);
        fclose(fp);
    }
}

// 从文件加载配置
void LoadConfig(char* commandType, char* model, char* ctxSize, char* ngl, 
               char* temp, char* repeatPenalty, char* n, char* port, BOOL* mliEnabled, BOOL* otherParamsEnabled, char* otherParams) {
    FILE* fp = fopen(CONFIG_FILE, "r");
    if (fp) {
        char buffer[512];
        fgets(buffer, sizeof(buffer), fp);
        buffer[strcspn(buffer, "\n")] = '\0';
        strcpy(commandType, buffer);
        
        fgets(buffer, sizeof(buffer), fp);
        buffer[strcspn(buffer, "\n")] = '\0';
        strcpy(model, buffer);
        
        fgets(buffer, sizeof(buffer), fp);
        buffer[strcspn(buffer, "\n")] = '\0';
        strcpy(ctxSize, buffer);
        
        fgets(buffer, sizeof(buffer), fp);
        buffer[strcspn(buffer, "\n")] = '\0';
        strcpy(ngl, buffer);
        
        fgets(buffer, sizeof(buffer), fp);
        buffer[strcspn(buffer, "\n")] = '\0';
        strcpy(temp, buffer);
        
        fgets(buffer, sizeof(buffer), fp);
        buffer[strcspn(buffer, "\n")] = '\0';
        strcpy(repeatPenalty, buffer);
        
        fgets(buffer, sizeof(buffer), fp);
        buffer[strcspn(buffer, "\n")] = '\0';
        strcpy(n, buffer);
        
        fgets(buffer, sizeof(buffer), fp);
        buffer[strcspn(buffer, "\n")] = '\0';
        strcpy(port, buffer);
        
        int mliValue;
        fscanf(fp, "%d", &mliValue);
        *mliEnabled = (mliValue != 0);
        
        int ch;
        while ((ch = fgetc(fp)) != '\n' && ch != EOF);
        
        int otherParamsValue;
        if (fscanf(fp, "%d", &otherParamsValue) == 1) {
            *otherParamsEnabled = (otherParamsValue != 0);
            while ((ch = fgetc(fp)) != '\n' && ch != EOF);
            if (fgets(buffer, sizeof(buffer), fp)) {
                buffer[strcspn(buffer, "\n")] = '\0';
                strcpy(otherParams, buffer);
            }
        } else {
            *otherParamsEnabled = FALSE;
            strcpy(otherParams, "");
        }
        
        fclose(fp);
    }
}

// 保存转换配置到文件
void SaveConvertConfig() {
    FILE* fp = fopen(CONVERT_CONFIG_FILE, "w");
    if (fp) {
        fprintf(fp, "# llama.cpp路径\n");
        fprintf(fp, "%s\n", g_llamaPath);
        fprintf(fp, "# 模型文件路径\n");
        fprintf(fp, "%s\n", g_modelSrcPath);
        fprintf(fp, "# lora文件路径\n");
        fprintf(fp, "%s\n", g_loraSrcPath);
        fprintf(fp, "# 转换后模型存储路径\n");
        fprintf(fp, "%s\n", g_modelDstPath);
        fprintf(fp, "# outtype参数\n");
        fprintf(fp, "%s\n", g_outtype);
        fclose(fp);
    }
}

// 从文件加载转换配置，返回true表示文件存在，false表示文件不存在
BOOL LoadConvertConfig() {
    FILE* fp = fopen(CONVERT_CONFIG_FILE, "r");
    if (fp) {
        char buffer[512];
        int lineCount = 0;
        while (fgets(buffer, sizeof(buffer), fp)) {
            buffer[strcspn(buffer, "\n")] = '\0';
            if (buffer[0] == '#') {
                continue;
            }
            if (strlen(buffer) > 0) {
                if (lineCount == 0) {
                    strcpy(g_llamaPath, buffer);
                } else if (lineCount == 1) {
                    strcpy(g_modelSrcPath, buffer);
                } else if (lineCount == 2) {
                    strcpy(g_loraSrcPath, buffer);
                } else if (lineCount == 3) {
                    strcpy(g_modelDstPath, buffer);
                } else if (lineCount == 4) {
                    strcpy(g_outtype, buffer);
                }
                lineCount++;
            }
        }
        fclose(fp);
        return TRUE;
    } else {
        SaveConvertConfig();
        // 将宏定义的文件名转换为宽字符
        wchar_t wFileName[128];
        MultiByteToWideChar(CP_UTF8, 0, CONVERT_CONFIG_FILE, -1, wFileName, sizeof(wFileName));
        
        wchar_t wMsg[512];
        swprintf(wMsg, sizeof(wMsg), L"已创建%s文件，请先配置路径！", wFileName);
        MessageBoxW(NULL, wMsg, L"提示", MB_OK);
        return FALSE;
    }
}

HWND AddControl(HWND hParent, const wchar_t* text, int x, int y, int w, int h) {
    return CreateWindowExW(0, L"STATIC", text, WS_CHILD | WS_VISIBLE, x, y, w, h, hParent, NULL, NULL, NULL);
}

HWND CreateEditBox(HWND hParent, const wchar_t* defaultText, int x, int y, int w, int h, int id) {
    HWND hEdit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", defaultText,
        WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
        x, y, w, h, hParent, (HMENU)id, NULL, NULL);
    SendMessageW(hEdit, WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), TRUE);
    return hEdit;
}

void ExecuteCommand(const char* cmd, HWND hwnd, const char* commandType, const char* port) {
    char fullCmd[4096];
    snprintf(fullCmd, sizeof(fullCmd), "cmd /k \"%s\"", cmd);
    
    STARTUPINFOA si = {0};
    PROCESS_INFORMATION pi = {0};
    si.cb = sizeof(si);
    
    if (!CreateProcessA(NULL, fullCmd, NULL, NULL, FALSE, CREATE_NEW_CONSOLE, NULL, NULL, &si, &pi)) {
        MessageBoxW(NULL, L"无法启动命令！", L"错误", MB_ICONERROR);
        return;
    }
    
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    
    if (strcmp(commandType, "llama-server") == 0) {
        Sleep(3000);
        char url[256];
        snprintf(url, sizeof(url), "http://127.0.0.1:%s", port);
        ShellExecuteA(NULL, "open", url, NULL, NULL, SW_SHOWNORMAL);
    }
    
    if (hwnd != NULL) {
        PostMessageW(hwnd, WM_CLOSE, 0, 0);
    }
}

// 转换窗口的消息处理
LRESULT CALLBACK ConvertWindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_CREATE: {
            HFONT hFont = CreateFontW(14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                GB2312_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, L"微软雅黑");
            
            AddControl(hwnd, L"--outtype:", 100, 40, 100, 25);
            wchar_t wOuttype[64];
            MultiByteToWideChar(CP_UTF8, 0, g_outtype, -1, wOuttype, sizeof(wOuttype));
            hEditOuttype = CreateEditBox(hwnd, wOuttype, 220, 40, 150, 25, ID_EDIT_OUTTYPE);
            
            HWND hButtonModel = CreateWindowExW(0, L"BUTTON", L"转换模型",
                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                50, 100, 200, 40, hwnd, (HMENU)ID_BUTTON_CONVERT_MODEL, NULL, NULL);
            SendMessageW(hButtonModel, WM_SETFONT, (WPARAM)hFont, TRUE);
            
            HWND hButtonLora = CreateWindowExW(0, L"BUTTON", L"转换lora",
                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                280, 100, 200, 40, hwnd, (HMENU)ID_BUTTON_CONVERT_LORA, NULL, NULL);
            SendMessageW(hButtonLora, WM_SETFONT, (WPARAM)hFont, TRUE);
            break;
        }
        
        case WM_COMMAND: {
            if (LOWORD(wParam) == ID_BUTTON_CONVERT_MODEL && HIWORD(wParam) == BN_CLICKED) {
                wchar_t wOuttype[64];
                GetWindowTextW(hEditOuttype, wOuttype, sizeof(wOuttype)/sizeof(wchar_t));
                
                char outtype[64];
                WideCharToMultiByte(CP_UTF8, 0, wOuttype, -1, outtype, sizeof(outtype), NULL, NULL);
                
                strcpy(g_outtype, outtype);
                SaveConvertConfig();
                
                char cmd[4096];
                snprintf(cmd, sizeof(cmd), "cd /d \"%s\" && python convert_hf_to_gguf.py \"%s\" --outtype %s --outfile \"%s\"", g_llamaPath, g_modelSrcPath, outtype, g_modelDstPath);
                char fullCmd[4096];
                snprintf(fullCmd, sizeof(fullCmd), "cmd /k \"%s\"", cmd);
                STARTUPINFOA si = {0};
                PROCESS_INFORMATION pi = {0};
                si.cb = sizeof(si);
                CreateProcessA(NULL, fullCmd, NULL, NULL, FALSE, CREATE_NEW_CONSOLE, NULL, NULL, &si, &pi);
                PostMessageW(hwnd, WM_CLOSE, 0, 0);
            } else if (LOWORD(wParam) == ID_BUTTON_CONVERT_LORA && HIWORD(wParam) == BN_CLICKED) {
                wchar_t wOuttype[64];
                GetWindowTextW(hEditOuttype, wOuttype, sizeof(wOuttype)/sizeof(wchar_t));
                
                char outtype[64];
                WideCharToMultiByte(CP_UTF8, 0, wOuttype, -1, outtype, sizeof(outtype), NULL, NULL);
                
                strcpy(g_outtype, outtype);
                SaveConvertConfig();
                
                char cmd[4096];
                snprintf(cmd, sizeof(cmd), "cd /d \"%s\" && python convert_lora_to_gguf.py \"%s\" --outfile \"%s\" --outtype %s --base \"%s\"", g_llamaPath, g_loraSrcPath, g_modelDstPath, outtype, g_modelSrcPath);
                char fullCmd[4096];
                snprintf(fullCmd, sizeof(fullCmd), "cmd /k \"%s\"", cmd);
                STARTUPINFOA si = {0};
                PROCESS_INFORMATION pi = {0};
                si.cb = sizeof(si);
                CreateProcessA(NULL, fullCmd, NULL, NULL, FALSE, CREATE_NEW_CONSOLE, NULL, NULL, &si, &pi);
                PostMessageW(hwnd, WM_CLOSE, 0, 0);
            }
            break;
        }
        
        case WM_CLOSE: {
            DestroyWindow(hwnd);
            hConvertWindow = NULL;
            PostQuitMessage(0);
            break;
        }
        
        case WM_DESTROY: {
            PostQuitMessage(0);
            break;
        }
        
        default: {
            return DefWindowProcW(hwnd, uMsg, wParam, lParam);
        }
    }
    return 0;
}

// 运行窗口的消息处理
LRESULT CALLBACK RunWindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_CREATE: {
            HFONT hFont = CreateFontW(14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                GB2312_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, L"微软雅黑");
            
            hLabelCommand = AddControl(hwnd, L"选择模式:", 20, 20, 100, 25);
            hComboCommand = CreateWindowExW(0, L"COMBOBOX", L"",
                WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
                130, 20, 200, 200, hwnd, (HMENU)ID_COMBO_COMMAND, NULL, NULL);
            SendMessageW(hComboCommand, CB_ADDSTRING, 0, (LPARAM)L"命令行");
            SendMessageW(hComboCommand, CB_ADDSTRING, 0, (LPARAM)L"网页");
            SendMessageW(hComboModel, WM_SETFONT, (WPARAM)hFont, TRUE);
            
            hLabelModel = AddControl(hwnd, L"模型:", 20, 60, 100, 25);
            hComboModel = CreateWindowExW(0, L"COMBOBOX", L"",
                WS_CHILD | WS_VISIBLE | CBS_DROPDOWN | WS_VSCROLL,
                130, 60, 200, 200, hwnd, (HMENU)ID_COMBO_MODEL, NULL, NULL);
            for (int i = 0; i < modelCount; i++) {
                wchar_t wName[128];
                MultiByteToWideChar(CP_UTF8, 0, modelList[i].name, -1, wName, sizeof(wName));
                SendMessageW(hComboModel, CB_ADDSTRING, 0, (LPARAM)wName);
            }
            SendMessageW(hComboModel, WM_SETFONT, (WPARAM)hFont, TRUE);
            
            hLabelCtxSize = AddControl(hwnd, L"ctx-size:", 20, 100, 100, 25);
            hEditCtxSize = CreateEditBox(hwnd, L"8192", 130, 100, 200, 25, ID_EDIT_CTX_SIZE);
            
            hLabelNgl = AddControl(hwnd, L"ngl:", 20, 140, 100, 25);
            hEditNgl = CreateEditBox(hwnd, L"48", 130, 140, 200, 25, ID_EDIT_NGL);
            
            hLabelTemp = AddControl(hwnd, L"温度:", 20, 180, 100, 25);
            hEditTemp = CreateEditBox(hwnd, L"0.7", 130, 180, 200, 25, ID_EDIT_TEMP);
            
            hLabelRepeatPenalty = AddControl(hwnd, L"重复惩罚:", 20, 220, 100, 25);
            hEditRepeatPenalty = CreateEditBox(hwnd, L"1.1", 130, 220, 200, 25, ID_EDIT_REPEAT_PENALTY);
            
            hLabelN = AddControl(hwnd, L"最大生成 (-n):", 20, 260, 100, 25);
            hEditN = CreateEditBox(hwnd, L"2048", 130, 260, 200, 25, ID_EDIT_N);
            
            hLabelMli = AddControl(hwnd, L"多行输入模式:", 20, 300, 100, 25);
            hCheckboxMli = CreateWindowExW(0, L"BUTTON", L"启用 (-mli)",
                WS_CHILD | WS_VISIBLE | BS_CHECKBOX,
                130, 300, 200, 25, hwnd, (HMENU)ID_CHECKBOX_MLI, NULL, NULL);
            SendMessageW(hCheckboxMli, BM_SETCHECK, BST_CHECKED, 0);
            SendMessageW(hCheckboxMli, WM_SETFONT, (WPARAM)hFont, TRUE);
            
            hLabelOtherParams = AddControl(hwnd, L"其他参数:", 20, 340, 100, 25);
            hCheckboxOtherParams = CreateWindowExW(0, L"BUTTON", L"启用额外参数",
                WS_CHILD | WS_VISIBLE | BS_CHECKBOX,
                130, 340, 200, 25, hwnd, (HMENU)ID_CHECKBOX_OTHER_PARAMS, NULL, NULL);
            SendMessageW(hCheckboxOtherParams, BM_SETCHECK, BST_UNCHECKED, 0);
            SendMessageW(hCheckboxOtherParams, WM_SETFONT, (WPARAM)hFont, TRUE);
            
            hEditOtherParams = CreateEditBox(hwnd, L"", 130, 370, 400, 25, ID_EDIT_OTHER_PARAMS);
            ShowWindow(hEditOtherParams, SW_HIDE);
            
            hLabelParamHelp = CreateWindowExW(0, L"STATIC", PARAM_HELP_TEXT,
                WS_CHILD | WS_VISIBLE | SS_LEFT,
                20, 405, 540, 190, hwnd, NULL, NULL, NULL);
            SendMessageW(hLabelParamHelp, WM_SETFONT, (WPARAM)hFont, TRUE);
            
            hLabelPort = AddControl(hwnd, L"端口:", 20, 180, 100, 25);
            hEditPort = CreateEditBox(hwnd, L"8081", 130, 180, 200, 25, ID_EDIT_PORT);
            
            hButtonRun = CreateWindowExW(0, L"BUTTON", L"运行命令",
                WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
                20, 605, 150, 40, hwnd, (HMENU)ID_BUTTON_RUN, NULL, NULL);
            SendMessageW(hButtonRun, WM_SETFONT, (WPARAM)hFont, TRUE);
            
            hLabelHelp = AddControl(hwnd, L"运行llama-cli --help查看全部参数", 190, 615, 250, 20);
            
            hButtonHelp = CreateWindowExW(0, L"BUTTON", L"?",
                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                440, 608, 40, 35, hwnd, (HMENU)ID_BUTTON_HELP, NULL, NULL);
            SendMessageW(hButtonHelp, WM_SETFONT, (WPARAM)hFont, TRUE);
            
            char configCommandType[64] = "命令行";
            char configModel[512] = "";
            char configCtxSize[64] = "8192";
            char configNgl[64] = "48";
            char configTemp[64] = "0.7";
            char configRepeatPenalty[64] = "1.1";
            char configN[64] = "2048";
            char configPort[64] = "8081";
            BOOL configMliEnabled = TRUE;
            BOOL configOtherParamsEnabled = FALSE;
            char configOtherParams[512] = "";
            
            LoadConfig(configCommandType, configModel, configCtxSize, configNgl, 
                      configTemp, configRepeatPenalty, configN, configPort, &configMliEnabled, &configOtherParamsEnabled, configOtherParams);
            
            if (strcmp(configCommandType, "命令行") == 0) {
                SendMessageW(hComboCommand, CB_SETCURSEL, 0, 0);
            } else {
                SendMessageW(hComboCommand, CB_SETCURSEL, 1, 0);
            }
            
            for (int i = 0; i < modelCount; i++) {
                if (strcmp(configModel, modelList[i].name) == 0) {
                    SendMessageW(hComboModel, CB_SETCURSEL, i, 0);
                    break;
                }
            }
            
            wchar_t wCtxSize[64], wNgl[64], wTemp[64], wRepeatPenalty[64], wN[64], wPort[64], wOtherParams[512];
            MultiByteToWideChar(CP_UTF8, 0, configCtxSize, -1, wCtxSize, sizeof(wCtxSize));
            MultiByteToWideChar(CP_UTF8, 0, configNgl, -1, wNgl, sizeof(wNgl));
            MultiByteToWideChar(CP_UTF8, 0, configTemp, -1, wTemp, sizeof(wTemp));
            MultiByteToWideChar(CP_UTF8, 0, configRepeatPenalty, -1, wRepeatPenalty, sizeof(wRepeatPenalty));
            MultiByteToWideChar(CP_UTF8, 0, configN, -1, wN, sizeof(wN));
            MultiByteToWideChar(CP_UTF8, 0, configPort, -1, wPort, sizeof(wPort));
            MultiByteToWideChar(CP_UTF8, 0, configOtherParams, -1, wOtherParams, sizeof(wOtherParams));
            
            SetWindowTextW(hEditCtxSize, wCtxSize);
            SetWindowTextW(hEditNgl, wNgl);
            SetWindowTextW(hEditTemp, wTemp);
            SetWindowTextW(hEditRepeatPenalty, wRepeatPenalty);
            SetWindowTextW(hEditN, wN);
            SetWindowTextW(hEditPort, wPort);
            SetWindowTextW(hEditOtherParams, wOtherParams);
            SendMessageW(hCheckboxMli, BM_SETCHECK, configMliEnabled ? BST_CHECKED : BST_UNCHECKED, 0);
            SendMessageW(hCheckboxOtherParams, BM_SETCHECK, configOtherParamsEnabled ? BST_CHECKED : BST_UNCHECKED, 0);
            
            if (configOtherParamsEnabled) {
                ShowWindow(hEditOtherParams, SW_SHOW);
            }
            
            if (strcmp(configCommandType, "命令行") == 0) {
                ShowWindow(hLabelTemp, SW_SHOW);
                ShowWindow(hEditTemp, SW_SHOW);
                ShowWindow(hLabelRepeatPenalty, SW_SHOW);
                ShowWindow(hEditRepeatPenalty, SW_SHOW);
                ShowWindow(hLabelN, SW_SHOW);
                ShowWindow(hEditN, SW_SHOW);
                ShowWindow(hLabelMli, SW_SHOW);
                ShowWindow(hCheckboxMli, SW_SHOW);
                ShowWindow(hLabelOtherParams, SW_SHOW);
                ShowWindow(hCheckboxOtherParams, SW_SHOW);
                ShowWindow(hLabelParamHelp, SW_SHOW);
                ShowWindow(hLabelHelp, SW_SHOW);
                ShowWindow(hButtonHelp, SW_SHOW);
                ShowWindow(hLabelPort, SW_HIDE);
                ShowWindow(hEditPort, SW_HIDE);
            } else {
                ShowWindow(hLabelTemp, SW_HIDE);
                ShowWindow(hEditTemp, SW_HIDE);
                ShowWindow(hLabelRepeatPenalty, SW_HIDE);
                ShowWindow(hEditRepeatPenalty, SW_HIDE);
                ShowWindow(hLabelN, SW_HIDE);
                ShowWindow(hEditN, SW_HIDE);
                ShowWindow(hLabelMli, SW_HIDE);
                ShowWindow(hCheckboxMli, SW_HIDE);
                ShowWindow(hLabelOtherParams, SW_HIDE);
                ShowWindow(hCheckboxOtherParams, SW_HIDE);
                ShowWindow(hEditOtherParams, SW_HIDE);
                ShowWindow(hLabelParamHelp, SW_HIDE);
                ShowWindow(hLabelHelp, SW_HIDE);
                ShowWindow(hButtonHelp, SW_HIDE);
                ShowWindow(hLabelPort, SW_SHOW);
                ShowWindow(hEditPort, SW_SHOW);
            }
            break;
        }
        
        case WM_COMMAND: {
            if (LOWORD(wParam) == ID_COMBO_COMMAND && HIWORD(wParam) == CBN_SELCHANGE) {
                int curSel = SendMessageW(hComboCommand, CB_GETCURSEL, 0, 0);
                wchar_t wcommandType[64];
                SendMessageW(hComboCommand, CB_GETLBTEXT, curSel, (LPARAM)wcommandType);
                char commandType[64];
                WideCharToMultiByte(CP_UTF8, 0, wcommandType, -1, commandType, sizeof(commandType), NULL, NULL);
                
                if (strcmp(commandType, "命令行") == 0) {
                    ShowWindow(hLabelTemp, SW_SHOW);
                    ShowWindow(hEditTemp, SW_SHOW);
                    ShowWindow(hLabelRepeatPenalty, SW_SHOW);
                    ShowWindow(hEditRepeatPenalty, SW_SHOW);
                    ShowWindow(hLabelN, SW_SHOW);
                    ShowWindow(hEditN, SW_SHOW);
                    ShowWindow(hLabelMli, SW_SHOW);
                    ShowWindow(hCheckboxMli, SW_SHOW);
                    ShowWindow(hLabelOtherParams, SW_SHOW);
                    ShowWindow(hCheckboxOtherParams, SW_SHOW);
                    ShowWindow(hLabelParamHelp, SW_SHOW);
                    ShowWindow(hLabelHelp, SW_SHOW);
                    ShowWindow(hButtonHelp, SW_SHOW);
                    ShowWindow(hLabelPort, SW_HIDE);
                    ShowWindow(hEditPort, SW_HIDE);
                    
                    BOOL otherParamsEnabled = SendMessageW(hCheckboxOtherParams, BM_GETCHECK, 0, 0) == BST_CHECKED;
                    if (otherParamsEnabled) {
                        ShowWindow(hEditOtherParams, SW_SHOW);
                    }
                } else {
                    ShowWindow(hLabelTemp, SW_HIDE);
                    ShowWindow(hEditTemp, SW_HIDE);
                    ShowWindow(hLabelRepeatPenalty, SW_HIDE);
                    ShowWindow(hEditRepeatPenalty, SW_HIDE);
                    ShowWindow(hLabelN, SW_HIDE);
                    ShowWindow(hEditN, SW_HIDE);
                    ShowWindow(hLabelMli, SW_HIDE);
                    ShowWindow(hCheckboxMli, SW_HIDE);
                    ShowWindow(hLabelOtherParams, SW_HIDE);
                    ShowWindow(hCheckboxOtherParams, SW_HIDE);
                    ShowWindow(hEditOtherParams, SW_HIDE);
                    ShowWindow(hLabelParamHelp, SW_HIDE);
                    ShowWindow(hLabelHelp, SW_HIDE);
                    ShowWindow(hButtonHelp, SW_HIDE);
                    ShowWindow(hLabelPort, SW_SHOW);
                    ShowWindow(hEditPort, SW_SHOW);
                }
            } else if (LOWORD(wParam) == ID_CHECKBOX_MLI && HIWORD(wParam) == BN_CLICKED) {
                BOOL currentState = SendMessageW(hCheckboxMli, BM_GETCHECK, 0, 0);
                if (currentState == BST_CHECKED) {
                    SendMessageW(hCheckboxMli, BM_SETCHECK, BST_UNCHECKED, 0);
                } else {
                    SendMessageW(hCheckboxMli, BM_SETCHECK, BST_CHECKED, 0);
                }
            } else if (LOWORD(wParam) == ID_CHECKBOX_OTHER_PARAMS && HIWORD(wParam) == BN_CLICKED) {
                BOOL currentState = SendMessageW(hCheckboxOtherParams, BM_GETCHECK, 0, 0);
                if (currentState == BST_CHECKED) {
                    SendMessageW(hCheckboxOtherParams, BM_SETCHECK, BST_UNCHECKED, 0);
                    ShowWindow(hEditOtherParams, SW_HIDE);
                } else {
                    SendMessageW(hCheckboxOtherParams, BM_SETCHECK, BST_CHECKED, 0);
                    ShowWindow(hEditOtherParams, SW_SHOW);
                }
            } else if (LOWORD(wParam) == ID_BUTTON_HELP && HIWORD(wParam) == BN_CLICKED) {
                char cmd[4096];
                snprintf(cmd, sizeof(cmd), "llama-cli --help");
                char fullCmd[4096];
                snprintf(fullCmd, sizeof(fullCmd), "cmd /k \"%s\"", cmd);
                STARTUPINFOA si = {0};
                PROCESS_INFORMATION pi = {0};
                si.cb = sizeof(si);
                CreateProcessA(NULL, fullCmd, NULL, NULL, FALSE, CREATE_NEW_CONSOLE, NULL, NULL, &si, &pi);
                PostQuitMessage(0);
            } else if (LOWORD(wParam) == ID_BUTTON_RUN) {
                int modelSel = SendMessageW(hComboModel, CB_GETCURSEL, 0, 0);
                if (modelSel == CB_ERR || modelCount == 0) {
                    MessageBoxW(NULL, L"请先添加模型！", L"错误", MB_ICONERROR);
                    PostQuitMessage(0);
                    break;
                }
                
                char cmd[4096];
                wchar_t wcommandType[64], wmodel[512], wctxSize[64], wngl[64], wtemp[64], wrepeatPenalty[64], wn[64], wport[64], wOtherParams[512];
                char commandType[64], model[512], ctxSize[64], ngl[64], temp[64], repeatPenalty[64], n[64], port[64], otherParams[512];
                
                int curSel = SendMessageW(hComboCommand, CB_GETCURSEL, 0, 0);
                SendMessageW(hComboCommand, CB_GETLBTEXT, curSel, (LPARAM)wcommandType);
                SendMessageW(hComboModel, CB_GETLBTEXT, modelSel, (LPARAM)wmodel);
                GetWindowTextW(hEditCtxSize, wctxSize, sizeof(wctxSize)/sizeof(wchar_t));
                GetWindowTextW(hEditNgl, wngl, sizeof(wngl)/sizeof(wchar_t));
                GetWindowTextW(hEditTemp, wtemp, sizeof(wtemp)/sizeof(wchar_t));
                GetWindowTextW(hEditRepeatPenalty, wrepeatPenalty, sizeof(wrepeatPenalty)/sizeof(wchar_t));
                GetWindowTextW(hEditN, wn, sizeof(wn)/sizeof(wchar_t));
                GetWindowTextW(hEditPort, wport, sizeof(wport)/sizeof(wchar_t));
                GetWindowTextW(hEditOtherParams, wOtherParams, sizeof(wOtherParams)/sizeof(wchar_t));
                
                WideCharToMultiByte(CP_UTF8, 0, wcommandType, -1, commandType, sizeof(commandType), NULL, NULL);
                WideCharToMultiByte(CP_UTF8, 0, wmodel, -1, model, sizeof(model), NULL, NULL);
                WideCharToMultiByte(CP_UTF8, 0, wctxSize, -1, ctxSize, sizeof(ctxSize), NULL, NULL);
                WideCharToMultiByte(CP_UTF8, 0, wngl, -1, ngl, sizeof(ngl), NULL, NULL);
                WideCharToMultiByte(CP_UTF8, 0, wtemp, -1, temp, sizeof(temp), NULL, NULL);
                WideCharToMultiByte(CP_UTF8, 0, wrepeatPenalty, -1, repeatPenalty, sizeof(repeatPenalty), NULL, NULL);
                WideCharToMultiByte(CP_UTF8, 0, wn, -1, n, sizeof(n), NULL, NULL);
                WideCharToMultiByte(CP_UTF8, 0, wport, -1, port, sizeof(port), NULL, NULL);
                WideCharToMultiByte(CP_UTF8, 0, wOtherParams, -1, otherParams, sizeof(otherParams), NULL, NULL);
                
                char modelPath[512];
                int modelFound = 0;
                for (int i = 0; i < modelCount; i++) {
                    if (strcmp(model, modelList[i].name) == 0) {
                        strcpy(modelPath, modelList[i].path);
                        modelFound = 1;
                        break;
                    }
                }
                if (!modelFound) {
                    strcpy(modelPath, model);
                }
                
                if (strcmp(commandType, "命令行") == 0) {
                    BOOL mliEnabled = SendMessageW(hCheckboxMli, BM_GETCHECK, 0, 0) == BST_CHECKED;
                    BOOL otherParamsEnabled = SendMessageW(hCheckboxOtherParams, BM_GETCHECK, 0, 0) == BST_CHECKED;
                    const char* mliOption = mliEnabled ? "-mli" : "";
                    const char* otherParamsOption = otherParamsEnabled ? otherParams : "";
                    
                    snprintf(cmd, sizeof(cmd),
                        "llama-cli -m \"%s\" -cnv --color auto -ngl %s --ctx-size %s --flash-attn on --temp %s --repeat-penalty %s -n %s %s %s",
                        modelPath, ngl, ctxSize, temp, repeatPenalty, n, mliOption, otherParamsOption);
                } else {
                    snprintf(cmd, sizeof(cmd),
                        "llama-server -m \"%s\" --ctx-size %s -ngl %s --flash-attn on --webui --port %s",
                        modelPath, ctxSize, ngl, port);
                }
                
                BOOL mliEnabled = SendMessageW(hCheckboxMli, BM_GETCHECK, 0, 0) == BST_CHECKED;
                BOOL otherParamsEnabled = SendMessageW(hCheckboxOtherParams, BM_GETCHECK, 0, 0) == BST_CHECKED;
                SaveConfig(commandType, model, ctxSize, ngl, temp, repeatPenalty, n, port, mliEnabled, otherParamsEnabled, otherParams);
                ExecuteCommand(cmd, hwnd, commandType, port);
            }
            break;
        }
        
        case WM_CLOSE: {
            DestroyWindow(hwnd);
            hRunWindow = NULL;
            PostQuitMessage(0);
            break;
        }
        
        case WM_DESTROY: {
            PostQuitMessage(0);
            break;
        }
        
        default: {
            return DefWindowProcW(hwnd, uMsg, wParam, lParam);
        }
    }
    return 0;
}

// 主窗口的消息处理
LRESULT CALLBACK MainWindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_CREATE: {
            HFONT hFont = CreateFontW(16, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                GB2312_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, L"微软雅黑");
            
            AddControl(hwnd, L"Llama 工具", 130, 30, 200, 40);
            
            HWND hButtonRun = CreateWindowExW(0, L"BUTTON", L"运行",
                WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
                80, 100, 120, 50, hwnd, (HMENU)ID_BUTTON_MAIN_RUN, NULL, NULL);
            SendMessageW(hButtonRun, WM_SETFONT, (WPARAM)hFont, TRUE);
            
            HWND hButtonConvert = CreateWindowExW(0, L"BUTTON", L"转化",
                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                260, 100, 120, 50, hwnd, (HMENU)ID_BUTTON_MAIN_CONVERT, NULL, NULL);
            SendMessageW(hButtonConvert, WM_SETFONT, (WPARAM)hFont, TRUE);
            break;
        }
        
        case WM_COMMAND: {
            if (LOWORD(wParam) == ID_BUTTON_MAIN_RUN && HIWORD(wParam) == BN_CLICKED) {
                PostMessageW(hwnd, WM_CLOSE, 0, 0);
                
                const wchar_t* RUN_CLASS_NAME = L"LlamaRunClass";
                
                WNDCLASSEXW wcRun = {0};
                wcRun.cbSize = sizeof(WNDCLASSEXW);
                wcRun.lpfnWndProc = RunWindowProc;
                wcRun.hInstance = NULL;
                wcRun.lpszClassName = RUN_CLASS_NAME;
                wcRun.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
                wcRun.hCursor = LoadCursor(NULL, IDC_ARROW);
                
                if (!RegisterClassExW(&wcRun)) {
                    MessageBoxW(NULL, L"注册运行窗口类失败！", L"错误", MB_ICONERROR);
                    return 0;
                }
                
                hRunWindow = CreateWindowExW(
                    0,
                    RUN_CLASS_NAME,
                    L"Llama 运行",
                    WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
                    CW_USEDEFAULT, CW_USEDEFAULT, 580, 700,
                    NULL, NULL, NULL, NULL);
                
                ShowWindow(hRunWindow, SW_SHOW);
                UpdateWindow(hRunWindow);
                
                MSG msg = {0};
                while (GetMessage(&msg, NULL, 0, 0)) {
                    TranslateMessage(&msg);
                    DispatchMessage(&msg);
                }
                
                UnregisterClassW(RUN_CLASS_NAME, NULL);
            } else if (LOWORD(wParam) == ID_BUTTON_MAIN_CONVERT && HIWORD(wParam) == BN_CLICKED) {
                BOOL configExists = LoadConvertConfig();
                PostMessageW(hwnd, WM_CLOSE, 0, 0);
                
                if (configExists) {
                    const wchar_t* CONVERT_CLASS_NAME = L"LlamaConvertClass";
                    
                    WNDCLASSEXW wcConvert = {0};
                    wcConvert.cbSize = sizeof(WNDCLASSEXW);
                    wcConvert.lpfnWndProc = ConvertWindowProc;
                    wcConvert.hInstance = NULL;
                    wcConvert.lpszClassName = CONVERT_CLASS_NAME;
                    wcConvert.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
                    wcConvert.hCursor = LoadCursor(NULL, IDC_ARROW);
                    
                    if (!RegisterClassExW(&wcConvert)) {
                        MessageBoxW(NULL, L"注册转换窗口类失败！", L"错误", MB_ICONERROR);
                        return 0;
                    }
                    
                    hConvertWindow = CreateWindowExW(
                        0,
                        CONVERT_CLASS_NAME,
                        L"Llama 转换",
                        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
                        CW_USEDEFAULT, CW_USEDEFAULT, 530, 200,
                        NULL, NULL, NULL, NULL);
                    
                    ShowWindow(hConvertWindow, SW_SHOW);
                    UpdateWindow(hConvertWindow);
                    
                    MSG msg = {0};
                    while (GetMessage(&msg, NULL, 0, 0)) {
                        TranslateMessage(&msg);
                        DispatchMessage(&msg);
                    }
                    
                    UnregisterClassW(CONVERT_CLASS_NAME, NULL);
                }
            }
            break;
        }
        
        case WM_CLOSE: {
            PostQuitMessage(0);
            break;
        }
        
        case WM_DESTROY: {
            PostQuitMessage(0);
            break;
        }
        
        default: {
            return DefWindowProcW(hwnd, uMsg, wParam, lParam);
        }
    }
    return 0;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    LoadModelList();
    
    const wchar_t* CLASS_NAME = L"LlamaMainClass";
    
    WNDCLASSEXW wc = {0};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.lpfnWndProc = MainWindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    
    if (!RegisterClassExW(&wc)) {
        MessageBoxW(NULL, L"注册窗口类失败！", L"错误", MB_ICONERROR);
        return 1;
    }
    
    HWND hwnd = CreateWindowExW(
        0,
        CLASS_NAME,
        L"Llama 工具",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, 480, 220,
        NULL,
        NULL,
        hInstance,
        NULL);
    
    if (hwnd == NULL) {
        MessageBoxW(NULL, L"创建窗口失败！", L"错误", MB_ICONERROR);
        return 1;
    }
    
    hMainWindow = hwnd;
    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);
    
    MSG msg = {0};
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    
    return (int)msg.wParam;
}
