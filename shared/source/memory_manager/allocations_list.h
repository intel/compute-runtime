/*
 * Copyright (C) 2018-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/non_copyable_or_moveable.h"
#include "shared/source/memory_manager/graphics_allocation.h"
#include "shared/source/memory_manager/memory_manager.h"

#include <algorithm>
#include <memory>
#include <mutex>
#include <vector>

namespace NEO {
class CommandStreamReceiver;
struct ReusableAllocationRequirements;

class AllocationsList : NEO::NonCopyableAndNonMovableClass {
  public:
    AllocationsList();
    AllocationsList(AllocationUsage allocationUsage);
    ~AllocationsList();
    AllocationsList(const AllocationsList &) = delete;
    AllocationsList &operator=(const AllocationsList &) = delete;

    void pushTailOne(GraphicsAllocation &allocation);
    void pushFrontOne(GraphicsAllocation &allocation);
    std::unique_ptr<GraphicsAllocation> removeOne(GraphicsAllocation &allocation);
    GraphicsAllocation *peekHead();
    GraphicsAllocation *peekTail();
    bool peekIsEmpty();
    bool peekContains(GraphicsAllocation &allocation);
    std::vector<GraphicsAllocation *> peekAllocations();
    void transferAllAllocationsTo(AllocationsList &target);

    template <typename ShouldRemove, typename OnRemove>
    void removeMatching(ShouldRemove shouldRemove, OnRemove onRemove) {
        std::vector<GraphicsAllocation *> detachedAllocations;
        {
            std::lock_guard<std::mutex> lock(mutex);
            detachedAllocations.swap(allocations);
        }
        std::vector<GraphicsAllocation *> survivingAllocations;
        for (auto *allocation : detachedAllocations) {
            if (shouldRemove(allocation)) {
                onRemove(allocation);
            } else {
                survivingAllocations.push_back(allocation);
            }
        }
        if (!survivingAllocations.empty()) {
            std::lock_guard<std::mutex> lock(mutex);
            allocations.insert(allocations.end(), survivingAllocations.begin(), survivingAllocations.end());
        }
    }

    std::unique_ptr<GraphicsAllocation> detachAllocation(size_t requiredMinimalSize, const void *requiredPtr, CommandStreamReceiver *commandStreamReceiver, AllocationType allocationType);
    std::unique_ptr<GraphicsAllocation> detachAllocation(size_t requiredMinimalSize, const void *requiredPtr, CommandStreamReceiver *commandStreamReceiver, AllocationType allocationType, bool *nonUsmHostPtrPartialOverlapFound);
    std::unique_ptr<GraphicsAllocation> detachAllocation(size_t requiredMinimalSize, const void *requiredPtr, bool forceSystemMemoryFlag, CommandStreamReceiver *commandStreamReceiver, AllocationType allocationType);
    std::unique_ptr<GraphicsAllocation> detachAllocation(size_t requiredMinimalSize, const void *requiredPtr, bool forceSystemMemoryFlag, CommandStreamReceiver *commandStreamReceiver, AllocationType allocationType, bool *nonUsmHostPtrPartialOverlapFound);
    void freeAllGraphicsAllocations(Device *neoDevice);

  private:
    std::unique_ptr<GraphicsAllocation> detachMatchingAllocation(ReusableAllocationRequirements &requirements);

    std::vector<GraphicsAllocation *> allocations;
    std::mutex mutex;
    const AllocationUsage allocationUsage{REUSABLE_ALLOCATION};
};
} // namespace NEO
