/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/memory_manager/memory_manager.h"

namespace NEO {
struct MockAllocationProperties : public AllocationProperties {
    MockAllocationProperties(uint32_t rootDeviceIndex, size_t size) : AllocationProperties(rootDeviceIndex, size, GraphicsAllocation::AllocationType::INTERNAL_HOST_MEMORY) {}
    MockAllocationProperties(uint32_t rootDeviceIndex, bool allocateMemory, size_t size) : AllocationProperties(rootDeviceIndex, allocateMemory, size, GraphicsAllocation::AllocationType::INTERNAL_HOST_MEMORY, false) {}
    MockAllocationProperties(uint32_t rootDeviceIndex, bool allocateMemory, size_t size, GraphicsAllocation::AllocationType allocationType) : AllocationProperties(rootDeviceIndex, allocateMemory, size, allocationType, false) {}
};
} // namespace NEO
