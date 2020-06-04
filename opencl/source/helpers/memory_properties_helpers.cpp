/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/helpers/memory_properties_helpers_base.inl"
#include "opencl/source/mem_obj/mem_obj_helper.h"

namespace NEO {

void MemoryPropertiesHelper::addExtraMemoryProperties(MemoryProperties &properties, cl_mem_flags flags, cl_mem_flags_intel flagsIntel,
                                                      const Device *pDevice) {
}
DeviceBitfield MemoryPropertiesHelper::adjustDeviceBitfield(const MemoryProperties &memoryProperties, DeviceBitfield deviceBitfield) {
    return deviceBitfield;
}

bool MemoryPropertiesHelper::parseMemoryProperties(const cl_mem_properties_intel *properties, MemoryProperties &memoryProperties,
                                                   cl_mem_flags &flags, cl_mem_flags_intel &flagsIntel,
                                                   cl_mem_alloc_flags_intel &allocflags, ObjType objectType, Context &context) {

    if (properties != nullptr) {
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
    }

    memoryProperties = MemoryPropertiesHelper::createMemoryProperties(flags, flagsIntel, allocflags, nullptr);

    switch (objectType) {
    case MemoryPropertiesHelper::ObjType::BUFFER:
        return isFieldValid(flags, MemObjHelper::validFlagsForBuffer) &&
               isFieldValid(flagsIntel, MemObjHelper::validFlagsForBufferIntel);
    case MemoryPropertiesHelper::ObjType::IMAGE:
        return isFieldValid(flags, MemObjHelper::validFlagsForImage) &&
               isFieldValid(flagsIntel, MemObjHelper::validFlagsForImageIntel);
    default:
        break;
    }
    return true;
}

void MemoryPropertiesHelper::fillPoliciesInProperties(AllocationProperties &allocationProperties, const MemoryProperties &memoryProperties, const HardwareInfo &hwInfo) {
    fillCachePolicyInProperties(allocationProperties,
                                memoryProperties.flags.locallyUncachedResource,
                                memoryProperties.flags.readOnly,
                                false);
}

} // namespace NEO
