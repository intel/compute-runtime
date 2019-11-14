/*
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "core/memory_manager/graphics_allocation.h"
#include "runtime/device/sub_device.h"

namespace NEO {
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
            uint32_t readOnlyMultiStorage : 1;
            uint32_t shareable : 1;
            uint32_t reserved : 24;
        } flags;
        uint32_t allFlags = 0;
    };
    static_assert(sizeof(AllocationProperties::flags) == sizeof(AllocationProperties::allFlags), "");
    const uint32_t rootDeviceIndex;
    size_t size = 0;
    size_t alignment = 0;
    GraphicsAllocation::AllocationType allocationType = GraphicsAllocation::AllocationType::UNKNOWN;
    ImageInfo *imgInfo = nullptr;
    bool multiStorageResource = false;
    DeviceBitfield subDevicesBitfield{};

    AllocationProperties(uint32_t rootDeviceIndex, size_t size,
                         GraphicsAllocation::AllocationType allocationType)
        : AllocationProperties(rootDeviceIndex, true, size, allocationType, false) {}

    AllocationProperties(uint32_t rootDeviceIndex, bool allocateMemory,
                         ImageInfo &imgInfo,
                         GraphicsAllocation::AllocationType allocationType)
        : AllocationProperties(rootDeviceIndex, allocateMemory, 0u, allocationType, false) {
        this->imgInfo = &imgInfo;
    }

    AllocationProperties(uint32_t rootDeviceIndex,
                         bool allocateMemory,
                         size_t size,
                         GraphicsAllocation::AllocationType allocationType,
                         bool isMultiStorageAllocation)
        : AllocationProperties(rootDeviceIndex, allocateMemory, size, allocationType, false, isMultiStorageAllocation, {}) {}

    AllocationProperties(uint32_t rootDeviceIndexParam,
                         bool allocateMemoryParam,
                         size_t sizeParam,
                         GraphicsAllocation::AllocationType allocationTypeParam,
                         bool multiOsContextCapableParam,
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
        flags.multiOsContextCapable = multiOsContextCapableParam;
    }
};
} // namespace NEO
