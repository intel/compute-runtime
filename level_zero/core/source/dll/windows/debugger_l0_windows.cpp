/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/debugger/debugger_l0.h"

namespace L0 {
std::unique_ptr<NEO::Debugger> DebuggerL0::create(NEO::Device *device) {
    return std::unique_ptr<NEO::Debugger>(nullptr);
}
} // namespace L0