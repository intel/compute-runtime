/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/helpers/in_order_cmd_helpers.h"

#include "shared/source/memory_manager/memory_manager.h"

#include <cstdint>
#include <string.h>
#include <vector>

namespace L0 {

InOrderExecInfo::~InOrderExecInfo() {
    memoryManager.freeGraphicsMemory(&deviceCounterAllocation);
}

InOrderExecInfo::InOrderExecInfo(NEO::GraphicsAllocation &deviceCounterAllocation, NEO::MemoryManager &memoryManager, uint32_t partitionCount, bool regularCmdList, bool atomicDeviceSignalling)
    : deviceCounterAllocation(deviceCounterAllocation), memoryManager(memoryManager), regularCmdList(regularCmdList) {

    numDevicePartitionsToWait = atomicDeviceSignalling ? 1 : partitionCount;
    numHostPartitionsToWait = partitionCount;

    hostAddress = reinterpret_cast<uint64_t *>(deviceCounterAllocation.getUnderlyingBuffer());

    reset();
}

void InOrderExecInfo::reset() {
    counterValue = 0;
    regularCmdListSubmissionCounter = 0;

    memset(deviceCounterAllocation.getUnderlyingBuffer(), 0, deviceCounterAllocation.getUnderlyingBufferSize());
}

} // namespace L0
