/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/command_queue/command_queue.h"
#include "runtime/kernel/kernel.h"

#include "CL/cl.h"
#include "ocl_igc_shared/gtpin/gtpin_ocl_interface.h"

namespace OCLRT {

struct GTPinKernelExec {
    Kernel *pKernel;
    cl_mem gtpinResource;
    CommandQueue *pCommandQueue;
    gtpin::command_buffer_handle_t commandBuffer;
    uint32_t taskCount;
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

} // namespace OCLRT
