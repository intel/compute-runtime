/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/gen9/hw_cmds.h"
#include "core/memory_manager/graphics_allocation.h"

#include "CL/cl.h"
#include "builtin_kernels_simulation/opencl_c.h"
#include "builtin_kernels_simulation/scheduler_simulation.h"
#include "builtin_kernels_simulation/scheduler_simulation.inl"
#include "execution_model/device_enqueue.h"

using namespace NEO;
using namespace BuiltinKernelsSimulation;

namespace NEO {
struct SKLFamily;
}

namespace Gen9SchedulerSimulation {

#define SCHEDULER_EMULATION

float __intel__getProfilingTimerResolution() {
    return static_cast<float>(DEFAULT_GEN9_PLATFORM::hwInfo.capabilityTable.defaultProfilingTimerResolution);
}

#include "gen9/device_enqueue.h"
#include "gen9/scheduler_builtin_kernel.inl"
#include "scheduler/scheduler.cl"
} // namespace Gen9SchedulerSimulation

namespace BuiltinKernelsSimulation {

template <>
void SchedulerSimulation<SKLFamily>::startScheduler(uint32_t index,
                                                    GraphicsAllocation *queue,
                                                    GraphicsAllocation *commandsStack,
                                                    GraphicsAllocation *eventsPool,
                                                    GraphicsAllocation *secondaryBatchBuffer,
                                                    GraphicsAllocation *dsh,
                                                    GraphicsAllocation *reflectionSurface,
                                                    GraphicsAllocation *queueStorageBuffer,
                                                    GraphicsAllocation *ssh,
                                                    GraphicsAllocation *debugQueue) {

    threadIDToLocalIDmap.insert(std::make_pair(std::this_thread::get_id(), index));

    while (!conditionReady) {
    }

    Gen9SchedulerSimulation::SchedulerParallel20((IGIL_CommandQueue *)queue->getUnderlyingBuffer(),
                                                 (uint *)commandsStack->getUnderlyingBuffer(),
                                                 (IGIL_EventPool *)eventsPool->getUnderlyingBuffer(),
                                                 (uint *)secondaryBatchBuffer->getUnderlyingBuffer(),
                                                 (char *)dsh->getUnderlyingBuffer(),
                                                 (IGIL_KernelDataHeader *)reflectionSurface->getUnderlyingBuffer(),
                                                 (uint *)queueStorageBuffer->getUnderlyingBuffer(),
                                                 (char *)ssh->getUnderlyingBuffer(),
                                                 debugQueue != nullptr ? (DebugDataBuffer *)debugQueue->getUnderlyingBuffer() : nullptr);
}
template <>
void SchedulerSimulation<SKLFamily>::patchGpGpuWalker(uint secondLevelBatchOffset,
                                                      __global uint *secondaryBatchBuffer,
                                                      uint interfaceDescriptorOffset,
                                                      uint simdSize,
                                                      uint totalLocalWorkSize,
                                                      uint3 dimSize,
                                                      uint3 startPoint,
                                                      uint numberOfHwThreadsPerWg,
                                                      uint indirectPayloadSize,
                                                      uint ioHoffset) {
    Gen9SchedulerSimulation::patchGpGpuWalker(secondLevelBatchOffset,
                                              secondaryBatchBuffer,
                                              interfaceDescriptorOffset,
                                              simdSize,
                                              totalLocalWorkSize,
                                              dimSize,
                                              startPoint,
                                              numberOfHwThreadsPerWg,
                                              indirectPayloadSize,
                                              ioHoffset);
}
template class SchedulerSimulation<SKLFamily>;
} // namespace BuiltinKernelsSimulation
