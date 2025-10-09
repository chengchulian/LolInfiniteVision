#include "pch.h"
#include "Constant.h"

// 无限视距特征码
const char* const VISION_SIGNATURE_CODE =
    "77 ?? 48 8B 83 ?? ?? ?? ?? F3 ?? ?? ?? ?? F3 ?? ?? ??";

// 时间特征码
const char* const TIME_SIGNATURE_CODE =
    "F3 0F 5C 35 ?? ?? ?? ?? 0F 28 F8";


const char* const RCX_SIGNATURE_CODE = "48 8B 05 ?? ?? ?? ?? BA ?? ?? ?? ?? 48 8B 0D ?? ?? ?? ?? 48 8B 58 18";

DWORD64 RCX_OFFSET_DEFAULT = 0x18;

const DWORD64 RCX_OFFSET_CURRENT = 0x324;

// FindWindowW 的客户端类名
const wchar_t* const CLIENT_NAME = L"RiotWindowClass";

// 配置文件名
const wchar_t* const CONFIG_PATH = L".\\LolInfiniteVision.ini";

// 程序代码前缀（日志用）
const WCHAR APP_CODE[512] = L"[902] ";
