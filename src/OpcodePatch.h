#pragma once

#include "GameTime.h"

namespace features {
class OpcodePatch {
public:
    void Locate(const gametime::ReadyContext& context, const char* signature, BYTE original, BYTE target);
    bool Set(bool enabled);
    bool Enabled() const { return enabled_; }
private:
    DWORD64 address_ = 0;
    BYTE original_ = 0, target_ = 0;
    bool enabled_ = false;
};
}
