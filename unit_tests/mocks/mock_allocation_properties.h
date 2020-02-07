/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "core/memory_manager/memory_manager.h"

namespace NEO {
struct MockAllocationProperties : public AllocationProperties {
    MockAllocationProperties(size_t size, GraphicsAllocation::AllocationType allocationType) : AllocationProperties(0, size, allocationType) {}
    MockAllocationProperties(size_t size) : AllocationProperties(0, size, GraphicsAllocation::AllocationType::INTERNAL_HOST_MEMORY) {}
    MockAllocationProperties(bool allocateMemory, size_t size) : AllocationProperties(0, allocateMemory, size, GraphicsAllocation::AllocationType::INTERNAL_HOST_MEMORY, false) {}
    MockAllocationProperties(bool allocateMemory, size_t size, GraphicsAllocation::AllocationType allocationType) : AllocationProperties(0, allocateMemory, size, allocationType, false) {}
};
} // namespace NEO
