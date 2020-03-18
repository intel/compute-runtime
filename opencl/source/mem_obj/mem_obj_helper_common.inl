/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/mem_obj/mem_obj_helper.h"

namespace NEO {

bool MemObjHelper::validateMemoryPropertiesForBuffer(const MemoryPropertiesFlags &memoryProperties, cl_mem_flags flags, cl_mem_flags_intel flagsIntel, const Context &context) {
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

    return validateExtraMemoryProperties(memoryProperties, flags, flagsIntel, context);
}

bool MemObjHelper::validateMemoryPropertiesForImage(const MemoryPropertiesFlags &memoryProperties, cl_mem_flags flags, cl_mem_flags_intel flagsIntel, cl_mem parent, const Context &context) {
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

    return validateExtraMemoryProperties(memoryProperties, flags, flagsIntel, context);
}

AllocationProperties MemObjHelper::getAllocationPropertiesWithImageInfo(uint32_t rootDeviceIndex, ImageInfo &imgInfo, bool allocateMemory, const MemoryPropertiesFlags &memoryProperties, const HardwareInfo &hwInfo) {
    AllocationProperties allocationProperties{rootDeviceIndex, allocateMemory, imgInfo, GraphicsAllocation::AllocationType::IMAGE};
    MemoryPropertiesParser::fillPoliciesInProperties(allocationProperties, memoryProperties, hwInfo);
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

const uint64_t MemObjHelper::commonFlagsIntel = extraFlagsIntel | CL_MEM_LOCALLY_UNCACHED_RESOURCE |
                                                CL_MEM_LOCALLY_UNCACHED_SURFACE_STATE_RESOURCE | CL_MEM_48BIT_RESOURCE_INTEL;

const uint64_t MemObjHelper::validFlagsForBuffer = commonFlags | CL_MEM_ALLOW_UNRESTRICTED_SIZE_INTEL;

const uint64_t MemObjHelper::validFlagsForBufferIntel = commonFlagsIntel | CL_MEM_ALLOW_UNRESTRICTED_SIZE_INTEL;

const uint64_t MemObjHelper::validFlagsForImage = commonFlags | CL_MEM_NO_ACCESS_INTEL | CL_MEM_ACCESS_FLAGS_UNRESTRICTED_INTEL | CL_MEM_FORCE_LINEAR_STORAGE_INTEL;

const uint64_t MemObjHelper::validFlagsForImageIntel = commonFlagsIntel;

} // namespace NEO
