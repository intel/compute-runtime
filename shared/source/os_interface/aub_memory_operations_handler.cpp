/*
 * Copyright (C) 2019-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/aub_memory_operations_handler.h"

#include "shared/source/aub/aub_helper.h"
#include "shared/source/aub_mem_dump/aub_mem_dump.h"
#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/device/device.h"
#include "shared/source/gmm_helper/cache_settings_helper.h"
#include "shared/source/gmm_helper/gmm.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/memory_manager/graphics_allocation.h"

#include "aubstream/allocation_params.h"

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
        if (!isAubWritable(*allocation, device)) {
            continue;
        }

        uint64_t gpuAddress = device ? device->getGmmHelper()->decanonize(allocation->getGpuAddress()) : allocation->getGpuAddress();
        aub_stream::AllocationParams params(gpuAddress,
                                            allocation->getUnderlyingBuffer(),
                                            allocation->getUnderlyingBufferSize(),
                                            allocation->storageInfo.getMemoryBanks(),
                                            hint,
                                            allocation->getUsedPageSize());

        auto gmm = allocation->getDefaultGmm();

        if (gmm) {
            params.additionalParams.compressionEnabled = gmm->isCompressionEnabled;
            params.additionalParams.uncached = CacheSettingsHelper::isUncachedType(gmm->resourceParams.Usage);
        }

        if (allocation->storageInfo.cloningOfPageTables || !allocation->isAllocatedInLocalMemoryPool()) {
            aubManager->writeMemory2(params);
        } else {
            device->getDefaultEngine().commandStreamReceiver->writeMemoryAub(params);
        }

        if (!allocation->getAubInfo().writeMemoryOnly) {
            residentAllocations.push_back(allocation);
        }

        if (AubHelper::isOneTimeAubWritableAllocationType(allocation->getAllocationType())) {
            setAubWritable(false, *allocation, device);
        }
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

bool AubMemoryOperationsHandler::isAubWritable(GraphicsAllocation &graphicsAllocation, Device *device) const {
    if (!device) {
        return false;
    }
    auto bank = static_cast<uint32_t>(getMemoryBanksBitfield(&graphicsAllocation, device).to_ulong());
    if (bank == 0u || graphicsAllocation.storageInfo.cloningOfPageTables) {
        bank = GraphicsAllocation::defaultBank;
    }
    return graphicsAllocation.isAubWritable(bank);
}

void AubMemoryOperationsHandler::setAubWritable(bool writable, GraphicsAllocation &graphicsAllocation, Device *device) {
    if (!device) {
        return;
    }
    auto bank = static_cast<uint32_t>(getMemoryBanksBitfield(&graphicsAllocation, device).to_ulong());
    if (bank == 0u || graphicsAllocation.storageInfo.cloningOfPageTables) {
        bank = GraphicsAllocation::defaultBank;
    }
    graphicsAllocation.setAubWritable(writable, bank);
}

DeviceBitfield AubMemoryOperationsHandler::getMemoryBanksBitfield(GraphicsAllocation *allocation, Device *device) const {
    if (allocation->getMemoryPool() == MemoryPool::LocalMemory) {
        if (allocation->storageInfo.memoryBanks.any()) {
            if (allocation->storageInfo.cloningOfPageTables ||
                device->getDefaultEngine().commandStreamReceiver->isMultiOsContextCapable()) {
                return allocation->storageInfo.memoryBanks;
            }
        }
        return device->getDeviceBitfield();
    }
    return {};
}

} // namespace NEO
