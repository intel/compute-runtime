/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "runtime/memory_manager/memory_manager.h"

namespace OCLRT {
struct MockAllocationProperties : public AllocationProperties {
  public:
    MockAllocationProperties(size_t size, GraphicsAllocation::AllocationType allocationType) : AllocationProperties(size, allocationType) {}
    MockAllocationProperties(size_t size) : AllocationProperties(size, GraphicsAllocation::AllocationType::UNDECIDED) {}
    MockAllocationProperties(bool allocateMemory, size_t size) : AllocationProperties(allocateMemory, size, GraphicsAllocation::AllocationType::UNDECIDED) {}

    static AllocationProperties getPropertiesFor32BitInternalAllocation(size_t size, bool allocateMemory) {
        return AllocationProperties{allocateMemory, size, GraphicsAllocation::AllocationType::INTERNAL_HEAP};
    }
    static AllocationProperties getPropertiesFor32BitExternalAllocation(size_t size, bool allocateMemory) {
        return AllocationProperties{allocateMemory, size, GraphicsAllocation::AllocationType::BUFFER};
    }
};
} // namespace OCLRT
