/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include <cstdint>
#include <mutex>

namespace NEO {
struct UsmReuseInfo {
    uint64_t getAllocationsSavedForReuseSize() const {
        return allocationsSavedForReuseSize;
    }

    uint64_t getMaxAllocationsSavedForReuseSize() const {
        return maxAllocationsSavedForReuseSize;
    }

    uint64_t getLimitAllocationsReuseThreshold() const {
        return limitAllocationsReuseThreshold;
    }

    std::unique_lock<std::mutex> obtainAllocationsReuseLock() const {
        return std::unique_lock<std::mutex>(allocationsReuseMtx);
    }

    void recordAllocationSaveForReuse(size_t size) {
        allocationsSavedForReuseSize += size;
    }

    void recordAllocationGetFromReuse(size_t size) {
        allocationsSavedForReuseSize -= size;
    }

    void init(uint64_t maxAllocationsSavedForReuseSize, uint64_t limitAllocationsReuseThreshold) {
        this->maxAllocationsSavedForReuseSize = maxAllocationsSavedForReuseSize;
        this->limitAllocationsReuseThreshold = limitAllocationsReuseThreshold;
    }

    static constexpr uint64_t notLimited = std::numeric_limits<uint64_t>::max();

  protected:
    uint64_t maxAllocationsSavedForReuseSize = 0u;
    uint64_t limitAllocationsReuseThreshold = 0u;
    uint64_t allocationsSavedForReuseSize = 0u;
    mutable std::mutex allocationsReuseMtx;
};

} // namespace NEO
