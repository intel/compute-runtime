/*
* Copyright (c) 2017 - 2018, Intel Corporation
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

#include "CL/cl.h"
#include "runtime/builtin_kernels_simulation/opencl_c.h"
#include "runtime/builtin_kernels_simulation/scheduler_simulation.h"
#include "runtime/builtin_kernels_simulation/scheduler_simulation.inl"
#include "runtime/execution_model/device_enqueue.h"
#include "runtime/gen8/hw_cmds.h"
#include "runtime/memory_manager/graphics_allocation.h"

using namespace OCLRT;
using namespace BuiltinKernelsSimulation;

namespace Gen8SchedulerSimulation {

#define SCHEDULER_EMULATION

uint GetNextPowerof2(uint number);

float __intel__getProfilingTimerResolution() {
    return static_cast<float>(DEFAULT_GEN8_PLATFORM::hwInfo.capabilityTable.defaultProfilingTimerResolution);
}

#include "runtime/gen8/device_enqueue.h"
#include "runtime/gen8/scheduler_definitions.h"
#include "runtime/gen8/scheduler_igdrcl_built_in.inl"
#include "runtime/scheduler/scheduler.cl"
} // namespace Gen8SchedulerSimulation

namespace BuiltinKernelsSimulation {

template <>
void SchedulerSimulation<BDWFamily>::startScheduler(uint32_t index,
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

    Gen8SchedulerSimulation::SchedulerParallel20((IGIL_CommandQueue *)queue->getUnderlyingBuffer(),
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
void SchedulerSimulation<BDWFamily>::patchGpGpuWalker(uint secondLevelBatchOffset,
                                                      __global uint *secondaryBatchBuffer,
                                                      uint interfaceDescriptorOffset,
                                                      uint simdSize,
                                                      uint totalLocalWorkSize,
                                                      uint3 dimSize,
                                                      uint3 startPoint,
                                                      uint numberOfHwThreadsPerWg,
                                                      uint indirectPayloadSize,
                                                      uint ioHoffset) {
    Gen8SchedulerSimulation::patchGpGpuWalker(secondLevelBatchOffset,
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

template class SchedulerSimulation<BDWFamily>;

} // namespace BuiltinKernelsSimulation
