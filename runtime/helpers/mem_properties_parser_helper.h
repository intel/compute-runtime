/*
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "common/helpers/bit_helpers.h"
#include "runtime/memory_manager/allocation_properties.h"

#include "mem_obj_types.h"

namespace NEO {

class MemoryPropertiesParser {
  public:
    static bool parseMemoryProperties(const cl_mem_properties_intel *properties, MemoryProperties &propertiesStruct);

    static AllocationProperties getAllocationProperties(MemoryProperties memoryProperties, bool allocateMemory,
                                                        size_t size, GraphicsAllocation::AllocationType type, bool multiStorageResource) {
        AllocationProperties allocationProperties(allocateMemory, size, type, multiStorageResource);
        fillPoliciesInProperties(allocationProperties, memoryProperties);
        return allocationProperties;
    }

    static void fillPoliciesInProperties(AllocationProperties &allocationProperties, const MemoryProperties &memoryProperties);

    static void fillCachePolicyInProperties(AllocationProperties &allocationProperties, bool uncached, bool readOnly,
                                            bool deviceOnlyVisibilty) {
        allocationProperties.flags.uncacheable = uncached;
        auto cacheFlushRequired = !uncached && !readOnly && !deviceOnlyVisibilty;
        allocationProperties.flags.flushL3RequiredForRead = cacheFlushRequired;
        allocationProperties.flags.flushL3RequiredForWrite = cacheFlushRequired;
    }
};
} // namespace NEO
