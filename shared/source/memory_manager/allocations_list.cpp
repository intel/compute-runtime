/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/memory_manager/allocations_list.h"

#include "shared/source/command_stream/command_stream_receiver.h"

namespace NEO {

struct ReusableAllocationRequirements {
    size_t requiredMinimalSize;
    volatile uint32_t *csrTagAddress;
    GraphicsAllocation::AllocationType allocationType;
    uint32_t contextId;
    const void *requiredPtr;
};

AllocationsList::AllocationsList(AllocationUsage allocationUsage)
    : allocationUsage(allocationUsage) {}

AllocationsList::AllocationsList()
    : allocationUsage(REUSABLE_ALLOCATION) {}

std::unique_ptr<GraphicsAllocation> AllocationsList::detachAllocation(size_t requiredMinimalSize, const void *requiredPtr, CommandStreamReceiver *commandStreamReceiver, GraphicsAllocation::AllocationType allocationType) {
    ReusableAllocationRequirements req;
    req.requiredMinimalSize = requiredMinimalSize;
    commandStreamReceiver == nullptr ? req.csrTagAddress = nullptr : req.csrTagAddress = commandStreamReceiver->getTagAddress();
    req.allocationType = allocationType;
    commandStreamReceiver == nullptr ? req.contextId = UINT32_MAX : req.contextId = commandStreamReceiver->getOsContext().getContextId();
    req.requiredPtr = requiredPtr;
    GraphicsAllocation *a = nullptr;
    GraphicsAllocation *retAlloc = processLocked<AllocationsList, &AllocationsList::detachAllocationImpl>(a, static_cast<void *>(&req));
    return std::unique_ptr<GraphicsAllocation>(retAlloc);
}

GraphicsAllocation *AllocationsList::detachAllocationImpl(GraphicsAllocation *, void *data) {
    ReusableAllocationRequirements *req = static_cast<ReusableAllocationRequirements *>(data);
    auto *curr = head;
    while (curr != nullptr) {
        if ((req->allocationType == curr->getAllocationType()) &&
            (curr->getUnderlyingBufferSize() >= req->requiredMinimalSize)) {
            if (req->csrTagAddress == nullptr) {
                return removeOneImpl(curr, nullptr);
            }
            if ((this->allocationUsage == TEMPORARY_ALLOCATION || *req->csrTagAddress >= curr->getTaskCount(req->contextId)) &&
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
}
} // namespace NEO