/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "runtime/kernel/kernel.h"

#include <algorithm>

namespace NEO {

class SchedulerKernel : public Kernel {
  public:
    static constexpr const char *schedulerName = "SchedulerParallel20";
    friend Kernel;

    ~SchedulerKernel() override = default;

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
    SchedulerKernel(Program *programArg, const KernelInfo &kernelInfoArg, const ClDevice &deviceArg) : Kernel(programArg, kernelInfoArg, deviceArg, true), gws(0) {
        computeGws();
    };

    void computeGws();

    size_t gws;
};

} // namespace NEO
