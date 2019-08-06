/*
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "runtime/memory_manager/graphics_allocation.h"

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
    size_t size = 0;
    size_t alignment = 0;
    GraphicsAllocation::AllocationType allocationType = GraphicsAllocation::AllocationType::UNKNOWN;
    ImageInfo *imgInfo = nullptr;
    uint32_t deviceIndex = AllocationProperties::noDeviceSpecified;
    bool multiStorageResource = false;

    AllocationProperties(size_t size,
                         GraphicsAllocation::AllocationType allocationType)
        : AllocationProperties(true, size, allocationType, false) {}

    AllocationProperties(bool allocateMemory,
                         ImageInfo &imgInfo,
                         GraphicsAllocation::AllocationType allocationType)
        : AllocationProperties(allocateMemory, 0u, allocationType, false) {
        this->imgInfo = &imgInfo;
    }

    AllocationProperties(bool allocateMemory,
                         size_t size,
                         GraphicsAllocation::AllocationType allocationType,
                         bool isMultiStorageAllocation)
        : AllocationProperties(allocateMemory, size, allocationType, false, AllocationProperties::noDeviceSpecified) {
        this->multiStorageResource = isMultiStorageAllocation;
    }

    AllocationProperties(bool allocateMemory,
                         size_t size,
                         GraphicsAllocation::AllocationType allocationType,
                         bool multiOsContextCapable,
                         uint32_t deviceIndex)
        : size(size), allocationType(allocationType), deviceIndex(deviceIndex) {
        allFlags = 0;
        flags.flushL3RequiredForRead = 1;
        flags.flushL3RequiredForWrite = 1;
        flags.allocateMemory = allocateMemory;
        flags.multiOsContextCapable = multiOsContextCapable;
    }
};
} // namespace NEO
