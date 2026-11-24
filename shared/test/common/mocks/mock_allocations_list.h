/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/memory_manager/allocations_list.h"

#include <algorithm>

namespace NEO {
class MockAllocationsList : public AllocationsList {
  public:
    using AllocationsList::allocations;
    using AllocationsList::AllocationsList;

    static MockAllocationsList &whiteboxCast(AllocationsList &list) {
        return static_cast<MockAllocationsList &>(list);
    }

    size_t peekSize() {
        std::lock_guard<std::mutex> lock(this->allocationsMutex);
        return this->allocations.size();
    }

    GraphicsAllocation *peekAt(size_t index) {
        std::lock_guard<std::mutex> lock(this->allocationsMutex);
        return index < this->allocations.size() ? this->allocations[index].get() : nullptr;
    }

    GraphicsAllocation *peekFront() {
        std::lock_guard<std::mutex> lock(this->allocationsMutex);
        return this->allocations.empty() ? nullptr : this->allocations.front().get();
    }

    GraphicsAllocation *peekBack() {
        std::lock_guard<std::mutex> lock(this->allocationsMutex);
        return this->allocations.empty() ? nullptr : this->allocations.back().get();
    }

    bool peekContains(const GraphicsAllocation &allocation) {
        std::lock_guard<std::mutex> lock(this->allocationsMutex);
        return findAllocation(allocation) != this->allocations.end();
    }

    std::unique_ptr<GraphicsAllocation> removeOne(GraphicsAllocation &allocation) {
        std::lock_guard<std::mutex> lock(this->allocationsMutex);
        auto position = findAllocation(allocation);
        if (position == this->allocations.end()) {
            return nullptr;
        }
        auto removedAllocation = std::move(*position);
        this->allocations.erase(position);
        return removedAllocation;
    }

  protected:
    AllocationsContainer::iterator findAllocation(const GraphicsAllocation &allocation) {
        return std::find_if(this->allocations.begin(), this->allocations.end(), [&allocation](const auto &storedAllocation) { return storedAllocation.get() == &allocation; });
    }
};
} // namespace NEO
