/*
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/os_interface/aub_memory_operations_handler.h"

#include "core/memory_manager/graphics_allocation.h"
#include "runtime/aub_mem_dump/aub_mem_dump.h"

#include <algorithm>

namespace NEO {

AubMemoryOperationsHandler::AubMemoryOperationsHandler(aub_stream::AubManager *aubManager) {
    this->aubManager = aubManager;
}

MemoryOperationsStatus AubMemoryOperationsHandler::makeResident(GraphicsAllocation &gfxAllocation) {
    if (!aubManager) {
        return MemoryOperationsStatus::DEVICE_UNINITIALIZED;
    }
    auto lock = acquireLock(resourcesLock);
    int hint = AubMemDump::DataTypeHintValues::TraceNotype;
    aubManager->writeMemory(gfxAllocation.getGpuAddress(), gfxAllocation.getUnderlyingBuffer(), gfxAllocation.getUnderlyingBufferSize(), gfxAllocation.storageInfo.getMemoryBanks(), hint, gfxAllocation.getUsedPageSize());
    residentAllocations.push_back(&gfxAllocation);
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
