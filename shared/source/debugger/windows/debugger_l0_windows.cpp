/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/debugger/debugger_l0.h"
#include "shared/source/device/device.h"
#include "shared/source/helpers/hw_helper.h"
#include "shared/source/kernel/debug_data.h"
#include "shared/source/os_interface/windows/wddm/wddm.h"

namespace NEO {
bool DebuggerL0::initDebuggingInOs(NEO::OSInterface *osInterface) {

    if (osInterface == nullptr) {
        return false;
    }
    if (osInterface->isDebugAttachAvailable() == false) {
        return false;
    }

    return true;
}

void DebuggerL0::initSbaTrackingMode() {
    singleAddressSpaceSbaTracking = true;
}

void DebuggerL0::registerElf(NEO::DebugData *debugData, NEO::GraphicsAllocation *isaAllocation) {
}

bool DebuggerL0::attachZebinModuleToSegmentAllocations(const StackVec<NEO::GraphicsAllocation *, 32> &kernelAlloc, uint32_t &moduleHandle) {
    return false;
}

bool DebuggerL0::removeZebinModule(uint32_t moduleHandle) {
    return false;
}

void DebuggerL0::notifyCommandQueueCreated() {
}

void DebuggerL0::notifyCommandQueueDestroyed() {
}

} // namespace NEO
