/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/bit_helpers.h"
#include "shared/source/helpers/memory_properties_helpers.h"

namespace NEO {

AllocationProperties MemoryPropertiesHelper::getAllocationProperties(
    uint32_t rootDeviceIndex, MemoryProperties memoryProperties, bool allocateMemory, size_t size,
    GraphicsAllocation::AllocationType type, bool multiStorageResource, const HardwareInfo &hwInfo,
    DeviceBitfield subDevicesBitfieldParam, bool deviceOnlyVisibilty) {

    auto deviceBitfield = adjustDeviceBitfield(rootDeviceIndex, memoryProperties, subDevicesBitfieldParam);
    AllocationProperties allocationProperties(rootDeviceIndex, allocateMemory, size, type, multiStorageResource, deviceBitfield);
    allocationProperties.flags.resource48Bit = memoryProperties.flags.resource48Bit;

    fillPoliciesInProperties(allocationProperties, memoryProperties, hwInfo, deviceOnlyVisibilty);

    return allocationProperties;
}

void MemoryPropertiesHelper::fillCachePolicyInProperties(AllocationProperties &allocationProperties, bool uncached, bool readOnly,
                                                         bool deviceOnlyVisibilty, uint32_t cacheRegion) {
    allocationProperties.flags.uncacheable = uncached;
    auto cacheFlushRequired = !uncached && !readOnly && !deviceOnlyVisibilty;
    allocationProperties.flags.flushL3RequiredForRead = cacheFlushRequired;
    allocationProperties.flags.flushL3RequiredForWrite = cacheFlushRequired;
    allocationProperties.cacheRegion = cacheRegion;
}

DeviceBitfield MemoryPropertiesHelper::adjustDeviceBitfield(uint32_t rootDeviceIndex, const MemoryProperties &memoryProperties,
                                                            DeviceBitfield deviceBitfieldIn) {
    if (rootDeviceIndex == memoryProperties.pDevice->getRootDeviceIndex()) {
        return deviceBitfieldIn & memoryProperties.pDevice->getDeviceBitfield();
    }
    return deviceBitfieldIn;
}

} // namespace NEO
