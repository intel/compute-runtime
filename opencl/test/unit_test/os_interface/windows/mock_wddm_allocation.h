/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/os_interface/windows/wddm_allocation.h"

#include "opencl/test/unit_test/mock_gdi/mock_gdi.h"

namespace NEO {

class MockWddmAllocation : public WddmAllocation {
  public:
    MockWddmAllocation() : WddmAllocation(0, GraphicsAllocation::AllocationType::UNKNOWN, nullptr, 0, nullptr, MemoryPool::MemoryNull), gpuPtr(gpuAddress), handle(handles[0]) {
        for (uint32_t i = 0; i < EngineLimits::maxHandleCount; i++) {
            handles[i] = ALLOCATION_HANDLE;
        }
    }
    using WddmAllocation::cpuPtr;
    using WddmAllocation::handles;
    using WddmAllocation::memoryPool;
    using WddmAllocation::size;

    D3DGPU_VIRTUAL_ADDRESS &gpuPtr;
    D3DKMT_HANDLE &handle;
};

} // namespace NEO
