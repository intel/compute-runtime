/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "graphics_allocation.h"

#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/gmm_helper/gmm.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/helpers/aligned_memory.h"
#include "shared/source/helpers/bit_helpers.h"
#include "shared/source/memory_manager/allocation_properties.h"
#include "shared/source/memory_manager/memory_manager.h"
#include "shared/source/os_interface/os_context.h"
#include "shared/source/utilities/logger.h"
#include "shared/source/utilities/logger_neo_only.h"

namespace NEO {
void GraphicsAllocation::setAllocationType(AllocationType allocationType) {
    if (this->allocationType != allocationType) {
        this->allocationType = allocationType;
        logAllocation(fileLoggerInstance(), this, nullptr);
    }
}

GraphicsAllocation::GraphicsAllocation(uint32_t rootDeviceIndex, size_t numGmms, AllocationType allocationType, void *cpuPtrIn, uint64_t canonizedGpuAddress,
                                       uint64_t baseAddress, size_t sizeIn, MemoryPool pool, size_t maxOsContextCount)
    : rootDeviceIndex(rootDeviceIndex),
      gpuBaseAddress(baseAddress),
      gpuAddress(canonizedGpuAddress),
      size(sizeIn),
      cpuPtr(cpuPtrIn),
      memoryPool(pool),
      allocationType(allocationType),
      usageInfos(maxOsContextCount),
      residency(maxOsContextCount) {
    gmms.resize(numGmms);
}

GraphicsAllocation::GraphicsAllocation(uint32_t rootDeviceIndex, size_t numGmms, AllocationType allocationType, void *cpuPtrIn, size_t sizeIn,
                                       osHandle sharedHandleIn, MemoryPool pool, size_t maxOsContextCount, uint64_t canonizedGpuAddress)
    : rootDeviceIndex(rootDeviceIndex),
      gpuAddress(canonizedGpuAddress),
      size(sizeIn),
      cpuPtr(cpuPtrIn),
      memoryPool(pool),
      allocationType(allocationType),
      usageInfos(maxOsContextCount),
      residency(maxOsContextCount) {
    sharingInfo.sharedHandle = sharedHandleIn;
    gmms.resize(numGmms);
}

GraphicsAllocation::~GraphicsAllocation() = default;

void GraphicsAllocation::updateTaskCount(TaskCountType newTaskCount, uint32_t contextId) {
    OPTIONAL_UNRECOVERABLE_IF(contextId >= usageInfos.size());
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

std::string GraphicsAllocation::getPatIndexInfoString(const ProductHelper &) const {
    return "";
}

uint32_t GraphicsAllocation::getUsedPageSize() const {
    switch (this->memoryPool) {
    case MemoryPool::system64KBPages:
    case MemoryPool::system64KBPagesWith32BitGpuAddressing:
    case MemoryPool::localMemory:
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
    return (getDefaultGmm() && getDefaultGmm()->isCompressionEnabled());
}

bool GraphicsAllocation::isTbxWritable(uint32_t banks) const {
    return isAnyBitSet(aubInfo.tbxWritable, banks);
}

void GraphicsAllocation::prepareHostPtrForResidency(CommandStreamReceiver *csr) {
    if (hostPtrTaskCountAssignment > 0) {
        auto allocTaskCount = getTaskCount(csr->getOsContext().getContextId());
        auto currentTaskCount = csr->peekTaskCount() + 1;
        if (currentTaskCount > allocTaskCount || allocTaskCount == GraphicsAllocation::objectNotResident) {
            updateTaskCount(currentTaskCount, csr->getOsContext().getContextId());
            hostPtrTaskCountAssignment--;
        }
    }
}

uint32_t GraphicsAllocation::getNumHandlesForKmdSharedAllocation(uint32_t numBanks) {
    return (numBanks > 1) && (debugManager.flags.CreateKmdMigratedSharedAllocationWithMultipleBOs.get() != 0) ? numBanks : 1u;
}

void GraphicsAllocation::updateCompletionDataForAllocationAndFragments(uint64_t newFenceValue, uint32_t contextId) {
    getResidencyData().updateCompletionData(newFenceValue, contextId);

    for (uint32_t allocationId = 0; allocationId < fragmentsStorage.fragmentCount; allocationId++) {
        auto residencyData = fragmentsStorage.fragmentStorageData[allocationId].residency;
        residencyData->updateCompletionData(newFenceValue, contextId);
    }
}

bool GraphicsAllocation::hasAllocationReadOnlyType() {
    if (allocationType == AllocationType::kernelIsa ||
        allocationType == AllocationType::kernelIsaInternal ||
        allocationType == AllocationType::commandBuffer ||
        allocationType == AllocationType::ringBuffer) {
        return true;
    }

    if (debugManager.flags.ReadOnlyAllocationsTypeMask.get() != 0) {
        UNRECOVERABLE_IF(allocationType == AllocationType::unknown);
        auto maskVal = debugManager.flags.ReadOnlyAllocationsTypeMask.get();
        if (maskVal & (1llu << (static_cast<int64_t>(getAllocationType()) - 1))) {
            return true;
        }
    }
    return false;
}

void GraphicsAllocation::checkAllocationTypeReadOnlyRestrictions(const AllocationData &allocData) {
    if (getAllocationType() == AllocationType::commandBuffer &&
        (allocData.flags.cantBeReadOnly | allocData.flags.multiOsContextCapable)) {
        setAsCantBeReadOnly(true);
        return;
    }
    setAsCantBeReadOnly(!hasAllocationReadOnlyType());
}

constexpr TaskCountType GraphicsAllocation::objectNotUsed;
constexpr TaskCountType GraphicsAllocation::objectNotResident;
constexpr TaskCountType GraphicsAllocation::objectAlwaysResident;
} // namespace NEO
