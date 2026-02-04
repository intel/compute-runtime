/*
 * Copyright (C) 2019-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/aub_memory_operations_handler.h"

#include "shared/source/aub/aub_helper.h"
#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/device/device.h"
#include "shared/source/gmm_helper/cache_settings_helper.h"
#include "shared/source/gmm_helper/gmm.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/gmm_helper/gmm_lib.h"
#include "shared/source/memory_manager/graphics_allocation.h"
#include "shared/source/os_interface/os_context.h"

#include "aubstream/allocation_params.h"
#include "aubstream/hint_values.h"

#include <algorithm>

namespace NEO {

AubMemoryOperationsHandler::AubMemoryOperationsHandler(aub_stream::AubManager *aubManager) {
    this->aubManager = aubManager;
}

MemoryOperationsStatus AubMemoryOperationsHandler::makeResident(Device *device, ArrayRef<GraphicsAllocation *> gfxAllocations, bool isDummyExecNeeded, const bool forcePagingFence) {
    if (!aubManager) {
        return MemoryOperationsStatus::deviceUninitialized;
    }

    if (!device) {
        return MemoryOperationsStatus::success;
    }

    device->getDefaultEngine().commandStreamReceiver->initializeEngine();

    return makeResidentWithinDevice(gfxAllocations, isDummyExecNeeded, forcePagingFence, device->getDeviceBitfield(), device->getDefaultEngine().commandStreamReceiver->isMultiOsContextCapable());
}

MemoryOperationsStatus AubMemoryOperationsHandler::makeResidentWithinDevice(ArrayRef<GraphicsAllocation *> gfxAllocations, bool isDummyExecNeeded, bool forcePagingFence, DeviceBitfield deviceBitfield, bool isMultiOsContextCapable) {
    auto lock = acquireLock(resourcesLock);
    int hint = aub_stream::DataTypeHintValues::TraceNotype;
    for (const auto &allocation : gfxAllocations) {
        if (!isAubWritable(*allocation, deviceBitfield, isMultiOsContextCapable)) {
            continue;
        }

        auto memoryBanks = static_cast<uint32_t>(getMemoryBanksBitfield(allocation, deviceBitfield, isMultiOsContextCapable).to_ulong());
        uint64_t gpuAddress = decanonizeAddress(allocation->getGpuAddress());
        aub_stream::AllocationParams params(gpuAddress,
                                            allocation->getUnderlyingBuffer(),
                                            allocation->getUnderlyingBufferSize(),
                                            memoryBanks,
                                            hint,
                                            allocation->getUsedPageSize());

        auto gmm = allocation->getDefaultGmm();

        if (gmm) {
            params.additionalParams.compressionEnabled = gmm->isCompressionEnabled();
            params.additionalParams.uncached = CacheSettingsHelper::isUncachedType(gmm->getResourceUsageType());
        }

        aubManager->writeMemory2(params);

        if (!allocation->getAubInfo().writeMemoryOnly) {
            residentAllocations.push_back(allocation);
        }

        if (AubHelper::isOneTimeAubWritableAllocationType(allocation->getAllocationType())) {
            setAubWritable(false, *allocation, deviceBitfield, isMultiOsContextCapable);
        }
    }
    return MemoryOperationsStatus::success;
}

MemoryOperationsStatus AubMemoryOperationsHandler::lock(Device *device, ArrayRef<GraphicsAllocation *> gfxAllocations) {
    return makeResident(device, gfxAllocations, false, false);
}

MemoryOperationsStatus AubMemoryOperationsHandler::evict(Device *device, GraphicsAllocation &gfxAllocation) {
    auto lock = acquireLock(resourcesLock);
    auto itor = std::find(residentAllocations.begin(), residentAllocations.end(), &gfxAllocation);
    if (itor == residentAllocations.end()) {
        return MemoryOperationsStatus::memoryNotFound;
    } else {
        residentAllocations.erase(itor, itor + 1);
        return MemoryOperationsStatus::success;
    }
}
MemoryOperationsStatus AubMemoryOperationsHandler::free(Device *device, GraphicsAllocation &gfxAllocation) {
    auto lock = acquireLock(resourcesLock);
    auto itor = std::find(residentAllocations.begin(), residentAllocations.end(), &gfxAllocation);
    if (itor != residentAllocations.end()) {
        residentAllocations.erase(itor, itor + 1);
    }
    return MemoryOperationsStatus::success;
}

MemoryOperationsStatus AubMemoryOperationsHandler::makeResidentWithinOsContext(OsContext *osContext, ArrayRef<GraphicsAllocation *> gfxAllocations, bool evictable, const bool forcePagingFence, const bool acquireLock) {
    if (!osContext) {
        return MemoryOperationsStatus::success;
    }
    return makeResidentWithinDevice(gfxAllocations, false, forcePagingFence, osContext->getDeviceBitfield(), osContext->getNumSupportedDevices() > 1);
}

MemoryOperationsStatus AubMemoryOperationsHandler::evictWithinOsContext(OsContext *osContext, GraphicsAllocation &gfxAllocation) {
    return evict(nullptr, gfxAllocation);
}

MemoryOperationsStatus AubMemoryOperationsHandler::isResident(Device *device, GraphicsAllocation &gfxAllocation) {
    auto lock = acquireLock(resourcesLock);
    auto itor = std::find(residentAllocations.begin(), residentAllocations.end(), &gfxAllocation);
    if (itor == residentAllocations.end()) {
        return MemoryOperationsStatus::memoryNotFound;
    } else {
        return MemoryOperationsStatus::success;
    }
}

void AubMemoryOperationsHandler::setAubManager(aub_stream::AubManager *aubManager) {
    this->aubManager = aubManager;
}

bool AubMemoryOperationsHandler::isAubWritable(GraphicsAllocation &graphicsAllocation, DeviceBitfield contextDeviceBitfield, bool isMultiOsContextCapable) const {
    auto bank = static_cast<uint32_t>(getMemoryBanksBitfield(&graphicsAllocation, contextDeviceBitfield, isMultiOsContextCapable).to_ulong());
    if (bank == 0u || graphicsAllocation.storageInfo.cloningOfPageTables) {
        bank = GraphicsAllocation::defaultBank;
    }
    return graphicsAllocation.isAubWritable(bank);
}

void AubMemoryOperationsHandler::setAubWritable(bool writable, GraphicsAllocation &graphicsAllocation, DeviceBitfield contextDeviceBitfield, bool isMultiOsContextCapable) {

    auto bank = static_cast<uint32_t>(getMemoryBanksBitfield(&graphicsAllocation, contextDeviceBitfield, isMultiOsContextCapable).to_ulong());
    if (bank == 0u || graphicsAllocation.storageInfo.cloningOfPageTables) {
        bank = GraphicsAllocation::defaultBank;
    }
    graphicsAllocation.setAubWritable(writable, bank);
}

DeviceBitfield AubMemoryOperationsHandler::getMemoryBanksBitfield(GraphicsAllocation *allocation, DeviceBitfield contextDeviceBitfield, bool isMultiOsContextCapable) const {
    if (allocation->getMemoryPool() == MemoryPool::localMemory) {
        if (allocation->storageInfo.memoryBanks.any()) {
            if (allocation->storageInfo.cloningOfPageTables ||
                isMultiOsContextCapable) {
                return allocation->storageInfo.memoryBanks;
            }
        }
        return contextDeviceBitfield;
    }
    return {};
}

void AubMemoryOperationsHandler::processFlushResidency(CommandStreamReceiver *csr) {
    auto lock = acquireLock(resourcesLock);
    for (const auto &allocation : this->residentAllocations) {
        csr->writeMemory(*allocation);
    }
}

} // namespace NEO
