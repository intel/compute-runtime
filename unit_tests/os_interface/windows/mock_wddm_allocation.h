/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "runtime/os_interface/windows/wddm_allocation.h"

namespace OCLRT {

class MockWddmAllocation : public WddmAllocation {
  public:
    MockWddmAllocation() : WddmAllocation(nullptr, 0, nullptr, MemoryPool::MemoryNull, 1u, false) {
    }

    void setMemoryPool(MemoryPool::Type pool) {
        memoryPool = pool;
    }
};

} // namespace OCLRT
