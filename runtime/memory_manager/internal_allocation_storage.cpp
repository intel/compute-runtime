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
    std::lock_guard<decltype(mutex)> lock(mutex);

    uint32_t taskCount = gfxAllocation->taskCount;

    if (allocationUsage == REUSABLE_ALLOCATION) {
        taskCount = commandStreamReceiver.peekTaskCount();
    }

    storeAllocationWithTaskCount(std::move(gfxAllocation), allocationUsage, taskCount);
}
void InternalAllocationStorage::storeAllocationWithTaskCount(std::unique_ptr<GraphicsAllocation> gfxAllocation, uint32_t allocationUsage, uint32_t taskCount) {
    std::lock_guard<decltype(mutex)> lock(mutex);
    if (DebugManager.flags.DisableResourceRecycling.get()) {
        if (allocationUsage == REUSABLE_ALLOCATION) {
            commandStreamReceiver.getMemoryManager()->freeGraphicsMemory(gfxAllocation.release());
            return;
        }
    }
    auto &allocationsList = (allocationUsage == TEMPORARY_ALLOCATION) ? commandStreamReceiver.getTemporaryAllocations() : commandStreamReceiver.getAllocationsForReuse();
    gfxAllocation->taskCount = taskCount;
    allocationsList.pushTailOne(*gfxAllocation.release());
}

void InternalAllocationStorage::cleanAllocationsList(uint32_t waitTaskCount, uint32_t allocationUsage) {
    std::lock_guard<decltype(mutex)> lock(mutex);
    freeAllocationsList(waitTaskCount, (allocationUsage == TEMPORARY_ALLOCATION) ? commandStreamReceiver.getTemporaryAllocations() : commandStreamReceiver.getAllocationsForReuse());
}

void InternalAllocationStorage::freeAllocationsList(uint32_t waitTaskCount, AllocationsList &allocationsList) {
    std::lock_guard<decltype(mutex)> lock(mutex);
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
    std::lock_guard<decltype(mutex)> lock(mutex);
    auto allocation = commandStreamReceiver.getAllocationsForReuse().detachAllocation(requiredSize, commandStreamReceiver.getTagAddress(), internalAllocation);
    return allocation;
}

} // namespace OCLRT
