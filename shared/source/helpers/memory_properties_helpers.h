/*
 * Copyright (C) 2020-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/memory_manager/graphics_allocation.h"

namespace NEO {
struct AllocationProperties;
struct HardwareInfo;
struct MemoryProperties;

class MemoryPropertiesHelper {
  public:
    static AllocationProperties getAllocationProperties(
        uint32_t rootDeviceIndex, const MemoryProperties &memoryProperties, bool allocateMemory, size_t size,
        AllocationType type, bool multiStorageResource, const HardwareInfo &hwInfo,
        DeviceBitfield subDevicesBitfieldParam, bool deviceOnlyVisibilty);

    static DeviceBitfield adjustDeviceBitfield(uint32_t rootDeviceIndex, const MemoryProperties &memoryProperties,
                                               DeviceBitfield subDevicesBitfieldParam);

    static void fillPoliciesInProperties(AllocationProperties &allocationProperties, const MemoryProperties &memoryProperties, const HardwareInfo &hwInfo, bool deviceOnlyVisibilty);

    static void fillCachePolicyInProperties(AllocationProperties &allocationProperties, bool uncached, bool readOnly,
                                            bool deviceOnlyVisibilty, uint32_t cacheRegion);

    static uint32_t getCacheRegion(const MemoryProperties &memoryProperties);

    static GraphicsAllocation::UsmInitialPlacement getUSMInitialPlacement(const MemoryProperties &memoryProperties);

    static void setUSMInitialPlacement(AllocationProperties &allocationProperties, GraphicsAllocation::UsmInitialPlacement initialPlacement);
};
} // namespace NEO
