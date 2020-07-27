/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/debugger/debugger_l0.h"

namespace L0 {
std::unique_ptr<NEO::Debugger> DebuggerL0::create(NEO::Device *device) {
    return std::unique_ptr<DebuggerL0>(debuggerL0Factory[device->getHardwareInfo().platform.eRenderCoreFamily](device));
}

} // namespace L0