/*
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/helpers/mem_properties_parser_helper.h"

#include "runtime/mem_obj/mem_obj_helper.h"

namespace NEO {

bool NEO::MemoryPropertiesParser::parseMemoryProperties(const cl_mem_properties_intel *properties, MemoryProperties &propertiesStruct, ObjType objectType) {
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

    switch (objectType) {
    case MemoryPropertiesParser::ObjType::BUFFER:
        return isFieldValid(propertiesStruct.flags, MemObjHelper::validFlagsForBuffer) &&
               isFieldValid(propertiesStruct.flags_intel, MemObjHelper::validFlagsForBufferIntel);
    case MemoryPropertiesParser::ObjType::IMAGE:
        return isFieldValid(propertiesStruct.flags, MemObjHelper::validFlagsForImage) &&
               isFieldValid(propertiesStruct.flags_intel, MemObjHelper::validFlagsForImageIntel);
    default:
        break;
    }
    return true;
}

void MemoryPropertiesParser::fillPoliciesInProperties(AllocationProperties &allocationProperties, const MemoryPropertiesFlags &memoryProperties) {
    fillCachePolicyInProperties(allocationProperties,
                                memoryProperties.flags.locallyUncachedResource,
                                memoryProperties.flags.readOnly,
                                false);
}

} // namespace NEO
