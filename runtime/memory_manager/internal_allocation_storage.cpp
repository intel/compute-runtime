/*
 * Copyright (C) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/command_stream/command_stream_receiver.h"
#include "runtime/memory_manager/internal_allocation_storage.h"
#include "runtime/memory_manager/memory_manager.h"

namespace OCLRT {
InternalAllocationStorage::InternalAllocationStorage(CommandStreamReceiver &commandStreamReceiver) : commandStreamReceiver(commandStreamReceiver), contextId(commandStreamReceiver.getDeviceIndex()){};
void InternalAllocationStorage::storeAllocation(std::unique_ptr<GraphicsAllocation> gfxAllocation, uint32_t allocationUsage) {
    uint32_t taskCount = gfxAllocation->getTaskCount(contextId);

    if (allocationUsage == REUSABLE_ALLOCATION) {
        taskCount = commandStreamReceiver.peekTaskCount();
    }

    storeAllocationWithTaskCount(std::move(gfxAllocation), allocationUsage, taskCount);
}
void InternalAllocationStorage::storeAllocationWithTaskCount(std::unique_ptr<GraphicsAllocation> gfxAllocation, uint32_t allocationUsage, uint32_t taskCount) {
    if (allocationUsage == REUSABLE_ALLOCATION) {
        if (DebugManager.flags.DisableResourceRecycling.get()) {
            commandStreamReceiver.getMemoryManager()->freeGraphicsMemory(gfxAllocation.release());
            return;
        }
    }
    auto &allocationsList = (allocationUsage == TEMPORARY_ALLOCATION) ? temporaryAllocations : allocationsForReuse;
    gfxAllocation->updateTaskCount(taskCount, contextId);
    allocationsList.pushTailOne(*gfxAllocation.release());
}

void InternalAllocationStorage::cleanAllocationList(uint32_t waitTaskCount, uint32_t allocationUsage) {
    freeAllocationsList(waitTaskCount, (allocationUsage == TEMPORARY_ALLOCATION) ? temporaryAllocations : allocationsForReuse);
}

void InternalAllocationStorage::freeAllocationsList(uint32_t waitTaskCount, AllocationsList &allocationsList) {
    auto memoryManager = commandStreamReceiver.getMemoryManager();
    GraphicsAllocation *curr = allocationsList.detachNodes();

    IDList<GraphicsAllocation, false, true> allocationsLeft;
    while (curr != nullptr) {
        auto *next = curr->next;
        if (curr->getTaskCount(contextId) <= waitTaskCount) {
            memoryManager->freeGraphicsMemory(curr);
        } else {
            allocationsLeft.pushTailOne(*curr);
        }
        curr = next;
    }

    if (allocationsLeft.peekIsEmpty() == false) {
        allocationsList.splice(*allocationsLeft.detachNodes());
    }
}

std::unique_ptr<GraphicsAllocation> InternalAllocationStorage::obtainReusableAllocation(size_t requiredSize, bool internalAllocation) {
    auto allocation = allocationsForReuse.detachAllocation(requiredSize, commandStreamReceiver, internalAllocation);
    return allocation;
}

struct ReusableAllocationRequirements {
    size_t requiredMinimalSize;
    volatile uint32_t *csrTagAddress;
    bool internalAllocationRequired;
    uint32_t contextId;
};

std::unique_ptr<GraphicsAllocation> AllocationsList::detachAllocation(size_t requiredMinimalSize, CommandStreamReceiver &commandStreamReceiver, bool internalAllocationRequired) {
    ReusableAllocationRequirements req;
    req.requiredMinimalSize = requiredMinimalSize;
    req.csrTagAddress = commandStreamReceiver.getTagAddress();
    req.internalAllocationRequired = internalAllocationRequired;
    req.contextId = commandStreamReceiver.getDeviceIndex();
    GraphicsAllocation *a = nullptr;
    GraphicsAllocation *retAlloc = processLocked<AllocationsList, &AllocationsList::detachAllocationImpl>(a, static_cast<void *>(&req));
    return std::unique_ptr<GraphicsAllocation>(retAlloc);
}

GraphicsAllocation *AllocationsList::detachAllocationImpl(GraphicsAllocation *, void *data) {
    ReusableAllocationRequirements *req = static_cast<ReusableAllocationRequirements *>(data);
    auto *curr = head;
    while (curr != nullptr) {
        auto currentTagValue = *req->csrTagAddress;
        if ((req->internalAllocationRequired == curr->is32BitAllocation) &&
            (curr->getUnderlyingBufferSize() >= req->requiredMinimalSize) &&
            (currentTagValue >= curr->getTaskCount(req->contextId))) {
            return removeOneImpl(curr, nullptr);
        }
        curr = curr->next;
    }
    return nullptr;
}

} // namespace OCLRT
