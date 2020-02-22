/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "opencl/source/builtin_kernels_simulation/opencl_c.h"

#include <cstdint>
#include <thread>
namespace NEO {
class GraphicsAllocation;
}

namespace BuiltinKernelsSimulation {

extern bool conditionReady;
extern std::thread threads[];

template <typename GfxFamily>
class SchedulerSimulation {
  public:
    void runSchedulerSimulation(NEO::GraphicsAllocation *queue,
                                NEO::GraphicsAllocation *commandsStack,
                                NEO::GraphicsAllocation *eventsPool,
                                NEO::GraphicsAllocation *secondaryBatchBuffer,
                                NEO::GraphicsAllocation *dsh,
                                NEO::GraphicsAllocation *reflectionSurface,
                                NEO::GraphicsAllocation *queueStorageBuffer,
                                NEO::GraphicsAllocation *ssh,
                                NEO::GraphicsAllocation *debugQueue);

    void cleanSchedulerSimulation();

    static void startScheduler(uint32_t index,
                               NEO::GraphicsAllocation *queue,
                               NEO::GraphicsAllocation *commandsStack,
                               NEO::GraphicsAllocation *eventsPool,
                               NEO::GraphicsAllocation *secondaryBatchBuffer,
                               NEO::GraphicsAllocation *dsh,
                               NEO::GraphicsAllocation *reflectionSurface,
                               NEO::GraphicsAllocation *queueStorageBuffer,
                               NEO::GraphicsAllocation *ssh,
                               NEO::GraphicsAllocation *debugQueue);

    void initializeSchedulerSimulation(NEO::GraphicsAllocation *queue,
                                       NEO::GraphicsAllocation *commandsStack,
                                       NEO::GraphicsAllocation *eventsPool,
                                       NEO::GraphicsAllocation *secondaryBatchBuffer,
                                       NEO::GraphicsAllocation *dsh,
                                       NEO::GraphicsAllocation *reflectionSurface,
                                       NEO::GraphicsAllocation *queueStorageBuffer,
                                       NEO::GraphicsAllocation *ssh,
                                       NEO::GraphicsAllocation *debugQueue);

    static void patchGpGpuWalker(uint secondLevelBatchOffset,
                                 __global uint *secondaryBatchBuffer,
                                 uint interfaceDescriptorOffset,
                                 uint simdSize,
                                 uint totalLocalWorkSize,
                                 uint3 dimSize,
                                 uint3 startPoint,
                                 uint numberOfHwThreadsPerWg,
                                 uint indirectPayloadSize,
                                 uint ioHoffset);
    static bool enabled;
    static bool simulationRun;
};

template <typename GfxFamily>
bool SchedulerSimulation<GfxFamily>::enabled = true;

template <typename GfxFamily>
bool SchedulerSimulation<GfxFamily>::simulationRun = false;

} // namespace BuiltinKernelsSimulation
