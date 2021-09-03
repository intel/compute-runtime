/*
 * Copyright (C) 2019-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/aub_memory_operations_handler.h"

#include "shared/source/gmm_helper/gmm.h"
#include "shared/source/memory_manager/graphics_allocation.h"

#include "aub_mem_dump.h"
#include "third_party/aub_stream/headers/allocation_params.h"

#include <algorithm>

namespace NEO {

AubMemoryOperationsHandler::AubMemoryOperationsHandler(aub_stream::AubManager *aubManager) {
    this->aubManager = aubManager;
}

MemoryOperationsStatus AubMemoryOperationsHandler::makeResident(Device *device, ArrayRef<GraphicsAllocation *> gfxAllocations) {
    if (!aubManager) {
        return MemoryOperationsStatus::DEVICE_UNINITIALIZED;
    }
    auto lock = acquireLock(resourcesLock);
    int hint = AubMemDump::DataTypeHintValues::TraceNotype;
    for (const auto &allocation : gfxAllocations) {
        aub_stream::AllocationParams params(allocation->getGpuAddress(),
                                            allocation->getUnderlyingBuffer(),
                                            allocation->getUnderlyingBufferSize(),
                                            allocation->storageInfo.getMemoryBanks(),
                                            hint,
                                            allocation->getUsedPageSize());

        auto gmm = allocation->getDefaultGmm();

        params.additionalParams.compressionEnabled = gmm ? gmm->isCompressionEnabled : false;

        aubManager->writeMemory2(params);
        residentAllocations.push_back(allocation);
    }
    return MemoryOperationsStatus::SUCCESS;
}

MemoryOperationsStatus AubMemoryOperationsHandler::evict(Device *device, GraphicsAllocation &gfxAllocation) {
    auto lock = acquireLock(resourcesLock);
    auto itor = std::find(residentAllocations.begin(), residentAllocations.end(), &gfxAllocation);
    if (itor == residentAllocations.end()) {
        return MemoryOperationsStatus::MEMORY_NOT_FOUND;
    } else {
        residentAllocations.erase(itor, itor + 1);
        return MemoryOperationsStatus::SUCCESS;
    }
}

MemoryOperationsStatus AubMemoryOperationsHandler::makeResidentWithinOsContext(OsContext *osContext, ArrayRef<GraphicsAllocation *> gfxAllocations, bool evictable) {
    return makeResident(nullptr, gfxAllocations);
}

MemoryOperationsStatus AubMemoryOperationsHandler::evictWithinOsContext(OsContext *osContext, GraphicsAllocation &gfxAllocation) {
    return evict(nullptr, gfxAllocation);
}

MemoryOperationsStatus AubMemoryOperationsHandler::isResident(Device *device, GraphicsAllocation &gfxAllocation) {
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
