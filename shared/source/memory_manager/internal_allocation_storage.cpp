/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/memory_manager/internal_allocation_storage.h"

#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/memory_manager/host_ptr_manager.h"
#include "shared/source/os_interface/os_context.h"

namespace NEO {

InternalAllocationStorage::InternalAllocationStorage(CommandStreamReceiver &commandStreamReceiver)
    : commandStreamReceiver(commandStreamReceiver),
      temporaryAllocations(TEMPORARY_ALLOCATION),
      allocationsForReuse(REUSABLE_ALLOCATION){};

void InternalAllocationStorage::storeAllocation(std::unique_ptr<GraphicsAllocation> &&gfxAllocation, uint32_t allocationUsage) {
    uint32_t taskCount = gfxAllocation->getTaskCount(commandStreamReceiver.getOsContext().getContextId());

    if (allocationUsage == REUSABLE_ALLOCATION) {
        taskCount = commandStreamReceiver.peekTaskCount();
    }

    storeAllocationWithTaskCount(std::move(gfxAllocation), allocationUsage, taskCount);
}
void InternalAllocationStorage::storeAllocationWithTaskCount(std::unique_ptr<GraphicsAllocation> &&gfxAllocation, uint32_t allocationUsage, uint32_t taskCount) {
    if (allocationUsage == REUSABLE_ALLOCATION) {
        if (DebugManager.flags.DisableResourceRecycling.get()) {
            commandStreamReceiver.getMemoryManager()->freeGraphicsMemory(gfxAllocation.release());
            return;
        }
    }
    auto &allocationsList = (allocationUsage == TEMPORARY_ALLOCATION) ? temporaryAllocations : allocationsForReuse;
    gfxAllocation->updateTaskCount(taskCount, commandStreamReceiver.getOsContext().getContextId());
    allocationsList.pushTailOne(*gfxAllocation.release());
}

void InternalAllocationStorage::cleanAllocationList(uint32_t waitTaskCount, uint32_t allocationUsage) {
    freeAllocationsList(waitTaskCount, (allocationUsage == TEMPORARY_ALLOCATION) ? temporaryAllocations : allocationsForReuse);
}

void InternalAllocationStorage::freeAllocationsList(uint32_t waitTaskCount, AllocationsList &allocationsList) {
    auto memoryManager = commandStreamReceiver.getMemoryManager();
    auto lock = memoryManager->getHostPtrManager()->obtainOwnership();

    GraphicsAllocation *curr = allocationsList.detachNodes();

    IDList<GraphicsAllocation, false, true> allocationsLeft;
    while (curr != nullptr) {
        auto *next = curr->next;
        if (curr->getTaskCount(commandStreamReceiver.getOsContext().getContextId()) <= waitTaskCount) {
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

std::unique_ptr<GraphicsAllocation> InternalAllocationStorage::obtainReusableAllocation(size_t requiredSize, GraphicsAllocation::AllocationType allocationType) {
    auto allocation = allocationsForReuse.detachAllocation(requiredSize, nullptr, &commandStreamReceiver, allocationType);
    return allocation;
}

std::unique_ptr<GraphicsAllocation> InternalAllocationStorage::obtainTemporaryAllocationWithPtr(size_t requiredSize, const void *requiredPtr, GraphicsAllocation::AllocationType allocationType) {
    auto allocation = temporaryAllocations.detachAllocation(requiredSize, requiredPtr, &commandStreamReceiver, allocationType);
    return allocation;
}

DeviceBitfield InternalAllocationStorage::getDeviceBitfield() const {
    return commandStreamReceiver.getOsContext().getDeviceBitfield();
}

} // namespace NEO
