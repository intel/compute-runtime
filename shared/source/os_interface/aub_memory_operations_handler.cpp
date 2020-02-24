/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/aub_memory_operations_handler.h"

#include "shared/source/memory_manager/graphics_allocation.h"

#include "opencl/source/aub_mem_dump/aub_mem_dump.h"

#include <algorithm>

namespace NEO {

AubMemoryOperationsHandler::AubMemoryOperationsHandler(aub_stream::AubManager *aubManager) {
    this->aubManager = aubManager;
}

MemoryOperationsStatus AubMemoryOperationsHandler::makeResident(ArrayRef<GraphicsAllocation *> gfxAllocations) {
    if (!aubManager) {
        return MemoryOperationsStatus::DEVICE_UNINITIALIZED;
    }
    auto lock = acquireLock(resourcesLock);
    int hint = AubMemDump::DataTypeHintValues::TraceNotype;
    for (const auto &allocation : gfxAllocations) {
        aubManager->writeMemory(allocation->getGpuAddress(),
                                allocation->getUnderlyingBuffer(),
                                allocation->getUnderlyingBufferSize(),
                                allocation->storageInfo.getMemoryBanks(),
                                hint,
                                allocation->getUsedPageSize());
        residentAllocations.push_back(allocation);
    }
    return MemoryOperationsStatus::SUCCESS;
}

MemoryOperationsStatus AubMemoryOperationsHandler::evict(GraphicsAllocation &gfxAllocation) {
    auto lock = acquireLock(resourcesLock);
    auto itor = std::find(residentAllocations.begin(), residentAllocations.end(), &gfxAllocation);
    if (itor == residentAllocations.end()) {
        return MemoryOperationsStatus::MEMORY_NOT_FOUND;
    } else {
        residentAllocations.erase(itor, itor + 1);
        return MemoryOperationsStatus::SUCCESS;
    }
}

MemoryOperationsStatus AubMemoryOperationsHandler::isResident(GraphicsAllocation &gfxAllocation) {
    auto lock = acquireLock(resourcesLock);
    auto itor = std::find(residentAllocations.begin(), residentAllocations.end(), &gfxAllocation);
    if (itor == residentAllocations.end()) {
        return MemoryOperationsStatus::MEMORY_NOT_FOUND;
    } else {
        return MemoryOperationsStatus::SUCCESS;
    }
}
void AubMemoryOperationsHandler::setAubManager(aub_stream::AubManager *aubManager) {
    this->aubManager = aubManager;
}

} // namespace NEO
