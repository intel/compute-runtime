/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/helpers/mem_properties_parser_helper.h"

#include "opencl/source/helpers/memory_properties_flags_helpers.h"
#include "opencl/source/mem_obj/mem_obj_helper.h"

namespace NEO {

bool MemoryPropertiesParser::parseMemoryProperties(const cl_mem_properties_intel *properties, MemoryPropertiesFlags &memoryProperties,
                                                   cl_mem_flags &flags, cl_mem_flags_intel &flagsIntel,
                                                   cl_mem_alloc_flags_intel &allocflags, ObjType objectType, Context &context) {
    if (properties == nullptr) {
        return true;
    }

    for (int i = 0; properties[i] != 0; i += 2) {
        switch (properties[i]) {
        case CL_MEM_FLAGS:
            flags |= static_cast<cl_mem_flags>(properties[i + 1]);
            break;
        case CL_MEM_FLAGS_INTEL:
            flagsIntel |= static_cast<cl_mem_flags_intel>(properties[i + 1]);
            break;
        case CL_MEM_ALLOC_FLAGS_INTEL:
            allocflags |= static_cast<cl_mem_alloc_flags_intel>(properties[i + 1]);
            break;
        default:
            return false;
        }
    }

    memoryProperties = MemoryPropertiesFlagsParser::createMemoryPropertiesFlags(flags, flagsIntel, allocflags);

    switch (objectType) {
    case MemoryPropertiesParser::ObjType::BUFFER:
        return isFieldValid(flags, MemObjHelper::validFlagsForBuffer) &&
               isFieldValid(flagsIntel, MemObjHelper::validFlagsForBufferIntel);
    case MemoryPropertiesParser::ObjType::IMAGE:
        return isFieldValid(flags, MemObjHelper::validFlagsForImage) &&
               isFieldValid(flagsIntel, MemObjHelper::validFlagsForImageIntel);
    default:
        break;
    }
    return true;
}

void MemoryPropertiesParser::fillPoliciesInProperties(AllocationProperties &allocationProperties, const MemoryPropertiesFlags &memoryProperties, const HardwareInfo &hwInfo) {
    fillCachePolicyInProperties(allocationProperties,
                                memoryProperties.flags.locallyUncachedResource,
                                memoryProperties.flags.readOnly,
                                false);
}

} // namespace NEO
