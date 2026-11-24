/*
 * Copyright (C) 2018-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/non_copyable_or_moveable.h"
#include "shared/source/memory_manager/allocation_usage.h"
#include "shared/source/memory_manager/graphics_allocation.h"

#include <memory>
#include <mutex>
#include <vector>

namespace NEO {
class CommandStreamReceiver;
class Device;

class AllocationsList : NEO::NonCopyableAndNonMovableClass {
  public:
    AllocationsList() = default;
    AllocationsList(AllocationUsage allocationUsage);

    void pushTailOne(GraphicsAllocation &allocation);
    void spliceFrom(AllocationsList &otherList);
    bool peekIsEmpty();

    template <typename ShouldFreeFuncT, typename FreeFuncT>
    void freeOrKeepAllocations(const ShouldFreeFuncT &shouldFreeAllocation, const FreeFuncT &freeAllocation);

    std::unique_ptr<GraphicsAllocation> detachAllocation(size_t requiredMinimalSize, const void *requiredPtr, CommandStreamReceiver *commandStreamReceiver, AllocationType allocationType);
    std::unique_ptr<GraphicsAllocation> detachAllocation(size_t requiredMinimalSize, const void *requiredPtr, CommandStreamReceiver *commandStreamReceiver, AllocationType allocationType, bool *nonUsmHostPtrPartialOverlapFound);
    std::unique_ptr<GraphicsAllocation> detachAllocation(size_t requiredMinimalSize, const void *requiredPtr, bool forceSystemMemoryFlag, CommandStreamReceiver *commandStreamReceiver, AllocationType allocationType);
    std::unique_ptr<GraphicsAllocation> detachAllocation(size_t requiredMinimalSize, const void *requiredPtr, bool forceSystemMemoryFlag, CommandStreamReceiver *commandStreamReceiver, AllocationType allocationType, bool *nonUsmHostPtrPartialOverlapFound);
    std::unique_ptr<GraphicsAllocation> detachAllocation(size_t requiredMinimalSize, const void *requiredPtr, bool forceSystemMemoryFlag, CommandStreamReceiver *commandStreamReceiver, AllocationType allocationType, Device &device);
    void freeAllGraphicsAllocations(Device *neoDevice);

  protected:
    using AllocationsContainer = std::vector<std::unique_ptr<GraphicsAllocation>>;

    AllocationsContainer detachAll();
    void appendAll(AllocationsContainer &&otherAllocations);

    AllocationsContainer allocations;
    std::mutex allocationsMutex;
    const AllocationUsage allocationUsage{REUSABLE_ALLOCATION};
};

template <typename ShouldFreeFuncT, typename FreeFuncT>
void AllocationsList::freeOrKeepAllocations(const ShouldFreeFuncT &shouldFreeAllocation, const FreeFuncT &freeAllocation) {
    auto detachedAllocations = this->detachAll();

    AllocationsContainer allocationsLeft;
    for (auto &allocation : detachedAllocations) {
        if (shouldFreeAllocation(*allocation)) {
            freeAllocation(allocation.release());
        } else {
            allocationsLeft.push_back(std::move(allocation));
        }
    }

    this->appendAll(std::move(allocationsLeft));
}
} // namespace NEO
