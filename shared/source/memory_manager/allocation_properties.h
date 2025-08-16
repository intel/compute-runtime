/*
 * Copyright (C) 2019-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/memory_manager/graphics_allocation.h"

namespace NEO {
class OsContext;
struct ImageInfo;
struct AllocationProperties {
    union {
        struct {
            uint32_t allocateMemory : 1;
            uint32_t flushL3RequiredForRead : 1;
            uint32_t flushL3RequiredForWrite : 1;
            uint32_t forcePin : 1;
            uint32_t uncacheable : 1;
            uint32_t multiOsContextCapable : 1;
            uint32_t shareable : 1;
            uint32_t resource48Bit : 1;
            uint32_t isUSMHostAllocation : 1;
            uint32_t isUSMDeviceAllocation : 1;
            uint32_t use32BitFrontWindow : 1;
            uint32_t forceSystemMemory : 1;
            uint32_t preferCompressed : 1;
            uint32_t cantBeReadOnly : 1;
            uint32_t shareableWithoutNTHandle : 1;
            uint32_t reserved : 16;
        } flags;
        uint32_t allFlags = 0;
    };
    static_assert(sizeof(AllocationProperties::flags) == sizeof(AllocationProperties::allFlags), "");
    uint32_t rootDeviceIndex = std::numeric_limits<uint32_t>::max();
    size_t size = 0;
    size_t alignment = 0;
    AllocationType allocationType = AllocationType::unknown;
    GraphicsAllocation::UsmInitialPlacement usmInitialPlacement = GraphicsAllocation::UsmInitialPlacement::DEFAULT;
    ImageInfo *imgInfo = nullptr;
    bool multiStorageResource = false;
    ColouringPolicy colouringPolicy = ColouringPolicy::deviceCountBased;
    size_t colouringGranularity = MemoryConstants::pageSize64k;
    DeviceBitfield subDevicesBitfield{};
    uint64_t gpuAddress = 0;
    OsContext *osContext = nullptr;
    bool useMmapObject = true;
    bool forceKMDAllocation = false;
    bool makeGPUVaDifferentThanCPUPtr = false;
    uint32_t cacheRegion = 0;
    bool makeDeviceBufferLockable = false;
    bool isaPaddingIncluded = false;

    AllocationProperties(uint32_t rootDeviceIndex, size_t size,
                         AllocationType allocationType, DeviceBitfield subDevicesBitfieldParam)
        : AllocationProperties(rootDeviceIndex, true, size, allocationType, false, subDevicesBitfieldParam) {}

    AllocationProperties(uint32_t rootDeviceIndex, bool allocateMemory,
                         ImageInfo *imgInfo,
                         AllocationType allocationType,
                         DeviceBitfield subDevicesBitfieldParam)
        : AllocationProperties(rootDeviceIndex, allocateMemory, 0u, allocationType, false, subDevicesBitfieldParam) {
        this->imgInfo = imgInfo;
    }

    AllocationProperties(uint32_t rootDeviceIndex,
                         bool allocateMemory,
                         size_t size,
                         AllocationType allocationType,
                         bool isMultiStorageAllocation,
                         DeviceBitfield subDevicesBitfieldParam)
        : AllocationProperties(rootDeviceIndex, allocateMemory, size, allocationType, false, isMultiStorageAllocation, subDevicesBitfieldParam) {}

    AllocationProperties(uint32_t rootDeviceIndexParam,
                         bool allocateMemoryParam,
                         size_t sizeParam,
                         AllocationType allocationTypeParam,
                         bool multiOsContextCapable,
                         bool isMultiStorageAllocationParam,
                         DeviceBitfield subDevicesBitfieldParam)
        : rootDeviceIndex(rootDeviceIndexParam),
          size(sizeParam),
          allocationType(allocationTypeParam),
          multiStorageResource(isMultiStorageAllocationParam),
          subDevicesBitfield(subDevicesBitfieldParam) {
        allFlags = 0;
        flags.flushL3RequiredForRead = 1;
        flags.flushL3RequiredForWrite = 1;
        flags.allocateMemory = allocateMemoryParam;
        flags.multiOsContextCapable = multiOsContextCapable;
    }
};

struct AllocationData {
    union {
        struct {
            uint32_t allocateMemory : 1;
            uint32_t allow64kbPages : 1;
            uint32_t allow32Bit : 1;
            uint32_t useSystemMemory : 1;
            uint32_t forcePin : 1;
            uint32_t uncacheable : 1;
            uint32_t flushL3 : 1;
            uint32_t preferCompressed : 1;
            uint32_t multiOsContextCapable : 1;
            uint32_t requiresCpuAccess : 1;
            uint32_t shareable : 1;
            uint32_t resource48Bit : 1;
            uint32_t isUSMHostAllocation : 1;
            uint32_t use32BitFrontWindow : 1;
            uint32_t isUSMDeviceMemory : 1;
            uint32_t zeroMemory : 1;
            uint32_t cantBeReadOnly : 1;
            uint32_t shareableWithoutNTHandle : 1;
            uint32_t reserved : 14;
        } flags;
        uint32_t allFlags = 0;
    };
    static_assert(sizeof(AllocationData::flags) == sizeof(AllocationData::allFlags), "");
    AllocationType type = AllocationType::unknown;
    GraphicsAllocation::UsmInitialPlacement usmInitialPlacement = GraphicsAllocation::UsmInitialPlacement::DEFAULT;
    GfxMemoryAllocationMethod allocationMethod = GfxMemoryAllocationMethod::notDefined;
    const void *hostPtr = nullptr;
    uint64_t gpuAddress = 0;
    size_t size = 0;
    size_t alignment = 0;
    StorageInfo storageInfo = {};
    ImageInfo *imgInfo = nullptr;
    uint32_t rootDeviceIndex = 0;
    OsContext *osContext = nullptr;
    bool forceKMDAllocation = false;
    bool makeGPUVaDifferentThanCPUPtr = false;
    bool useMmapObject = true;
    uint32_t cacheRegion = 0;
};
} // namespace NEO
