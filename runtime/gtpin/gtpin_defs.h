/*
 * Copyright (c) 2018, Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#include "ocl_igc_shared/gtpin/gtpin_ocl_interface.h"
#include "CL/cl.h"
#include "runtime/command_queue/command_queue.h"
#include "runtime/kernel/kernel.h"

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
