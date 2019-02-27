/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/builtin_kernels_simulation/scheduler_simulation.h"

#include "runtime/builtin_kernels_simulation/opencl_c.h"
#include "runtime/builtin_kernels_simulation/scheduler_simulation.inl"
#include "runtime/execution_model/device_enqueue.h"
#include "runtime/gen10/hw_cmds.h"
#include "runtime/memory_manager/graphics_allocation.h"

#include "CL/cl.h"

using namespace OCLRT;
using namespace BuiltinKernelsSimulation;

namespace Gen10SchedulerSimulation {

#define SCHEDULER_EMULATION

uint GetNextPowerof2(uint number);

float __intel__getProfilingTimerResolution() {
    return static_cast<float>(DEFAULT_GEN10_PLATFORM::hwInfo.capabilityTable.defaultProfilingTimerResolution);
}

#include "runtime/gen10/device_enqueue.h"
#include "runtime/gen10/scheduler_definitions.h"
#include "runtime/gen10/scheduler_igdrcl_built_in.inl"
#include "runtime/scheduler/scheduler.cl"
} // namespace Gen10SchedulerSimulation

namespace BuiltinKernelsSimulation {

template <>
void SchedulerSimulation<CNLFamily>::startScheduler(uint32_t index,
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

    Gen10SchedulerSimulation::SchedulerParallel20((IGIL_CommandQueue *)queue->getUnderlyingBuffer(),
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
void SchedulerSimulation<CNLFamily>::patchGpGpuWalker(uint secondLevelBatchOffset,
                                                      __global uint *secondaryBatchBuffer,
                                                      uint interfaceDescriptorOffset,
                                                      uint simdSize,
                                                      uint totalLocalWorkSize,
                                                      uint3 dimSize,
                                                      uint3 startPoint,
                                                      uint numberOfHwThreadsPerWg,
                                                      uint indirectPayloadSize,
                                                      uint ioHoffset) {
    Gen10SchedulerSimulation::patchGpGpuWalker(secondLevelBatchOffset,
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

template class SchedulerSimulation<CNLFamily>;

} // namespace BuiltinKernelsSimulation
