/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "CL/cl.h"
#include "mem_obj_types.h"
#include "public/cl_ext_private.h"
#include "runtime/mem_obj/mem_obj.h"
#include "runtime/memory_manager/memory_manager.h"

namespace OCLRT {

class MemObjHelper {
  public:
    static bool parseMemoryProperties(const cl_mem_properties_intel *properties, MemoryProperties &propertiesStruct);

    static bool validateMemoryProperties(const MemoryProperties &properties) {

        /* Are there some invalid flag bits? */
        if (!MemObjHelper::checkMemFlagsForBuffer(properties)) {
            return false;
        }

        /* Check all the invalid flags combination. */
        if (((properties.flags & CL_MEM_READ_WRITE) && (properties.flags & (CL_MEM_READ_ONLY | CL_MEM_WRITE_ONLY))) ||
            ((properties.flags & CL_MEM_READ_ONLY) && (properties.flags & (CL_MEM_WRITE_ONLY))) ||
            ((properties.flags & CL_MEM_ALLOC_HOST_PTR) && (properties.flags & CL_MEM_USE_HOST_PTR)) ||
            ((properties.flags & CL_MEM_COPY_HOST_PTR) && (properties.flags & CL_MEM_USE_HOST_PTR)) ||
            ((properties.flags & CL_MEM_HOST_READ_ONLY) && (properties.flags & CL_MEM_HOST_NO_ACCESS)) ||
            ((properties.flags & CL_MEM_HOST_READ_ONLY) && (properties.flags & CL_MEM_HOST_WRITE_ONLY)) ||
            ((properties.flags & CL_MEM_HOST_WRITE_ONLY) && (properties.flags & CL_MEM_HOST_NO_ACCESS))) {
            return false;
        }

        return validateExtraMemoryProperties(properties);
    }

    static AllocationProperties getAllocationProperties(cl_mem_flags_intel flags, bool allocateMemory, size_t size, GraphicsAllocation::AllocationType type);
    static AllocationProperties getAllocationProperties(ImageInfo *imgInfo);

    static DevicesBitfield getDevicesBitfield(const MemoryProperties &properties);

    static bool checkMemFlagsForSubBuffer(cl_mem_flags flags) {
        const cl_mem_flags allValidFlags =
            CL_MEM_READ_WRITE | CL_MEM_WRITE_ONLY | CL_MEM_READ_ONLY |
            CL_MEM_HOST_WRITE_ONLY | CL_MEM_HOST_READ_ONLY | CL_MEM_HOST_NO_ACCESS;

        if ((flags & (~allValidFlags)) != 0) {
            return false;
        }
        return true;
    }

  protected:
    static bool checkMemFlagsForBuffer(const MemoryProperties &properties) {
        MemoryProperties additionalAcceptedProperties;
        addExtraMemoryProperties(additionalAcceptedProperties);

        const cl_mem_flags allValidFlags =
            CL_MEM_READ_WRITE | CL_MEM_WRITE_ONLY | CL_MEM_READ_ONLY |
            CL_MEM_ALLOC_HOST_PTR | CL_MEM_COPY_HOST_PTR | CL_MEM_USE_HOST_PTR |
            CL_MEM_HOST_WRITE_ONLY | CL_MEM_HOST_READ_ONLY | CL_MEM_HOST_NO_ACCESS |
            additionalAcceptedProperties.flags;

        const cl_mem_flags allValidFlagsIntel = CL_MEM_LOCALLY_UNCACHED_RESOURCE |
                                                additionalAcceptedProperties.flags_intel;

        return ((properties.flags & (~allValidFlags)) == 0) &&
               ((properties.flags_intel & (~allValidFlagsIntel)) == 0);
    }

    static bool validateExtraMemoryProperties(const MemoryProperties &properties);

    static void addExtraMemoryProperties(MemoryProperties &properties);
};
} // namespace OCLRT
