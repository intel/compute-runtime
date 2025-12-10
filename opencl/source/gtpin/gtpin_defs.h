/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/command_stream/task_count_helper.h"

#include "opencl/source/kernel/kernel.h"

#include "CL/cl.h"
#include "ocl_igc_shared/gtpin/gtpin_ocl_interface.h"

namespace NEO {

struct GTPinKernelExec {
    Kernel *pKernel;
    cl_mem gtpinResource;
    CommandQueue *pCommandQueue;
    gtpin::command_buffer_handle_t commandBuffer;
    TaskCountType taskCount;
    bool isTaskCountValid;
    bool isResourceResident;

    GTPinKernelExec() {
        pKernel = nullptr;
        gtpinResource = nullptr;
        pCommandQueue = nullptr;
        commandBuffer = nullptr;
        taskCount = 0;
        isTaskCountValid = false;
        isResourceResident = false;
    }
};
typedef struct GTPinKernelExec gtpinkexec_t;

} // namespace NEO
