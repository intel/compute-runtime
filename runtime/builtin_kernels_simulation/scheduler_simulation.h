/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "runtime/builtin_kernels_simulation/opencl_c.h"

#include <cstdint>
#include <thread>
namespace OCLRT {
class GraphicsAllocation;
}

namespace BuiltinKernelsSimulation {

extern bool conditionReady;
extern std::thread threads[];

template <typename GfxFamily>
class SchedulerSimulation {
  public:
    void runSchedulerSimulation(OCLRT::GraphicsAllocation *queue,
                                OCLRT::GraphicsAllocation *commandsStack,
                                OCLRT::GraphicsAllocation *eventsPool,
                                OCLRT::GraphicsAllocation *secondaryBatchBuffer,
                                OCLRT::GraphicsAllocation *dsh,
                                OCLRT::GraphicsAllocation *reflectionSurface,
                                OCLRT::GraphicsAllocation *queueStorageBuffer,
                                OCLRT::GraphicsAllocation *ssh,
                                OCLRT::GraphicsAllocation *debugQueue);

    void cleanSchedulerSimulation();

    static void startScheduler(uint32_t index,
                               OCLRT::GraphicsAllocation *queue,
                               OCLRT::GraphicsAllocation *commandsStack,
                               OCLRT::GraphicsAllocation *eventsPool,
                               OCLRT::GraphicsAllocation *secondaryBatchBuffer,
                               OCLRT::GraphicsAllocation *dsh,
                               OCLRT::GraphicsAllocation *reflectionSurface,
                               OCLRT::GraphicsAllocation *queueStorageBuffer,
                               OCLRT::GraphicsAllocation *ssh,
                               OCLRT::GraphicsAllocation *debugQueue);

    void initializeSchedulerSimulation(OCLRT::GraphicsAllocation *queue,
                                       OCLRT::GraphicsAllocation *commandsStack,
                                       OCLRT::GraphicsAllocation *eventsPool,
                                       OCLRT::GraphicsAllocation *secondaryBatchBuffer,
                                       OCLRT::GraphicsAllocation *dsh,
                                       OCLRT::GraphicsAllocation *reflectionSurface,
                                       OCLRT::GraphicsAllocation *queueStorageBuffer,
                                       OCLRT::GraphicsAllocation *ssh,
                                       OCLRT::GraphicsAllocation *debugQueue);

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
