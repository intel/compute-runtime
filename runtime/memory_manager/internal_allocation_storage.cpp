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
InternalAllocationStorage::InternalAllocationStorage(CommandStreamReceiver &commandStreamReceiver) : commandStreamReceiver(commandStreamReceiver){};
void InternalAllocationStorage::storeAllocation(std::unique_ptr<GraphicsAllocation> gfxAllocation, uint32_t allocationUsage) {
    uint32_t taskCount = gfxAllocation->taskCount;

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
    gfxAllocation->taskCount = taskCount;
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
        if (curr->taskCount <= waitTaskCount) {
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
    auto allocation = allocationsForReuse.detachAllocation(requiredSize, commandStreamReceiver.getTagAddress(), internalAllocation);
    return allocation;
}

} // namespace OCLRT
