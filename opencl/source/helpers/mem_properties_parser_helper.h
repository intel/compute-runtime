/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/bit_helpers.h"
#include "shared/source/memory_manager/allocation_properties.h"

#include "opencl/extensions/public/cl_ext_private.h"

#include "memory_properties_flags.h"

namespace NEO {

class MemoryPropertiesParser {
  public:
    enum class ObjType {
        UNKNOWN,
        BUFFER,
        IMAGE,
    };

    static bool parseMemoryProperties(const cl_mem_properties_intel *properties, MemoryPropertiesFlags &memoryProperties,
                                      cl_mem_flags &flags, cl_mem_flags_intel &flagsIntel, cl_mem_alloc_flags_intel &allocflags, ObjType objectType);

    static AllocationProperties getAllocationProperties(uint32_t rootDeviceIndex, MemoryPropertiesFlags memoryProperties, bool allocateMemory,
                                                        size_t size, GraphicsAllocation::AllocationType type, bool multiStorageResource) {
        AllocationProperties allocationProperties(rootDeviceIndex, allocateMemory, size, type, multiStorageResource);
        fillPoliciesInProperties(allocationProperties, memoryProperties);
        return allocationProperties;
    }

    static void fillPoliciesInProperties(AllocationProperties &allocationProperties, const MemoryPropertiesFlags &memoryProperties);

    static void fillCachePolicyInProperties(AllocationProperties &allocationProperties, bool uncached, bool readOnly,
                                            bool deviceOnlyVisibilty) {
        allocationProperties.flags.uncacheable = uncached;
        auto cacheFlushRequired = !uncached && !readOnly && !deviceOnlyVisibilty;
        allocationProperties.flags.flushL3RequiredForRead = cacheFlushRequired;
        allocationProperties.flags.flushL3RequiredForWrite = cacheFlushRequired;
    }
};
} // namespace NEO
