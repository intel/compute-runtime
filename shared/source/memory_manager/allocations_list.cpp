/*
 * Copyright (C) 2021-2023 Intel Corporation
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
    const void *requiredPtr;
    size_t requiredMinimalSize;
    volatile TagAddressType *csrTagAddress;
    NEO::AllocationType allocationType;
    uint32_t contextId;
    uint32_t activeTileCount;
    uint32_t tagOffset;
    bool forceSystemMemoryFlag;
};

bool checkTagAddressReady(ReusableAllocationRequirements *requirements, NEO::GraphicsAllocation *gfxAllocation) {
    auto tagAddress = requirements->csrTagAddress;
    auto taskCount = gfxAllocation->getTaskCount(requirements->contextId);
    for (uint32_t count = 0; count < requirements->activeTileCount; count++) {
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
    ReusableAllocationRequirements req;
    req.requiredMinimalSize = requiredMinimalSize;
    req.csrTagAddress = (commandStreamReceiver == nullptr) ? nullptr : commandStreamReceiver->getTagAddress();
    req.allocationType = allocationType;
    req.contextId = (commandStreamReceiver == nullptr) ? UINT32_MAX : commandStreamReceiver->getOsContext().getContextId();
    req.requiredPtr = requiredPtr;
    req.activeTileCount = (commandStreamReceiver == nullptr) ? 1u : commandStreamReceiver->getActivePartitions();
    req.tagOffset = (commandStreamReceiver == nullptr) ? 0u : commandStreamReceiver->getImmWritePostSyncWriteOffset();
    req.forceSystemMemoryFlag = forceSystemMemoryFlag;
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
            if ((this->allocationUsage == TEMPORARY_ALLOCATION || checkTagAddressReady(req, curr)) &&
                (req->requiredPtr == nullptr || req->requiredPtr == curr->getUnderlyingBuffer())) {
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
