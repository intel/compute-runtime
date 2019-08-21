/*
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/os_interface/aub_memory_operations_handler.h"

#include "runtime/aub_mem_dump/aub_mem_dump.h"
#include "runtime/memory_manager/graphics_allocation.h"

#include <algorithm>

namespace NEO {

AubMemoryOperationsHandler::AubMemoryOperationsHandler(aub_stream::AubManager *aubManager) {
    this->aubManager = aubManager;
}

bool AubMemoryOperationsHandler::makeResident(GraphicsAllocation &gfxAllocation) {
    if (!aubManager) {
        return false;
    }
    auto lock = acquireLock(resourcesLock);
    int hint = AubMemDump::DataTypeHintValues::TraceNotype;
    aubManager->writeMemory(gfxAllocation.getGpuAddress(), gfxAllocation.getUnderlyingBuffer(), gfxAllocation.getUnderlyingBufferSize(), gfxAllocation.storageInfo.getMemoryBanks(), hint, gfxAllocation.getUsedPageSize());
    residentAllocations.push_back(&gfxAllocation);
    return true;
}

bool AubMemoryOperationsHandler::evict(GraphicsAllocation &gfxAllocation) {
    auto lock = acquireLock(resourcesLock);
    auto itor = std::find(residentAllocations.begin(), residentAllocations.end(), &gfxAllocation);
    if (itor == residentAllocations.end()) {
        return false;
    } else {
        residentAllocations.erase(itor, itor + 1);
        return true;
    }
}

bool AubMemoryOperationsHandler::isResident(GraphicsAllocation &gfxAllocation) {
    auto lock = acquireLock(resourcesLock);
    auto itor = std::find(residentAllocations.begin(), residentAllocations.end(), &gfxAllocation);
    if (itor == residentAllocations.end()) {
        return false;
    } else {
        return true;
    }
}
void AubMemoryOperationsHandler::setAubManager(aub_stream::AubManager *aubManager) {
    this->aubManager = aubManager;
}

} // namespace NEO
