/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "graphics_allocation.h"

#include "shared/source/gmm_helper/gmm.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/helpers/aligned_memory.h"
#include "shared/source/memory_manager/memory_manager.h"
#include "shared/source/utilities/logger.h"

namespace NEO {
void GraphicsAllocation::setAllocationType(AllocationType allocationType) {
    this->allocationType = allocationType;
    FileLoggerInstance().logAllocation(this);
}

GraphicsAllocation::GraphicsAllocation(uint32_t rootDeviceIndex, size_t numGmms, AllocationType allocationType, void *cpuPtrIn, uint64_t gpuAddress,
                                       uint64_t baseAddress, size_t sizeIn, MemoryPool::Type pool, size_t maxOsContextCount)
    : rootDeviceIndex(rootDeviceIndex),
      gpuBaseAddress(baseAddress),
      gpuAddress(GmmHelper::canonize(gpuAddress)),
      size(sizeIn),
      cpuPtr(cpuPtrIn),
      memoryPool(pool),
      allocationType(allocationType),
      usageInfos(maxOsContextCount) {
    gmms.resize(numGmms);
}

GraphicsAllocation::GraphicsAllocation(uint32_t rootDeviceIndex, size_t numGmms, AllocationType allocationType, void *cpuPtrIn, size_t sizeIn,
                                       osHandle sharedHandleIn, MemoryPool::Type pool, size_t maxOsContextCount)
    : rootDeviceIndex(rootDeviceIndex),
      gpuAddress(GmmHelper::canonize(castToUint64(cpuPtrIn))),
      size(sizeIn),
      cpuPtr(cpuPtrIn),
      memoryPool(pool),
      allocationType(allocationType),
      usageInfos(maxOsContextCount) {
    sharingInfo.sharedHandle = sharedHandleIn;
    gmms.resize(numGmms);
}

GraphicsAllocation::~GraphicsAllocation() = default;

void GraphicsAllocation::updateTaskCount(uint32_t newTaskCount, uint32_t contextId) {
    if (usageInfos[contextId].taskCount == objectNotUsed) {
        registeredContextsNum++;
    }
    if (newTaskCount == objectNotUsed) {
        registeredContextsNum--;
    }
    usageInfos[contextId].taskCount = newTaskCount;
}

std::string GraphicsAllocation::getAllocationInfoString() const {
    return "";
}

uint32_t GraphicsAllocation::getUsedPageSize() const {
    switch (this->memoryPool) {
    case MemoryPool::System64KBPages:
    case MemoryPool::System64KBPagesWith32BitGpuAddressing:
    case MemoryPool::LocalMemory:
        return MemoryConstants::pageSize64k;
    default:
        return MemoryConstants::pageSize;
    }
}

bool GraphicsAllocation::isAllocationLockable() const {
    auto gmm = getDefaultGmm();
    if (!gmm) {
        return true;
    }
    return 0 == gmm->resourceParams.Flags.Info.NotLockable;
}

void GraphicsAllocation::setAubWritable(bool writable, uint32_t banks) {
    UNRECOVERABLE_IF(banks == 0);
    aubInfo.aubWritable = static_cast<uint32_t>(setBits(aubInfo.aubWritable, writable, banks));
}

bool GraphicsAllocation::isAubWritable(uint32_t banks) const {
    return isAnyBitSet(aubInfo.aubWritable, banks);
}

void GraphicsAllocation::setTbxWritable(bool writable, uint32_t banks) {
    UNRECOVERABLE_IF(banks == 0);
    aubInfo.tbxWritable = static_cast<uint32_t>(setBits(aubInfo.tbxWritable, writable, banks));
}

bool GraphicsAllocation::isCompressionEnabled() const {
    return (getDefaultGmm() && getDefaultGmm()->isCompressionEnabled);
}

bool GraphicsAllocation::isTbxWritable(uint32_t banks) const {
    return isAnyBitSet(aubInfo.tbxWritable, banks);
}

constexpr uint32_t GraphicsAllocation::objectNotUsed;
constexpr uint32_t GraphicsAllocation::objectNotResident;
constexpr uint32_t GraphicsAllocation::objectAlwaysResident;
} // namespace NEO
