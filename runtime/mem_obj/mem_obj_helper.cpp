/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/mem_obj/mem_obj_helper.h"

#include "common/helpers/bit_helpers.h"

namespace NEO {

bool MemObjHelper::parseMemoryProperties(const cl_mem_properties_intel *properties, MemoryProperties &propertiesStruct) {
    if (properties == nullptr) {
        return true;
    }

    for (int i = 0; properties[i] != 0; i += 2) {
        switch (properties[i]) {
        case CL_MEM_FLAGS:
            propertiesStruct.flags |= static_cast<cl_mem_flags>(properties[i + 1]);
            break;
        case CL_MEM_FLAGS_INTEL:
            propertiesStruct.flags_intel |= static_cast<cl_mem_flags_intel>(properties[i + 1]);
            break;
        default:
            return false;
        }
    }
    return true;
}

AllocationProperties MemObjHelper::getAllocationProperties(MemoryProperties memoryProperties, bool allocateMemory,
                                                           size_t size, GraphicsAllocation::AllocationType type) {
    AllocationProperties allocationProperties(allocateMemory, size, type);
    fillCachePolicyInProperties(allocationProperties,
                                isValueSet(memoryProperties.flags_intel, CL_MEM_LOCALLY_UNCACHED_RESOURCE),
                                isValueSet(memoryProperties.flags, CL_MEM_READ_ONLY));
    return allocationProperties;
}

bool MemObjHelper::isSuitableForRenderCompression(bool renderCompressed, const MemoryProperties &properties, ContextType contextType, bool preferCompression) {
    return renderCompressed;
}

bool MemObjHelper::validateExtraMemoryProperties(const MemoryProperties &properties) {
    return true;
}

void MemObjHelper::addExtraMemoryProperties(MemoryProperties &properties) {
}

} // namespace NEO
