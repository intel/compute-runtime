/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/mem_obj/mem_obj_helper.h"

namespace NEO {

bool MemObjHelper::validateMemoryPropertiesForBuffer(const MemoryPropertiesFlags &memoryProperties, cl_mem_flags flags, cl_mem_flags_intel flagsIntel) {
    /* Check all the invalid flags combination. */
    if ((isValueSet(flags, CL_MEM_READ_WRITE | CL_MEM_READ_ONLY)) ||
        (isValueSet(flags, CL_MEM_READ_WRITE | CL_MEM_WRITE_ONLY)) ||
        (isValueSet(flags, CL_MEM_READ_ONLY | CL_MEM_WRITE_ONLY)) ||
        (isValueSet(flags, CL_MEM_ALLOC_HOST_PTR | CL_MEM_USE_HOST_PTR)) ||
        (isValueSet(flags, CL_MEM_COPY_HOST_PTR | CL_MEM_USE_HOST_PTR)) ||
        (isValueSet(flags, CL_MEM_HOST_READ_ONLY | CL_MEM_HOST_NO_ACCESS)) ||
        (isValueSet(flags, CL_MEM_HOST_READ_ONLY | CL_MEM_HOST_WRITE_ONLY)) ||
        (isValueSet(flags, CL_MEM_HOST_WRITE_ONLY | CL_MEM_HOST_NO_ACCESS))) {
        return false;
    }

    return validateExtraMemoryProperties(memoryProperties, flags, flagsIntel);
}

bool MemObjHelper::validateMemoryPropertiesForImage(const MemoryPropertiesFlags &memoryProperties, cl_mem_flags flags, cl_mem_flags_intel flagsIntel, cl_mem parent) {
    /* Check all the invalid flags combination. */
    if ((!isValueSet(flags, CL_MEM_ACCESS_FLAGS_UNRESTRICTED_INTEL)) &&
        (isValueSet(flags, CL_MEM_READ_WRITE | CL_MEM_WRITE_ONLY) ||
         isValueSet(flags, CL_MEM_READ_WRITE | CL_MEM_READ_ONLY) ||
         isValueSet(flags, CL_MEM_WRITE_ONLY | CL_MEM_READ_ONLY) ||
         isValueSet(flags, CL_MEM_ALLOC_HOST_PTR | CL_MEM_USE_HOST_PTR) ||
         isValueSet(flags, CL_MEM_COPY_HOST_PTR | CL_MEM_USE_HOST_PTR) ||
         isValueSet(flags, CL_MEM_HOST_WRITE_ONLY | CL_MEM_HOST_READ_ONLY) ||
         isValueSet(flags, CL_MEM_HOST_WRITE_ONLY | CL_MEM_HOST_NO_ACCESS) ||
         isValueSet(flags, CL_MEM_HOST_READ_ONLY | CL_MEM_HOST_NO_ACCESS) ||
         isValueSet(flags, CL_MEM_NO_ACCESS_INTEL | CL_MEM_READ_WRITE) ||
         isValueSet(flags, CL_MEM_NO_ACCESS_INTEL | CL_MEM_WRITE_ONLY) ||
         isValueSet(flags, CL_MEM_NO_ACCESS_INTEL | CL_MEM_READ_ONLY))) {
        return false;
    }

    auto parentMemObj = castToObject<MemObj>(parent);
    if (parentMemObj != nullptr && flags) {
        auto parentFlags = parentMemObj->getMemoryPropertiesFlags();
        /* Check whether flags are compatible with parent. */
        if (isValueSet(flags, CL_MEM_ALLOC_HOST_PTR) ||
            isValueSet(flags, CL_MEM_COPY_HOST_PTR) ||
            isValueSet(flags, CL_MEM_USE_HOST_PTR) ||
            ((!isValueSet(parentFlags, CL_MEM_ACCESS_FLAGS_UNRESTRICTED_INTEL)) &&
             (!isValueSet(flags, CL_MEM_ACCESS_FLAGS_UNRESTRICTED_INTEL)) &&
             ((isValueSet(parentFlags, CL_MEM_WRITE_ONLY) && isValueSet(flags, CL_MEM_READ_WRITE)) ||
              (isValueSet(parentFlags, CL_MEM_WRITE_ONLY) && isValueSet(flags, CL_MEM_READ_ONLY)) ||
              (isValueSet(parentFlags, CL_MEM_READ_ONLY) && isValueSet(flags, CL_MEM_READ_WRITE)) ||
              (isValueSet(parentFlags, CL_MEM_READ_ONLY) && isValueSet(flags, CL_MEM_WRITE_ONLY)) ||
              (isValueSet(parentFlags, CL_MEM_NO_ACCESS_INTEL) && isValueSet(flags, CL_MEM_READ_WRITE)) ||
              (isValueSet(parentFlags, CL_MEM_NO_ACCESS_INTEL) && isValueSet(flags, CL_MEM_WRITE_ONLY)) ||
              (isValueSet(parentFlags, CL_MEM_NO_ACCESS_INTEL) && isValueSet(flags, CL_MEM_READ_ONLY)) ||
              (isValueSet(parentFlags, CL_MEM_HOST_NO_ACCESS) && isValueSet(flags, CL_MEM_HOST_WRITE_ONLY)) ||
              (isValueSet(parentFlags, CL_MEM_HOST_NO_ACCESS) && isValueSet(flags, CL_MEM_HOST_READ_ONLY))))) {
            return false;
        }
    }

    return validateExtraMemoryProperties(memoryProperties, flags, flagsIntel);
}

bool MemObjHelper::parseUnifiedMemoryProperties(cl_mem_properties_intel *properties, SVMAllocsManager::UnifiedMemoryProperties &unifiedMemoryProperties) {
    uint64_t unifiedMemoryTokenValue = properties ? *properties : 0;

    while (unifiedMemoryTokenValue != 0) {
        switch (unifiedMemoryTokenValue) {
        case CL_MEM_ALLOC_FLAGS_INTEL:
            unifiedMemoryProperties.allocationFlags = properties[1];
            break;
        default:
            return false;
        }
        properties += 2;
        unifiedMemoryTokenValue = *properties;
    }
    return true;
}

AllocationProperties MemObjHelper::getAllocationPropertiesWithImageInfo(uint32_t rootDeviceIndex, ImageInfo &imgInfo, bool allocateMemory, const MemoryPropertiesFlags &memoryProperties) {
    AllocationProperties allocationProperties{rootDeviceIndex, allocateMemory, imgInfo, GraphicsAllocation::AllocationType::IMAGE};
    MemoryPropertiesParser::fillPoliciesInProperties(allocationProperties, memoryProperties);
    return allocationProperties;
}

bool MemObjHelper::checkMemFlagsForSubBuffer(cl_mem_flags flags) {
    const cl_mem_flags allValidFlags =
        CL_MEM_READ_WRITE | CL_MEM_WRITE_ONLY | CL_MEM_READ_ONLY |
        CL_MEM_HOST_WRITE_ONLY | CL_MEM_HOST_READ_ONLY | CL_MEM_HOST_NO_ACCESS;

    return isFieldValid(flags, allValidFlags);
}

SVMAllocsManager::SvmAllocationProperties MemObjHelper::getSvmAllocationProperties(cl_mem_flags flags) {
    SVMAllocsManager::SvmAllocationProperties svmProperties;
    svmProperties.coherent = isValueSet(flags, CL_MEM_SVM_FINE_GRAIN_BUFFER);
    svmProperties.hostPtrReadOnly = isValueSet(flags, CL_MEM_HOST_READ_ONLY) || isValueSet(flags, CL_MEM_HOST_NO_ACCESS);
    svmProperties.readOnly = isValueSet(flags, CL_MEM_READ_ONLY);
    return svmProperties;
}

const uint64_t MemObjHelper::commonFlags = extraFlags | CL_MEM_READ_WRITE | CL_MEM_WRITE_ONLY | CL_MEM_READ_ONLY |
                                           CL_MEM_ALLOC_HOST_PTR | CL_MEM_COPY_HOST_PTR | CL_MEM_USE_HOST_PTR |
                                           CL_MEM_HOST_WRITE_ONLY | CL_MEM_HOST_READ_ONLY | CL_MEM_HOST_NO_ACCESS;

const uint64_t MemObjHelper::commonFlagsIntel = extraFlagsIntel | CL_MEM_LOCALLY_UNCACHED_RESOURCE | CL_MEM_LOCALLY_UNCACHED_SURFACE_STATE_RESOURCE;

const uint64_t MemObjHelper::validFlagsForBuffer = commonFlags | CL_MEM_ALLOW_UNRESTRICTED_SIZE_INTEL;

const uint64_t MemObjHelper::validFlagsForBufferIntel = commonFlagsIntel | CL_MEM_ALLOW_UNRESTRICTED_SIZE_INTEL;

const uint64_t MemObjHelper::validFlagsForImage = commonFlags | CL_MEM_NO_ACCESS_INTEL | CL_MEM_ACCESS_FLAGS_UNRESTRICTED_INTEL;

const uint64_t MemObjHelper::validFlagsForImageIntel = commonFlagsIntel;

} // namespace NEO
