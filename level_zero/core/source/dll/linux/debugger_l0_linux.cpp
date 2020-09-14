/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/debugger/debugger_l0.h"

namespace L0 {
std::unique_ptr<NEO::Debugger> DebuggerL0::create(NEO::Device *device) {
    auto debugger = debuggerL0Factory[device->getHardwareInfo().platform.eRenderCoreFamily](device);
    debugger->registerResourceClasses();
    return std::unique_ptr<DebuggerL0>(debugger);
}

} // namespace L0