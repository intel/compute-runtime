/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/bit_helpers.h"
#include "shared/source/memory_manager/allocation_properties.h"

#include "memory_properties_flags.h"

namespace NEO {

class MemoryPropertiesHelper {
  public:
    enum class ObjType {
        UNKNOWN,
        BUFFER,
        IMAGE,
    };

    static AllocationProperties getAllocationProperties(
        uint32_t rootDeviceIndex, MemoryProperties memoryProperties, bool allocateMemory, size_t size,
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
