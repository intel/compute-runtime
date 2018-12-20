/*
 * Copyright (C) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/mem_obj/mem_obj_helper.h"

namespace OCLRT {

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

bool MemObjHelper::validateExtraMemoryProperties(const MemoryProperties &properties) {
    return true;
}

AllocationProperties MemObjHelper::getAllocationProperties(cl_mem_flags_intel flags, bool allocateMemory, size_t size, GraphicsAllocation::AllocationType type) {
    return AllocationProperties(allocateMemory, size, type);
}

AllocationProperties MemObjHelper::getAllocationProperties(ImageInfo *imgInfo) {
    return AllocationProperties(imgInfo);
}

DevicesBitfield MemObjHelper::getDevicesBitfield(const MemoryProperties &properties) {
    return DevicesBitfield(0);
}

} // namespace OCLRT
