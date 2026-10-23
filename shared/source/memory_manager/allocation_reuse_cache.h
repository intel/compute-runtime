/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <chrono>

namespace NEO {
class MemoryManager;

class AllocationReuseCache {
  public:
    virtual ~AllocationReuseCache() = default;

    virtual bool isEmpty() = 0;
    virtual void trimOldAllocs(std::chrono::high_resolution_clock::time_point trimTimePoint, bool trimAll) = 0;
    virtual MemoryManager *getMemoryManager() const = 0;
};

} // namespace NEO
