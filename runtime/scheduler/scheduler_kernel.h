/*
 * Copyright (c) 2017, Intel Corporation
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

#pragma once
#include "runtime/kernel/kernel.h"

#include <algorithm>

namespace OCLRT {

class SchedulerKernel : public Kernel {
  public:
    static constexpr const char *schedulerName = "SchedulerParallel20";
    friend Kernel;

    size_t getLws() {
        return PARALLEL_SCHEDULER_HWTHREADS_IN_HW_GROUP20 * PARALLEL_SCHEDULER_COMPILATION_SIZE_20;
    }

    size_t getGws() {
        return gws;
    }

    void setGws(size_t newGws) {
        gws = newGws;
    }

    size_t getCurbeSize() {
        size_t crossTrheadDataSize = kernelInfo.patchInfo.dataParameterStream ? kernelInfo.patchInfo.dataParameterStream->DataParameterStreamSize : 0;
        size_t dshSize = kernelInfo.heapInfo.pKernelHeader ? kernelInfo.heapInfo.pKernelHeader->DynamicStateHeapSize : 0;

        crossTrheadDataSize = alignUp(crossTrheadDataSize, 64);
        dshSize = alignUp(dshSize, 64);

        return alignUp(SCHEDULER_DYNAMIC_PAYLOAD_SIZE, 64) + crossTrheadDataSize + dshSize;
    }

    void setArgs(GraphicsAllocation *queue,
                 GraphicsAllocation *commandsStack,
                 GraphicsAllocation *eventsPool,
                 GraphicsAllocation *secondaryBatchBuffer,
                 GraphicsAllocation *dsh,
                 GraphicsAllocation *reflectionSurface,
                 GraphicsAllocation *queueStorageBuffer,
                 GraphicsAllocation *ssh,
                 GraphicsAllocation *debugQueue = nullptr);

  protected:
    SchedulerKernel(Program *programArg, const KernelInfo &kernelInfoArg, const Device &deviceArg) : Kernel(programArg, kernelInfoArg, deviceArg, true), gws(0) {
        computeGws();
    };

    void computeGws();

    size_t gws;
};

} // namespace OCLRT
