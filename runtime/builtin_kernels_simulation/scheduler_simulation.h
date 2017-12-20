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
#include <cstdint>
#include <thread>

#include "runtime/builtin_kernels_simulation/opencl_c.h"
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
