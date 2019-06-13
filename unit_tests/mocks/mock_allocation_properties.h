/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "runtime/memory_manager/memory_manager.h"

namespace NEO {
struct MockAllocationProperties : public AllocationProperties {
    MockAllocationProperties(size_t size, GraphicsAllocation::AllocationType allocationType) : AllocationProperties(size, allocationType) {}
    MockAllocationProperties(size_t size) : AllocationProperties(size, GraphicsAllocation::AllocationType::INTERNAL_HOST_MEMORY) {}
    MockAllocationProperties(bool allocateMemory, size_t size) : AllocationProperties(allocateMemory, size, GraphicsAllocation::AllocationType::INTERNAL_HOST_MEMORY, false) {}
    MockAllocationProperties(bool allocateMemory, size_t size, GraphicsAllocation::AllocationType allocationType) : AllocationProperties(allocateMemory, size, allocationType, false) {}
};
} // namespace NEO
