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
    MockWddmAllocation() : WddmAllocation(GraphicsAllocation::AllocationType::UNDECIDED, nullptr, 0, nullptr, MemoryPool::MemoryNull, false), gpuPtr(gpuAddress) {
    }
    using WddmAllocation::memoryPool;

    D3DGPU_VIRTUAL_ADDRESS &gpuPtr;
};

} // namespace OCLRT
