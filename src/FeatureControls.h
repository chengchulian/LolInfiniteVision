#pragma once

#include "GameTime.h"
#include "Hotkeys.h"

namespace features {
void Start(const gametime::ReadyContext& context, const std::array<bool, 4>& enabled,
    const std::array<UINT, 4>& bindings);
}
