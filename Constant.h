#pragma once
#define VERSION L"1.1"
#include "Method.h"

// 特征码
extern const char* const VISION_SIGNATURE_CODE;
extern const char* const TIME_SIGNATURE_CODE;
extern const char* const RCX_SIGNATURE_CODE;

// 偏移
// RAX默认偏移
extern DWORD64 RCX_OFFSET_DEFAULT;
extern const DWORD64 RCX_OFFSET_CURRENT;
// 窗口 & 配置
extern const wchar_t* const CLIENT_NAME;
extern const wchar_t* const CONFIG_PATH;

// 程序代码前缀
extern const WCHAR APP_CODE[512];
