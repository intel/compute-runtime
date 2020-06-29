/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/debugger/debugger_l0.h"

#include "shared/source/device/device.h"
#include "shared/source/helpers/constants.h"
#include "shared/source/memory_manager/allocation_properties.h"
#include "shared/source/memory_manager/memory_manager.h"

namespace L0 {
DebuggerL0::DebuggerL0(NEO::Device *device) : device(device) {
    isLegacyMode = false;

    NEO::AllocationProperties properties{device->getRootDeviceIndex(), true, MemoryConstants::pageSize,
                                         NEO::GraphicsAllocation::AllocationType::DEBUG_SBA_TRACKING_BUFFER,
                                         false,
                                         CommonConstants::allDevicesBitfield};

    sbaAllocation = device->getMemoryManager()->allocateGraphicsMemoryWithProperties(properties);
}

DebuggerL0 ::~DebuggerL0() {
    device->getMemoryManager()->freeGraphicsMemory(sbaAllocation);
}

bool DebuggerL0::isDebuggerActive() {
    return true;
}

} // namespace L0