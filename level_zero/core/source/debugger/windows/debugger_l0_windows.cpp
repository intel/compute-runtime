/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/device/device.h"
#include "shared/source/helpers/hw_helper.h"
#include "shared/source/kernel/debug_data.h"

#include "level_zero/core/source/debugger/debugger_l0.h"

namespace L0 {
bool DebuggerL0::initDebuggingInOs(NEO::OSInterface *osInterface) {
    return false;
}

void DebuggerL0::registerElf(NEO::DebugData *debugData, NEO::GraphicsAllocation *isaAllocation) {
}

} // namespace L0
