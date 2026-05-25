/*
 * Copyright (C) 2018-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/memory_manager/internal_allocation_storage.h"

#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/memory_manager/host_ptr_manager.h"
#include "shared/source/os_interface/os_context.h"

namespace NEO {

InternalAllocationStorage::InternalAllocationStorage(CommandStreamReceiver &commandStreamReceiver)
    : commandStreamReceiver(commandStreamReceiver) {};

void InternalAllocationStorage::storeAllocation(std::unique_ptr<GraphicsAllocation> &&gfxAllocation, uint32_t allocationUsage) {
    TaskCountType taskCount = gfxAllocation->getTaskCount(commandStreamReceiver.getOsContext().getContextId());

    if (allocationUsage == REUSABLE_ALLOCATION) {
        taskCount = commandStreamReceiver.peekTaskCount();
    }

    storeAllocationWithTaskCount(std::move(gfxAllocation), allocationUsage, taskCount);
}
void InternalAllocationStorage::storeAllocationWithTaskCount(std::unique_ptr<GraphicsAllocation> &&gfxAllocation, uint32_t allocationUsage, TaskCountType taskCount) {
    auto memoryManager = commandStreamReceiver.getMemoryManager();
    auto osContextId = commandStreamReceiver.getOsContext().getContextId();

    if (allocationUsage == TEMPORARY_ALLOCATION) {
        memoryManager->storeTemporaryAllocation(std::move(gfxAllocation), osContextId, taskCount);
        return;
    }

    if (allocationUsage == REUSABLE_ALLOCATION) {
        if (debugManager.flags.DisableResourceRecycling.get()) {
            memoryManager->freeGraphicsMemory(gfxAllocation.release());
            return;
        }
    }
    auto &allocationsList = allocationLists[allocationUsage];
    gfxAllocation->updateTaskCount(taskCount, osContextId);
    allocationsList.pushTailOne(*gfxAllocation.release());
}

void InternalAllocationStorage::cleanAllocationList(TaskCountType waitTaskCount, uint32_t allocationUsage) {
    freeAllocationsList(waitTaskCount, allocationLists[allocationUsage]);
}

void InternalAllocationStorage::freeAllocationsList(TaskCountType waitTaskCount, AllocationsList &allocationsList) {
    auto memoryManager = commandStreamReceiver.getMemoryManager();

    if (&allocationsList == &allocationLists[TEMPORARY_ALLOCATION]) {
        memoryManager->cleanTemporaryAllocations(commandStreamReceiver, waitTaskCount);
        return;
    }

    auto lock = memoryManager->getHostPtrManager()->obtainOwnership();

    if (allocationsList.peekIsEmpty()) {
        return;
    }

    const auto contextId = commandStreamReceiver.getOsContext().getContextId();
    allocationsList.removeMatching(
        [&](GraphicsAllocation *currentAlloc) {
            return currentAlloc->getHostPtrTaskCountAssignment() == 0 &&
                   currentAlloc->getTaskCount(contextId) <= waitTaskCount;
        },
        [&](GraphicsAllocation *currentAlloc) {
            memoryManager->freeGraphicsMemory(currentAlloc);
        });
}

std::unique_ptr<GraphicsAllocation> InternalAllocationStorage::obtainReusableAllocation(size_t requiredSize, AllocationType allocationType) {
    auto allocation = allocationLists[REUSABLE_ALLOCATION].detachAllocation(requiredSize, nullptr, &commandStreamReceiver, allocationType);
    return allocation;
}

std::unique_ptr<GraphicsAllocation> InternalAllocationStorage::obtainTemporaryAllocationWithPtr(size_t requiredSize, const void *requiredPtr, AllocationType allocationType) {
    return obtainTemporaryAllocationWithPtr(requiredSize, requiredPtr, allocationType, nullptr);
}

std::unique_ptr<GraphicsAllocation> InternalAllocationStorage::obtainTemporaryAllocationWithPtr(size_t requiredSize, const void *requiredPtr, AllocationType allocationType, bool *nonUsmHostPtrPartialOverlapFound) {
    auto memoryManager = commandStreamReceiver.getMemoryManager();
    return memoryManager->obtainTemporaryAllocationWithPtr(&commandStreamReceiver, requiredSize, requiredPtr, allocationType, nonUsmHostPtrPartialOverlapFound);
}

AllocationsList &InternalAllocationStorage::getTemporaryAllocations() {
    auto memoryManager = commandStreamReceiver.getMemoryManager();
    return memoryManager->getTemporaryAllocationsList();
}

DeviceBitfield InternalAllocationStorage::getDeviceBitfield() const {
    return commandStreamReceiver.getOsContext().getDeviceBitfield();
}

} // namespace NEO
