#include "Constant.h"

const char* const TIME_SIGNATURE_CODE =
    "F3 0F 5C 35 ?? ?? ?? ?? 0F 28 F8";

const char* const VISION_SWITCH_SIGNATURE_CODE =
    "75 0E 48 8B 83 10 03 00 00 F3 0F 10 40 28";
const BYTE VISION_SWITCH_EXPECTED_OPCODE = 0x75;
const BYTE VISION_SWITCH_TARGET_OPCODE = 0x74;

const char* const ATTACK_RANGE_SIGNATURE_CODE =
    "75 12 48 8B 4F 28 83 79 50 03";
const BYTE ATTACK_RANGE_EXPECTED_OPCODE = 0x75;
const BYTE ATTACK_RANGE_TARGET_OPCODE = 0x74;

const wchar_t* const CONFIG_PATH = L".\\LolInfiniteVision.ini";

const WCHAR APP_CODE[512] = L"[902] ";
