/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/command_stream/linear_stream.h"
#include "shared/source/command_stream/preemption.h"
#include "shared/source/helpers/register_offsets.h"
#include "shared/source/indirect_heap/indirect_heap.h"

#include "opencl/source/command_queue/cl_local_work_size.h"
#include "opencl/source/command_queue/command_queue.h"
#include "opencl/source/helpers/hardware_commands_helper.h"
#include "opencl/source/helpers/task_information.h"

namespace NEO {

struct RootDeviceEnvironment;

template <typename GfxFamily>
using MI_STORE_REG_MEM = typename GfxFamily::MI_STORE_REGISTER_MEM_CMD;

template <typename GfxFamily>
class GpgpuWalkerHelper {
    using WALKER_TYPE = typename GfxFamily::WALKER_TYPE;

  public:
    static void applyWADisableLSQCROPERFforOCL(LinearStream *pCommandStream,
                                               const Kernel &kernel,
                                               bool disablePerfMode);

    static size_t getSizeForWADisableLSQCROPERFforOCL(const Kernel *pKernel);
    static size_t getSizeForWaDisableRccRhwoOptimization(const Kernel *pKernel);

    static size_t setGpgpuWalkerThreadData(
        WALKER_TYPE *walkerCmd,
        const KernelDescriptor &kernelDescriptor,
        const size_t globalOffsets[3],
        const size_t startWorkGroups[3],
        const size_t numWorkGroups[3],
        const size_t localWorkSizesIn[3],
        uint32_t simd,
        uint32_t workDim,
        bool localIdsGenerationByRuntime,
        bool inlineDataProgrammingRequired,
        uint32_t requiredWorkgroupOrder);

    static void dispatchProfilingCommandsStart(
        TagNodeBase &hwTimeStamps,
        LinearStream *commandStream,
        const HardwareInfo &hwInfo);

    static void dispatchProfilingCommandsEnd(
        TagNodeBase &hwTimeStamps,
        LinearStream *commandStream,
        const HardwareInfo &hwInfo);

    static void dispatchPerfCountersCommandsStart(
        CommandQueue &commandQueue,
        TagNodeBase &hwPerfCounter,
        LinearStream *commandStream);

    static void dispatchPerfCountersCommandsEnd(
        CommandQueue &commandQueue,
        TagNodeBase &hwPerfCounter,
        LinearStream *commandStream);

    static void setupTimestampPacket(
        LinearStream *cmdStream,
        WALKER_TYPE *walkerCmd,
        TagNodeBase *timestampPacketNode,
        const RootDeviceEnvironment &rootDeviceEnvironment);

    static void adjustMiStoreRegMemMode(MI_STORE_REG_MEM<GfxFamily> *storeCmd);

  private:
    using PIPE_CONTROL = typename GfxFamily::PIPE_CONTROL;

    static void addAluReadModifyWriteRegister(
        LinearStream *pCommandStream,
        uint32_t aluRegister,
        AluRegisters operation,
        uint32_t mask);
};

template <typename GfxFamily>
struct EnqueueOperation {
    using PIPE_CONTROL = typename GfxFamily::PIPE_CONTROL;
    static size_t getTotalSizeRequiredCS(uint32_t eventType, const CsrDependencies &csrDeps, bool reserveProfilingCmdsSpace, bool reservePerfCounters, bool blitEnqueue, CommandQueue &commandQueue, const MultiDispatchInfo &multiDispatchInfo, bool isMarkerWithProfiling, bool eventsInWaitList);
    static size_t getSizeRequiredCS(uint32_t cmdType, bool reserveProfilingCmdsSpace, bool reservePerfCounters, CommandQueue &commandQueue, const Kernel *pKernel, const DispatchInfo &dispatchInfo);
    static size_t getSizeRequiredForTimestampPacketWrite();
    static size_t getSizeForCacheFlushAfterWalkerCommands(const Kernel &kernel, const CommandQueue &commandQueue);

  private:
    static size_t getSizeRequiredCSKernel(bool reserveProfilingCmdsSpace, bool reservePerfCounters, CommandQueue &commandQueue, const Kernel *pKernel, const DispatchInfo &dispatchInfo);
    static size_t getSizeRequiredCSNonKernel(bool reserveProfilingCmdsSpace, bool reservePerfCounters, CommandQueue &commandQueue);
};

template <typename GfxFamily, uint32_t eventType>
LinearStream &getCommandStream(CommandQueue &commandQueue, const CsrDependencies &csrDeps, bool reserveProfilingCmdsSpace,
                               bool reservePerfCounterCmdsSpace, bool blitEnqueue, const MultiDispatchInfo &multiDispatchInfo,
                               Surface **surfaces, size_t numSurfaces, bool isMarkerWithProfiling, bool eventsInWaitList) {
    size_t expectedSizeCS = EnqueueOperation<GfxFamily>::getTotalSizeRequiredCS(eventType, csrDeps, reserveProfilingCmdsSpace, reservePerfCounterCmdsSpace, blitEnqueue, commandQueue, multiDispatchInfo, isMarkerWithProfiling, eventsInWaitList);
    return commandQueue.getCS(expectedSizeCS);
}

template <typename GfxFamily, IndirectHeap::Type heapType>
IndirectHeap &getIndirectHeap(CommandQueue &commandQueue, const MultiDispatchInfo &multiDispatchInfo) {
    size_t expectedSize = 0;
    IndirectHeap *ih = nullptr;

    // clang-format off
    switch (heapType) {
    case IndirectHeap::Type::DYNAMIC_STATE:   expectedSize = HardwareCommandsHelper<GfxFamily>::getTotalSizeRequiredDSH(multiDispatchInfo); break;
    case IndirectHeap::Type::INDIRECT_OBJECT: expectedSize = HardwareCommandsHelper<GfxFamily>::getTotalSizeRequiredIOH(multiDispatchInfo); break;
    case IndirectHeap::Type::SURFACE_STATE:   expectedSize = HardwareCommandsHelper<GfxFamily>::getTotalSizeRequiredSSH(multiDispatchInfo); break;
    }
    // clang-format on

    if (ih == nullptr)
        ih = &commandQueue.getIndirectHeap(heapType, expectedSize);

    return *ih;
}

} // namespace NEO
