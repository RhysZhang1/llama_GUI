/*
 * common.h - Llama GUI v2 公共头文件
 * 包含所有窗口共享的类型、常量、全局变量和辅助函数声明
 *
 * 编译: gcc -o llama_gui.exe main.c run_window.c convert_window.c -mwindows -s -O2 -lole32 -lshell32 -lcomctl32
 */

#ifndef COMMON_H
#define COMMON_H

/* ========== Windows 头文件 ========== */
#include <windows.h>
#include <commdlg.h>
#include <shlobj.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <shellapi.h>

/* ========== 窗口类名 ========== */
#define MAIN_CLASS      L"LlamaMainWnd"
#define RUN_CLASS       L"LlamaRunWnd"
#define CONVERT_CLASS   L"LlamaConvertWnd"
#define MODELS_CLASS    L"LlamaModelsWnd"

/* ========== 配置文件路径 ========== */
#define CONFIG_FILE          "llama_config.txt"
#define CONVERT_CONFIG_FILE  "llama_convert_config.txt"
#define MODELS_FILE          "llama_models.txt"

/* ========== 主窗口控件 ID ========== */
#define ID_BTN_RUN           1101
#define ID_BTN_CONVERT       1102
#define ID_BTN_MODELS        1103
#define ID_STATIC_TITLE      1104

/* ========== 运行窗口控件 ID ========== */
#define ID_CBO_MODE          1201
#define ID_CBO_MODEL         1202
#define ID_EDIT_CTX          1203
#define ID_EDIT_NGL          1204
#define ID_EDIT_TEMP         1205
#define ID_EDIT_REPEAT       1206
#define ID_EDIT_N            1207
#define ID_EDIT_PORT         1208
#define ID_CHK_MLI           1209
#define ID_CHK_EXTRA         1210
#define ID_EDIT_EXTRA        1211
#define ID_BTN_PREVIEW       1212
#define ID_BTN_EXECUTE       1213
#define ID_BTN_BACK          1214
#define ID_EDIT_CMD_PREVIEW  1215
#define ID_CHK_MTP           1216
#define ID_CHK_LLAMA_PATH      1217
#define ID_EDIT_LLAMA_BIN      1218
#define ID_BTN_BROWSE_LLAMA_BIN 1219
#define ID_BTN_HELP            1220

/* ========== 转换窗口控件 ID ========== */
#define ID_EDIT_LLAMA_PATH   1301
#define ID_EDIT_SRC_MODEL    1302
#define ID_EDIT_SRC_LORA     1303
#define ID_EDIT_DST_MODEL    1304
#define ID_EDIT_OUTTYPE      1305
#define ID_BTN_BROWSE_LLAMA  1306
#define ID_BTN_BROWSE_SRC    1307
#define ID_BTN_BROWSE_LORA   1308
#define ID_BTN_BROWSE_DST    1309
#define ID_BTN_CONV_MODEL    1310
#define ID_BTN_CONV_LORA     1311

/* ========== 模型管理窗口控件 ID ========== */
#define ID_LST_MODELS        1401
#define ID_EDIT_MDL_NAME     1402
#define ID_EDIT_MDL_PATH     1403
#define ID_BTN_MDL_BROWSE    1404
#define ID_BTN_MDL_ADD       1405
#define ID_BTN_MDL_UPDATE    1406
#define ID_BTN_MDL_DEL       1407
#define ID_BTN_MDL_CLOSE     1408

/* ========== 数据常量 ========== */
#define MAX_MODELS    100
#define MAX_NAME      128
#define PATH_LEN      512
#define MAX_CMD       8192
#define MAX_BUF       1024

/* ========== 模型信息结构 ========== */
typedef struct {
    char name[MAX_NAME];
    char path[PATH_LEN];
} ModelInfo;

/* ========== 运行配置结构 ========== */
typedef struct {
    char mode[32];
    char model[128];
    char ctxSize[32];
    char ngl[32];
    char temp[32];
    char repeatPenalty[32];
    char n[32];
    char port[32];
    BOOL mli;
    BOOL extraEnabled;
    char extraParams[MAX_BUF];
    BOOL mtp;
    BOOL useLlamaPath;
    char llamaBinPath[PATH_LEN];
} RunConfig;

/* ========== 转换配置结构 ========== */
typedef struct {
    char llamaPath[PATH_LEN];
    char modelSrcPath[PATH_LEN];
    char loraSrcPath[PATH_LEN];
    char modelDstPath[PATH_LEN];
    char outtype[64];
} ConvertConfig;

/* ========== 全局变量声明 (extern) ========== */
extern ModelInfo     g_models[MAX_MODELS];
extern int           g_modelCount;
extern RunConfig     g_runCfg;
extern ConvertConfig g_convCfg;
extern HINSTANCE     g_hInst;
extern HFONT         g_hFontNormal;
extern HFONT         g_hFontBold;
extern HFONT         g_hFontTitle;
extern BOOL           g_appExiting;   /* X 按钮退出整个程序标志 */

/* ========== main.c 提供的工具函数 ========== */

/* 模型列表文件读写 */
void LoadModels(void);
void SaveModels(void);

/* 运行配置文件读写 */
void LoadRunConfig(void);
void SaveRunConfig(void);

/* 转换配置文件读写 */
void LoadConvertConfig(void);
void SaveConvertConfig(void);

/* 启动命令（在新控制台窗口） */
void ExecuteCommand(const char* cmd);

/* 延迟打开浏览器 */
void OpenBrowserAfterDelay(const char* port);

/* 文件夹浏览对话框 */
BOOL BrowseForFolder(HWND hwnd, const wchar_t* title, wchar_t* outPath, DWORD outSize);

/* 文件打开对话框 */
BOOL BrowseForFile(HWND hwnd, const wchar_t* title, const wchar_t* filter,
                   wchar_t* outPath, DWORD outSize);

/* 文件保存对话框 */
BOOL BrowseForSaveFile(HWND hwnd, const wchar_t* title, const wchar_t* filter,
                       const wchar_t* defExt, wchar_t* outPath, DWORD outSize);

/* 便捷控件创建 */
HWND CreateLabel(HWND parent, const wchar_t* text, int x, int y, int w, int height);
HWND CreateEdit(HWND parent, const wchar_t* defaultText,
                int x, int y, int w, int height, int id);
HWND CreateButton(HWND parent, const wchar_t* text,
                  int x, int y, int w, int height, int id);
HWND CreateCombo(HWND parent, int x, int y, int w, int height, int id, BOOL dropdownList);
HWND CreateGroupBox(HWND parent, const wchar_t* text,
                    int x, int y, int w, int height);
HWND CreateCheckbox(HWND parent, const wchar_t* text,
                    int x, int y, int w, int height, int id);

/* 宽字符 <-> UTF-8 转换辅助 */
void ToUTF8(const wchar_t* src, char* dst, int dstSize);
void ToWide(const char* src, wchar_t* dst, int dstSize);

/* 初始化字体 */
void InitFonts(void);

/* 清理字体 */
void CleanupFonts(void);

/* 居中窗口 */
void CenterWindow(HWND hwnd);

/* 运行子窗口消息循环（模态方式） */
void RunModalLoop(HWND hwnd);

/* ========== 窗口过程函数声明 ========== */
LRESULT CALLBACK RunWindowProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
LRESULT CALLBACK ConvertWindowProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
LRESULT CALLBACK ModelsWindowProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);

#endif /* COMMON_H */
