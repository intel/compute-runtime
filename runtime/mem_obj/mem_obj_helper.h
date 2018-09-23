/*
 * Copyright (C) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "CL/cl.h"
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

        bool flagsValidated = (flags & (~allValidFlags)) == 0;
        flagsValidated |= checkExtraMemFlagsForBuffer(flags & (~allValidFlags));

        return flagsValidated;
    }

    static bool checkExtraMemFlagsForBuffer(cl_mem_flags flags);

    static AllocationFlags getAllocationFlags(cl_mem_flags flags);

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
