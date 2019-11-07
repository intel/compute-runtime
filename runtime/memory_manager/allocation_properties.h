/*
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "core/memory_manager/graphics_allocation.h"

namespace NEO {
struct ImageInfo;
struct AllocationProperties {
    constexpr static uint32_t noDeviceSpecified = std::numeric_limits<uint32_t>::max();
    union {
        struct {
            uint32_t allocateMemory : 1;
            uint32_t flushL3RequiredForRead : 1;
            uint32_t flushL3RequiredForWrite : 1;
            uint32_t forcePin : 1;
            uint32_t uncacheable : 1;
            uint32_t multiOsContextCapable : 1;
            uint32_t readOnlyMultiStorage : 1;
            uint32_t reserved : 25;
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
    uint32_t subDeviceIndex = AllocationProperties::noDeviceSpecified;

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
        : AllocationProperties(rootDeviceIndex, allocateMemory, size, allocationType, false, isMultiStorageAllocation, AllocationProperties::noDeviceSpecified) {}

    AllocationProperties(uint32_t rootDeviceIndexParam,
                         bool allocateMemoryParam,
                         size_t sizeParam,
                         GraphicsAllocation::AllocationType allocationTypeParam,
                         bool multiOsContextCapableParam,
                         bool isMultiStorageAllocationParam,
                         uint32_t subDeviceIndexParam)
        : rootDeviceIndex(rootDeviceIndexParam), size(sizeParam), allocationType(allocationTypeParam), multiStorageResource(isMultiStorageAllocationParam), subDeviceIndex(subDeviceIndexParam) {
        allFlags = 0;
        flags.flushL3RequiredForRead = 1;
        flags.flushL3RequiredForWrite = 1;
        flags.allocateMemory = allocateMemoryParam;
        flags.multiOsContextCapable = multiOsContextCapableParam;
    }
};
} // namespace NEO
