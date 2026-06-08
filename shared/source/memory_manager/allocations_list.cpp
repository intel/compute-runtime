/*
 * Copyright (C) 2021-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/memory_manager/allocations_list.h"

#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/command_stream/task_count_helper.h"
#include "shared/source/device/device.h"
#include "shared/source/helpers/aligned_memory.h"
#include "shared/source/os_interface/os_context.h"

#include <algorithm>

namespace NEO {

struct ReusableAllocationRequirements {
    ReusableAllocationRequirements() = delete;
    ReusableAllocationRequirements(CommandStreamReceiver *csr, const void *requiredPtr, size_t requiredMinimalSize, AllocationType allocationType, bool forceSystemMemoryFlag, bool *nonUsmHostPtrPartialOverlapFound)
        : requiredPtr(requiredPtr), requiredMinimalSize(requiredMinimalSize), allocationType(allocationType), forceSystemMemoryFlag(forceSystemMemoryFlag), nonUsmHostPtrPartialOverlapFound(nonUsmHostPtrPartialOverlapFound) {

        if (csr) {
            csrTagAddress = csr->getTagAddress();
            csrUcTagAddress = csr->getUcTagAddress();
            contextId = csr->getOsContext().getContextId();
            rootDeviceIndex = csr->getRootDeviceIndex();
            deviceBitfield = csr->getOsContext().getDeviceBitfield();
            tagOffset = csr->getImmWritePostSyncWriteOffset();
        }
    }

    const void *requiredPtr = nullptr;
    size_t requiredMinimalSize = 0;
    volatile TagAddressType *csrTagAddress = nullptr;
    volatile TagAddressType *csrUcTagAddress = nullptr;
    AllocationType allocationType = AllocationType::unknown;
    DeviceBitfield deviceBitfield = 1;
    uint32_t contextId = std::numeric_limits<uint32_t>::max();
    uint32_t rootDeviceIndex = 0;
    uint32_t tagOffset = 0;
    bool forceSystemMemoryFlag = false;
    bool *nonUsmHostPtrPartialOverlapFound = nullptr;
};

namespace {
bool checkTagAddressReady(const ReusableAllocationRequirements &requirements, GraphicsAllocation *gfxAllocation, volatile TagAddressType *tagAddress) {
    auto taskCount = gfxAllocation->getTaskCount(requirements.contextId);
    for (uint32_t count = 0; count < requirements.deviceBitfield.count(); count++) {
        if (*tagAddress < taskCount) {
            return false;
        }
        tagAddress = ptrOffset(tagAddress, requirements.tagOffset);
    }

    return true;
}

bool checkTagAddressReady(const ReusableAllocationRequirements &requirements, GraphicsAllocation *gfxAllocation) {
    if (gfxAllocation->isUsedByOsContext(requirements.contextId) == false) {
        return true;
    }
    if (requirements.allocationType == AllocationType::commandBuffer) {
        if (checkTagAddressReady(requirements, gfxAllocation, requirements.csrUcTagAddress)) {
            return true;
        }
    }
    return checkTagAddressReady(requirements, gfxAllocation, requirements.csrTagAddress);
}
} // namespace

AllocationsList::AllocationsList() = default;

AllocationsList::AllocationsList(AllocationUsage allocationUsage)
    : allocationUsage(allocationUsage) {}

AllocationsList::~AllocationsList() {
    std::lock_guard<std::mutex> lock(mutex);
    UNRECOVERABLE_IF(!allocations.empty());
}

void AllocationsList::pushTailOne(GraphicsAllocation &allocation) {
    std::lock_guard<std::mutex> lock(mutex);
    allocations.push_back(&allocation);
}

void AllocationsList::pushFrontOne(GraphicsAllocation &allocation) {
    std::lock_guard<std::mutex> lock(mutex);
    allocations.insert(allocations.begin(), &allocation);
}

std::unique_ptr<GraphicsAllocation> AllocationsList::removeOne(GraphicsAllocation &allocation) {
    std::lock_guard<std::mutex> lock(mutex);
    auto it = std::find(allocations.begin(), allocations.end(), &allocation);
    if (it == allocations.end()) {
        return nullptr;
    }
    allocations.erase(it);
    return std::unique_ptr<GraphicsAllocation>(&allocation);
}

GraphicsAllocation *AllocationsList::peekHead() {
    std::lock_guard<std::mutex> lock(mutex);
    if (allocations.empty()) {
        return nullptr;
    }
    return allocations.front();
}

GraphicsAllocation *AllocationsList::peekTail() {
    std::lock_guard<std::mutex> lock(mutex);
    if (allocations.empty()) {
        return nullptr;
    }
    return allocations.back();
}

bool AllocationsList::peekIsEmpty() {
    std::lock_guard<std::mutex> lock(mutex);
    return allocations.empty();
}

bool AllocationsList::peekContains(GraphicsAllocation &allocation) {
    std::lock_guard<std::mutex> lock(mutex);
    return std::find(allocations.begin(), allocations.end(), &allocation) != allocations.end();
}

void AllocationsList::transferAllAllocationsTo(AllocationsList &target) {
    if (this == &target) {
        return;
    }
    std::scoped_lock<std::mutex, std::mutex> lock(this->mutex, target.mutex);
    if (target.allocations.empty()) {
        target.allocations = std::move(allocations);
    } else {
        target.allocations.reserve(target.allocations.size() + allocations.size());
        target.allocations.insert(target.allocations.end(), allocations.begin(), allocations.end());
    }
    allocations.clear();
}

std::vector<GraphicsAllocation *> AllocationsList::peekAllocations() {
    std::lock_guard<std::mutex> lock(mutex);
    return allocations;
}

std::unique_ptr<GraphicsAllocation> AllocationsList::detachAllocation(size_t requiredMinimalSize, const void *requiredPtr, CommandStreamReceiver *commandStreamReceiver, AllocationType allocationType) {
    return this->detachAllocation(requiredMinimalSize, requiredPtr, false, commandStreamReceiver, allocationType);
}

std::unique_ptr<GraphicsAllocation> AllocationsList::detachAllocation(size_t requiredMinimalSize, const void *requiredPtr, CommandStreamReceiver *commandStreamReceiver, AllocationType allocationType, bool *nonUsmHostPtrPartialOverlapFound) {
    return this->detachAllocation(requiredMinimalSize, requiredPtr, false, commandStreamReceiver, allocationType, nonUsmHostPtrPartialOverlapFound);
}

std::unique_ptr<GraphicsAllocation> AllocationsList::detachAllocation(size_t requiredMinimalSize, const void *requiredPtr, bool forceSystemMemoryFlag, CommandStreamReceiver *commandStreamReceiver, AllocationType allocationType) {
    return this->detachAllocation(requiredMinimalSize, requiredPtr, forceSystemMemoryFlag, commandStreamReceiver, allocationType, nullptr);
}

std::unique_ptr<GraphicsAllocation> AllocationsList::detachAllocation(size_t requiredMinimalSize, const void *requiredPtr, bool forceSystemMemoryFlag, CommandStreamReceiver *commandStreamReceiver, AllocationType allocationType, bool *nonUsmHostPtrPartialOverlapFound) {
    ReusableAllocationRequirements requirements(commandStreamReceiver, requiredPtr, requiredMinimalSize, allocationType, forceSystemMemoryFlag, nonUsmHostPtrPartialOverlapFound);
    return detachMatchingAllocation(requirements);
}

std::unique_ptr<GraphicsAllocation> AllocationsList::detachMatchingAllocation(ReusableAllocationRequirements &requirements) {
    std::lock_guard<std::mutex> lock(mutex);
    for (auto it = allocations.begin(); it != allocations.end(); ++it) {
        auto *candidate = *it;
        bool typeMatch = (requirements.allocationType == candidate->getAllocationType());
        if (!typeMatch) {
            continue;
        }
        auto availableSize = candidate->getUnderlyingBufferSize();
        if (candidate->getAllocationType() == AllocationType::externalHostPtr) {
            availableSize = alignSizeWholePage(candidate->getUnderlyingBuffer(), candidate->getUnderlyingBufferSize()) - static_cast<size_t>(candidate->getAllocationOffset());
        }
        bool sizeMatch = (availableSize >= requirements.requiredMinimalSize);
        bool memMatch = (candidate->storageInfo.systemMemoryForced == requirements.forceSystemMemoryFlag);

        if (!memMatch) {
            continue;
        }
        if (sizeMatch) {
            if (requirements.csrTagAddress == nullptr) {
                allocations.erase(it);
                return std::unique_ptr<GraphicsAllocation>(candidate);
            }

            bool usageMatch = (this->allocationUsage == TEMPORARY_ALLOCATION || checkTagAddressReady(requirements, candidate));
            bool ptrMatch = (requirements.requiredPtr == nullptr || requirements.requiredPtr == candidate->getUnderlyingBuffer());
            bool tileMatch = (requirements.deviceBitfield == candidate->storageInfo.subDeviceBitfield) || (candidate->storageInfo.subDeviceBitfield == 0);
            bool placementMatch = (requirements.rootDeviceIndex == candidate->getRootDeviceIndex()) && tileMatch;

            if (usageMatch && ptrMatch && placementMatch) {
                if (this->allocationUsage == TEMPORARY_ALLOCATION) {
                    candidate->updateTaskCount(CompletionStamp::notReady, requirements.contextId);
                }
                allocations.erase(it);
                return std::unique_ptr<GraphicsAllocation>(candidate);
            }
        } else {
            bool detectPartialOverlap = (requirements.nonUsmHostPtrPartialOverlapFound != nullptr && candidate->getAllocationType() == AllocationType::externalHostPtr && this->allocationUsage == TEMPORARY_ALLOCATION);
            if (detectPartialOverlap) {
                auto importedStartPtr = candidate->getUnderlyingBuffer();
                auto importedEndPtr = ptrOffset(importedStartPtr, availableSize);
                auto requiredStartPtr = requirements.requiredPtr;
                auto requiredEndPtr = ptrOffset(requiredStartPtr, requirements.requiredMinimalSize);
                if (importedStartPtr <= requiredEndPtr && importedEndPtr >= requiredStartPtr) {
                    *requirements.nonUsmHostPtrPartialOverlapFound = true;
                }
            }
        }
    }
    return nullptr;
}

void AllocationsList::freeAllGraphicsAllocations(Device *neoDevice) {
    auto memoryManager = neoDevice->getMemoryManager();
    std::vector<GraphicsAllocation *> allocationsToFree;
    {
        std::lock_guard<std::mutex> lock(mutex);
        allocationsToFree.swap(allocations);
    }
    for (auto *allocation : allocationsToFree) {
        memoryManager->freeGraphicsMemory(allocation);
    }
}
} // namespace NEO
