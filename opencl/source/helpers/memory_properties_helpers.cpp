/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/cl_device/cl_device.h"
#include "opencl/source/context/context.h"
#include "opencl/source/helpers/memory_properties_helpers_base.inl"
#include "opencl/source/mem_obj/mem_obj_helper.h"

namespace NEO {

void MemoryPropertiesHelper::addExtraMemoryProperties(MemoryProperties &properties, cl_mem_flags flags, cl_mem_flags_intel flagsIntel,
                                                      const Device *pDevice) {
}

DeviceBitfield MemoryPropertiesHelper::adjustDeviceBitfield(uint32_t rootDeviceIndex, const MemoryProperties &memoryProperties,
                                                            DeviceBitfield deviceBitfield) {
    return deviceBitfield;
}

bool MemoryPropertiesHelper::parseMemoryProperties(const cl_mem_properties_intel *properties, MemoryProperties &memoryProperties,
                                                   cl_mem_flags &flags, cl_mem_flags_intel &flagsIntel,
                                                   cl_mem_alloc_flags_intel &allocflags, ObjType objectType, Context &context) {
    Device *pDevice = &context.getDevice(0)->getDevice();

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

    memoryProperties = MemoryPropertiesHelper::createMemoryProperties(flags, flagsIntel, allocflags, pDevice);

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

void MemoryPropertiesHelper::fillPoliciesInProperties(AllocationProperties &allocationProperties, const MemoryProperties &memoryProperties, const HardwareInfo &hwInfo, bool deviceOnlyVisibilty) {
    fillCachePolicyInProperties(allocationProperties,
                                memoryProperties.flags.locallyUncachedResource,
                                memoryProperties.flags.readOnly,
                                deviceOnlyVisibilty,
                                0);
}

uint32_t MemoryPropertiesHelper::getCacheRegion(const MemoryProperties &memoryProperties) {
    return 0;
}

} // namespace NEO
