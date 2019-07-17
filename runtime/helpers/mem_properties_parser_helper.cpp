/*
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/helpers/mem_properties_parser_helper.h"

namespace NEO {

bool NEO::MemoryPropertiesParser::parseMemoryProperties(const cl_mem_properties_intel *properties, MemoryProperties &propertiesStruct) {
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

void MemoryPropertiesParser::fillPoliciesInProperties(AllocationProperties &allocationProperties, const MemoryProperties &memoryProperties) {
    fillCachePolicyInProperties(allocationProperties,
                                isValueSet(memoryProperties.flags_intel, CL_MEM_LOCALLY_UNCACHED_RESOURCE),
                                isValueSet(memoryProperties.flags, CL_MEM_READ_ONLY),
                                false);
}

} // namespace NEO
