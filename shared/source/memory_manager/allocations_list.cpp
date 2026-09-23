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
#include "shared/source/memory_manager/engine_completion_snapshot.h"
#include "shared/source/os_interface/os_context.h"

namespace {
struct ReusableAllocationRequirements {
    ReusableAllocationRequirements() = delete;
    ReusableAllocationRequirements(NEO::CommandStreamReceiver *csr, const void *requiredPtr, size_t requiredMinimalSize, NEO::AllocationType allocationType, bool forceSystemMemoryFlag, bool *nonUsmHostPtrPartialOverlapFound)
        : csr(csr), requiredPtr(requiredPtr), requiredMinimalSize(requiredMinimalSize), allocationType(allocationType), forceSystemMemoryFlag(forceSystemMemoryFlag), nonUsmHostPtrPartialOverlapFound(nonUsmHostPtrPartialOverlapFound) {

        if (csr) {
            csrTagAddress = csr->getTagAddress();
            csrUcTagAddress = csr->getUcTagAddress();
            contextId = csr->getOsContext().getContextId();
            rootDeviceIndex = csr->getRootDeviceIndex();
            deviceBitfield = csr->getOsContext().getDeviceBitfield();
            tagOffset = csr->getImmWritePostSyncWriteOffset();
        }
    }

    NEO::CommandStreamReceiver *csr = nullptr;
    NEO::Device *device = nullptr;
    const void *requiredPtr = nullptr;
    size_t requiredMinimalSize = 0;
    volatile TagAddressType *csrTagAddress = nullptr;
    volatile TagAddressType *csrUcTagAddress = nullptr;
    NEO::AllocationType allocationType = NEO::AllocationType::unknown;
    NEO::DeviceBitfield deviceBitfield = 1;
    uint32_t contextId = std::numeric_limits<uint32_t>::max();
    uint32_t rootDeviceIndex = 0;
    uint32_t tagOffset = 0;
    bool forceSystemMemoryFlag = false;
    bool *nonUsmHostPtrPartialOverlapFound = nullptr;
};

bool checkTagAddressReady(ReusableAllocationRequirements *requirements, NEO::GraphicsAllocation *gfxAllocation, volatile TagAddressType *tagAddress) {
    return NEO::TaskCountHelper::isReady(tagAddress, gfxAllocation->getTaskCount(requirements->contextId),
                                         requirements->deviceBitfield.count(), requirements->tagOffset);
}

bool checkTagAddressReady(ReusableAllocationRequirements *requirements, NEO::GraphicsAllocation *gfxAllocation) {
    if (gfxAllocation->isUsedByOsContext(requirements->contextId) == false) {
        return true;
    }
    if (requirements->allocationType == NEO::AllocationType::commandBuffer) {
        if (checkTagAddressReady(requirements, gfxAllocation, requirements->csrUcTagAddress)) {
            return true;
        }
    }
    return checkTagAddressReady(requirements, gfxAllocation, requirements->csrTagAddress);
}
} // namespace

namespace NEO {
AllocationsList::AllocationsList(AllocationUsage allocationUsage)
    : allocationUsage(allocationUsage) {}

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
    ReusableAllocationRequirements req(commandStreamReceiver, requiredPtr, requiredMinimalSize, allocationType, forceSystemMemoryFlag, nonUsmHostPtrPartialOverlapFound);

    return std::unique_ptr<GraphicsAllocation>(this->processLocked<AllocationsList, &AllocationsList::detachAllocationImpl>(nullptr, &req));
}

std::unique_ptr<GraphicsAllocation> AllocationsList::detachAllocation(size_t requiredMinimalSize, const void *requiredPtr, bool forceSystemMemoryFlag, CommandStreamReceiver *commandStreamReceiver, AllocationType allocationType, Device &device) {
    ReusableAllocationRequirements req(commandStreamReceiver, requiredPtr, requiredMinimalSize, allocationType, forceSystemMemoryFlag, nullptr);
    req.device = &device;
    if (!commandStreamReceiver) {
        req.rootDeviceIndex = device.getRootDeviceIndex();
        req.deviceBitfield = device.getDeviceBitfield();
    }
    return std::unique_ptr<GraphicsAllocation>(this->processLocked<AllocationsList, &AllocationsList::detachAllocationImpl>(nullptr, &req));
}

GraphicsAllocation *AllocationsList::detachAllocationImpl(GraphicsAllocation *, void *data) {
    auto *req = static_cast<ReusableAllocationRequirements *>(data);
    for (auto *curr = this->head; curr != nullptr; curr = curr->next) {
        const bool typeMatch = (req->allocationType == curr->getAllocationType());
        const bool sizeMatch = (curr->getUnderlyingBufferSize() >= req->requiredMinimalSize);
        const bool memMatch = (curr->storageInfo.systemMemoryForced == req->forceSystemMemoryFlag);

        if (typeMatch && memMatch) {
            if (sizeMatch) {
                const bool checkCommandBufferCompletion = req->device && req->allocationType == AllocationType::commandBuffer;
                if (!checkCommandBufferCompletion && req->csrTagAddress == nullptr) {
                    return this->removeOneImpl(curr, nullptr);
                }

                const bool ptrMatch = (req->requiredPtr == nullptr || req->requiredPtr == curr->getUnderlyingBuffer());
                const bool tileMatch = (req->deviceBitfield == curr->storageInfo.subDeviceBitfield) || (curr->storageInfo.subDeviceBitfield == 0);
                const bool placementMatch = (req->rootDeviceIndex == curr->getRootDeviceIndex()) && tileMatch;
                if (!ptrMatch || !placementMatch) {
                    continue;
                }

                if (checkCommandBufferCompletion) {
                    if (isCommandBufferReady(*curr, req->csr, *req->device->getMemoryManager())) {
                        return this->removeOneImpl(curr, nullptr);
                    }
                    continue;
                }

                const bool usageMatch = (this->allocationUsage == TEMPORARY_ALLOCATION || checkTagAddressReady(req, curr));
                if (usageMatch) {
                    if (this->allocationUsage == TEMPORARY_ALLOCATION) {
                        // We may not have proper task count yet, so set notReady to avoid releasing in a different thread
                        curr->updateTaskCount(CompletionStamp::notReady, req->contextId);
                    }
                    return this->removeOneImpl(curr, nullptr);
                }
            } else {
                const bool detectPartialOverlap = (req->nonUsmHostPtrPartialOverlapFound != nullptr && curr->getAllocationType() == NEO::AllocationType::externalHostPtr && this->allocationUsage == TEMPORARY_ALLOCATION);
                if (detectPartialOverlap) {
                    const auto importedStartPtr = curr->getUnderlyingBuffer();
                    const auto pageAlignedSize = alignSizeWholePage(importedStartPtr, curr->getUnderlyingBufferSize()) - static_cast<size_t>(curr->getAllocationOffset());
                    const auto importedEndPtr = ptrOffset(importedStartPtr, pageAlignedSize);
                    const auto requiredStartPtr = req->requiredPtr;
                    const auto requiredEndPtr = ptrOffset(requiredStartPtr, req->requiredMinimalSize);
                    if (importedStartPtr <= requiredEndPtr && importedEndPtr >= requiredStartPtr) {
                        *req->nonUsmHostPtrPartialOverlapFound = true;
                    }
                }
            }
        }
    }
    return nullptr;
}

void AllocationsList::freeAllGraphicsAllocations(Device *neoDevice) {
    auto *curr = head;
    while (curr != nullptr) {
        auto currNext = curr->next;
        neoDevice->getMemoryManager()->freeGraphicsMemory(curr);
        curr = currNext;
    }
    head = nullptr;
    tail = nullptr;
}
} // namespace NEO
