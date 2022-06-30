/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/memory_manager/memory_manager.h"
#include "shared/test/common/mocks/mock_graphics_allocation.h"

namespace NEO {
struct MockAllocationProperties : public AllocationProperties {
    MockAllocationProperties(uint32_t rootDeviceIndex, size_t size) : AllocationProperties(rootDeviceIndex, true, size, AllocationType::INTERNAL_HOST_MEMORY, false, mockDeviceBitfield) {}
    MockAllocationProperties(uint32_t rootDeviceIndex, size_t size, AllocationType type) : AllocationProperties(rootDeviceIndex, true, size, type, false, mockDeviceBitfield) {}
    MockAllocationProperties(uint32_t rootDeviceIndex, size_t size, DeviceBitfield deviceBitfield) : AllocationProperties(rootDeviceIndex, true, size, AllocationType::INTERNAL_HOST_MEMORY, false, deviceBitfield) {}
    MockAllocationProperties(uint32_t rootDeviceIndex, bool allocateMemory, size_t size) : AllocationProperties(rootDeviceIndex, allocateMemory, size, AllocationType::INTERNAL_HOST_MEMORY, false, mockDeviceBitfield) {}
    MockAllocationProperties(uint32_t rootDeviceIndex, bool allocateMemory, size_t size, DeviceBitfield deviceBitfield) : AllocationProperties(rootDeviceIndex, allocateMemory, size, AllocationType::INTERNAL_HOST_MEMORY, false, deviceBitfield) {}
    MockAllocationProperties(uint32_t rootDeviceIndex, bool allocateMemory, size_t size, AllocationType allocationType) : AllocationProperties(rootDeviceIndex, allocateMemory, size, allocationType, false, mockDeviceBitfield) {}
    MockAllocationProperties(uint32_t rootDeviceIndex, bool allocateMemory, size_t size, AllocationType allocationType, DeviceBitfield deviceBitfield) : AllocationProperties(rootDeviceIndex, allocateMemory, size, allocationType, false, deviceBitfield) {}
};
} // namespace NEO
