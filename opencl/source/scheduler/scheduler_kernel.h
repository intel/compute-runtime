/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/built_ins/built_ins.h"

#include "opencl/source/kernel/kernel.h"

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
        size_t crossThreadDataSize = kernelInfo.kernelDescriptor.kernelAttributes.crossThreadDataSize;
        size_t dshSize = kernelInfo.heapInfo.DynamicStateHeapSize;

        crossThreadDataSize = alignUp(crossThreadDataSize, 64);
        dshSize = alignUp(dshSize, 64);

        return alignUp(SCHEDULER_DYNAMIC_PAYLOAD_SIZE, 64) + crossThreadDataSize + dshSize;
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
    static BuiltinCode loadSchedulerKernel(Device *device);

  protected:
    SchedulerKernel(Program *programArg, const KernelInfo &kernelInfoArg, ClDevice &clDeviceArg) : Kernel(programArg, kernelInfoArg, clDeviceArg, true) {
        computeGws();
    };

    void computeGws();

    size_t gws = 0u;
};

} // namespace NEO
