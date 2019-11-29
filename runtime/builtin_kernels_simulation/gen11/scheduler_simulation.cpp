/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/builtin_kernels_simulation/scheduler_simulation.h"

#include "core/gen11/hw_cmds.h"
#include "core/memory_manager/graphics_allocation.h"
#include "runtime/builtin_kernels_simulation/opencl_c.h"
#include "runtime/builtin_kernels_simulation/scheduler_simulation.inl"
#include "runtime/execution_model/device_enqueue.h"

#include "CL/cl.h"

using namespace NEO;
using namespace BuiltinKernelsSimulation;

namespace Gen11SchedulerSimulation {

#define SCHEDULER_EMULATION

uint GetNextPowerof2(uint number);

float __intel__getProfilingTimerResolution() {
    return static_cast<float>(DEFAULT_GEN11_PLATFORM::hwInfo.capabilityTable.defaultProfilingTimerResolution);
}

#include "runtime/gen11/device_enqueue.h"
#include "runtime/gen11/scheduler_definitions.h"
#include "runtime/gen11/scheduler_igdrcl_built_in.inl"
#include "runtime/scheduler/scheduler.cl"
} // namespace Gen11SchedulerSimulation

namespace BuiltinKernelsSimulation {

template <>
void SchedulerSimulation<ICLFamily>::startScheduler(uint32_t index,
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

    Gen11SchedulerSimulation::SchedulerParallel20((IGIL_CommandQueue *)queue->getUnderlyingBuffer(),
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
void SchedulerSimulation<ICLFamily>::patchGpGpuWalker(uint secondLevelBatchOffset,
                                                      __global uint *secondaryBatchBuffer,
                                                      uint interfaceDescriptorOffset,
                                                      uint simdSize,
                                                      uint totalLocalWorkSize,
                                                      uint3 dimSize,
                                                      uint3 startPoint,
                                                      uint numberOfHwThreadsPerWg,
                                                      uint indirectPayloadSize,
                                                      uint ioHoffset) {
    Gen11SchedulerSimulation::patchGpGpuWalker(secondLevelBatchOffset,
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

template class SchedulerSimulation<ICLFamily>;

} // namespace BuiltinKernelsSimulation
