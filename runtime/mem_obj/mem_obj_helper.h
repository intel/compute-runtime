/*
 * Copyright (C) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "CL/cl.h"
#include "public/cl_ext_private.h"
#include "runtime/mem_obj/mem_obj.h"
#include "runtime/memory_manager/memory_manager.h"

namespace OCLRT {

class MemObjHelper {
  public:
    static bool checkMemFlagsForBuffer(cl_mem_flags flags) {
        const cl_mem_flags allValidFlags =
            CL_MEM_READ_WRITE | CL_MEM_WRITE_ONLY | CL_MEM_READ_ONLY |
            CL_MEM_ALLOC_HOST_PTR | CL_MEM_COPY_HOST_PTR | CL_MEM_USE_HOST_PTR |
            CL_MEM_HOST_WRITE_ONLY | CL_MEM_HOST_READ_ONLY | CL_MEM_HOST_NO_ACCESS;

        return (flags & (~allValidFlags)) == 0;
    }

    static bool parseMemoryProperties(const cl_mem_properties_intel *properties, MemoryProperties &propertiesStruct);

    static bool validateMemoryProperties(const MemoryProperties &properties) {

        /* Are there some invalid flag bits? */
        if (!MemObjHelper::checkMemFlagsForBuffer(properties.flags)) {
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

    static bool validateExtraMemoryProperties(const MemoryProperties &properties);

    static AllocationFlags getAllocationFlags(cl_mem_flags flags, bool allocateMemory);

    static DevicesBitfield getDevicesBitfield(cl_mem_flags flags);

    static bool checkMemFlagsForSubBuffer(cl_mem_flags flags) {
        const cl_mem_flags allValidFlags =
            CL_MEM_READ_WRITE | CL_MEM_WRITE_ONLY | CL_MEM_READ_ONLY |
            CL_MEM_HOST_WRITE_ONLY | CL_MEM_HOST_READ_ONLY | CL_MEM_HOST_NO_ACCESS;

        if ((flags & (~allValidFlags)) != 0) {
            return false;
        }
        return true;
    }
};
} // namespace OCLRT
