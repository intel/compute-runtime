/*
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/memory_manager/allocations_list.h"

#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/command_stream/task_count_helper.h"
#include "shared/source/device/device.h"
#include "shared/source/os_interface/os_context.h"

namespace {
struct ReusableAllocationRequirements {
    ReusableAllocationRequirements() = delete;
    ReusableAllocationRequirements(NEO::CommandStreamReceiver *csr, const void *requiredPtr, size_t requiredMinimalSize, NEO::AllocationType allocationType, bool forceSystemMemoryFlag)
        : requiredPtr(requiredPtr), requiredMinimalSize(requiredMinimalSize), allocationType(allocationType), forceSystemMemoryFlag(forceSystemMemoryFlag) {

        if (csr) {
            csrTagAddress = csr->getTagAddress();
            contextId = csr->getOsContext().getContextId();
            rootDeviceIndex = csr->getRootDeviceIndex();
            deviceBitfield = csr->getOsContext().getDeviceBitfield();
            tagOffset = csr->getImmWritePostSyncWriteOffset();
        }
    }

    const void *requiredPtr = nullptr;
    size_t requiredMinimalSize = 0;
    volatile TagAddressType *csrTagAddress = nullptr;
    NEO::AllocationType allocationType = NEO::AllocationType::unknown;
    NEO::DeviceBitfield deviceBitfield = 1;
    uint32_t contextId = std::numeric_limits<uint32_t>::max();
    uint32_t rootDeviceIndex = 0;
    uint32_t tagOffset = 0;
    bool forceSystemMemoryFlag = false;
};

bool checkTagAddressReady(ReusableAllocationRequirements *requirements, NEO::GraphicsAllocation *gfxAllocation) {
    auto tagAddress = requirements->csrTagAddress;
    auto taskCount = gfxAllocation->getTaskCount(requirements->contextId);
    for (uint32_t count = 0; count < requirements->deviceBitfield.count(); count++) {
        if (*tagAddress < taskCount) {
            return false;
        }
        tagAddress = ptrOffset(tagAddress, requirements->tagOffset);
    }

    return true;
}
} // namespace

namespace NEO {
AllocationsList::AllocationsList(AllocationUsage allocationUsage)
    : allocationUsage(allocationUsage) {}

std::unique_ptr<GraphicsAllocation> AllocationsList::detachAllocation(size_t requiredMinimalSize, const void *requiredPtr, CommandStreamReceiver *commandStreamReceiver, AllocationType allocationType) {
    return this->detachAllocation(requiredMinimalSize, requiredPtr, false, commandStreamReceiver, allocationType);
}

std::unique_ptr<GraphicsAllocation> AllocationsList::detachAllocation(size_t requiredMinimalSize, const void *requiredPtr, bool forceSystemMemoryFlag, CommandStreamReceiver *commandStreamReceiver, AllocationType allocationType) {
    ReusableAllocationRequirements req(commandStreamReceiver, requiredPtr, requiredMinimalSize, allocationType, forceSystemMemoryFlag);

    GraphicsAllocation *a = nullptr;
    GraphicsAllocation *retAlloc = processLocked<AllocationsList, &AllocationsList::detachAllocationImpl>(a, static_cast<void *>(&req));
    return std::unique_ptr<GraphicsAllocation>(retAlloc);
}

GraphicsAllocation *AllocationsList::detachAllocationImpl(GraphicsAllocation *, void *data) {
    ReusableAllocationRequirements *req = static_cast<ReusableAllocationRequirements *>(data);
    auto *curr = head;
    while (curr != nullptr) {
        if ((req->allocationType == curr->getAllocationType()) &&
            (curr->getUnderlyingBufferSize() >= req->requiredMinimalSize) &&
            (curr->storageInfo.systemMemoryForced == req->forceSystemMemoryFlag)) {
            if (req->csrTagAddress == nullptr) {
                return removeOneImpl(curr, nullptr);
            }

            bool usageMatch = (this->allocationUsage == TEMPORARY_ALLOCATION || checkTagAddressReady(req, curr));
            bool ptrMatch = (req->requiredPtr == nullptr || req->requiredPtr == curr->getUnderlyingBuffer());
            bool tileMatch = (req->deviceBitfield == curr->storageInfo.subDeviceBitfield) || (curr->storageInfo.subDeviceBitfield == 0);
            bool placementMatch = (req->rootDeviceIndex == curr->getRootDeviceIndex()) && tileMatch;

            if (usageMatch && ptrMatch && placementMatch) {
                if (this->allocationUsage == TEMPORARY_ALLOCATION) {
                    // We may not have proper task count yet, so set notReady to avoid releasing in a different thread
                    curr->updateTaskCount(CompletionStamp::notReady, req->contextId);
                }
                return removeOneImpl(curr, nullptr);
            }
        }
        curr = curr->next;
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
