/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "runtime/built_ins/built_ins.h"
#include "runtime/command_queue/command_queue.h"
#include "runtime/command_stream/linear_stream.h"
#include "runtime/command_stream/preemption.h"
#include "runtime/context/context.h"
#include "runtime/device_queue/device_queue_hw.h"
#include "runtime/event/hw_timestamps.h"
#include "runtime/event/perf_counter.h"
#include "runtime/helpers/dispatch_info.h"
#include "runtime/helpers/kernel_commands.h"
#include "runtime/helpers/task_information.h"
#include "runtime/helpers/timestamp_packet.h"
#include "runtime/indirect_heap/indirect_heap.h"
#include "runtime/kernel/kernel.h"
#include "runtime/program/kernel_info.h"
#include "runtime/utilities/tag_allocator.h"
#include "runtime/utilities/vec.h"

namespace OCLRT {

template <typename GfxFamily>
using WALKER_TYPE = typename GfxFamily::WALKER_TYPE;
template <typename GfxFamily>
using MI_STORE_REG_MEM = typename GfxFamily::MI_STORE_REGISTER_MEM_CMD;

constexpr int32_t NUM_ALU_INST_FOR_READ_MODIFY_WRITE = 4;

constexpr int32_t L3SQC_BIT_LQSC_RO_PERF_DIS = 0x08000000;
constexpr int32_t L3SQC_REG4 = 0xB118;

constexpr int32_t GPGPU_WALKER_COOKIE_VALUE_BEFORE_WALKER = 0xFFFFFFFF;
constexpr int32_t GPGPU_WALKER_COOKIE_VALUE_AFTER_WALKER = 0x00000000;

constexpr int32_t CS_GPR_R0 = 0x2600;
constexpr int32_t CS_GPR_R1 = 0x2608;

constexpr int32_t ALU_OPCODE_LOAD = 0x080;
constexpr int32_t ALU_OPCODE_STORE = 0x180;
constexpr int32_t ALU_OPCODE_OR = 0x103;
constexpr int32_t ALU_OPCODE_AND = 0x102;

constexpr int32_t ALU_REGISTER_R_0 = 0x0;
constexpr int32_t ALU_REGISTER_R_1 = 0x1;
constexpr int32_t ALU_REGISTER_R_SRCA = 0x20;
constexpr int32_t ALU_REGISTER_R_SRCB = 0x21;
constexpr int32_t ALU_REGISTER_R_ACCU = 0x31;

constexpr uint32_t GP_THREAD_TIME_REG_ADDRESS_OFFSET_LOW = 0x23A8;

void computeWorkgroupSize1D(
    uint32_t maxWorkGroupSize,
    size_t workGroupSize[3],
    const size_t workItems[3],
    size_t simdSize);

void computeWorkgroupSizeND(
    WorkSizeInfo wsInfo,
    size_t workGroupSize[3],
    const size_t workItems[3],
    const uint32_t workDim);

void computeWorkgroupSize2D(
    uint32_t maxWorkGroupSize,
    size_t workGroupSize[3],
    const size_t workItems[3],
    size_t simdSize);

void computeWorkgroupSizeSquared(
    uint32_t maxWorkGroupSize,
    size_t workGroupSize[3],
    const size_t workItems[3],
    size_t simdSize,
    const uint32_t workDim);

Vec3<size_t> computeWorkgroupSize(
    const DispatchInfo &dispatchInfo);

Vec3<size_t> generateWorkgroupSize(
    const DispatchInfo &dispatchInfo);

Vec3<size_t> computeWorkgroupsNumber(
    const Vec3<size_t> gws,
    const Vec3<size_t> lws);

Vec3<size_t> generateWorkgroupsNumber(
    const Vec3<size_t> gws,
    const Vec3<size_t> lws);

Vec3<size_t> generateWorkgroupsNumber(
    const DispatchInfo &dispatchInfo);

inline uint32_t calculateDispatchDim(Vec3<size_t> dispatchSize, Vec3<size_t> dispatchOffset) {
    return std::max(1U, std::max(dispatchSize.getSimplifiedDim(), dispatchOffset.getSimplifiedDim()));
}

Vec3<size_t> canonizeWorkgroup(
    Vec3<size_t> workgroup);

void provideLocalWorkGroupSizeHints(Context *context, uint32_t maxWorkGroupSize, DispatchInfo dispatchInfo);

inline cl_uint computeDimensions(const size_t workItems[3]) {
    return (workItems[2] > 1) ? 3 : (workItems[1] > 1) ? 2 : 1;
}

template <typename GfxFamily>
class GpgpuWalkerHelper {
  public:
    using PIPE_CONTROL = typename GfxFamily::PIPE_CONTROL;
    using INTERFACE_DESCRIPTOR_DATA = typename GfxFamily::INTERFACE_DESCRIPTOR_DATA;

    static void addAluReadModifyWriteRegister(
        LinearStream *pCommandStream,
        uint32_t aluRegister,
        uint32_t operation,
        uint32_t mask);

    static void applyWADisableLSQCROPERFforOCL(LinearStream *pCommandStream,
                                               const Kernel &kernel,
                                               bool disablePerfMode);

    static size_t getSizeForWADisableLSQCROPERFforOCL(const Kernel *pKernel);

    static size_t setGpgpuWalkerThreadData(
        WALKER_TYPE<GfxFamily> *walkerCmd,
        const size_t globalOffsets[3],
        const size_t startWorkGroups[3],
        const size_t numWorkGroups[3],
        const size_t localWorkSizesIn[3],
        uint32_t simd,
        uint32_t workDim,
        bool localIdsGenerationByRuntime,
        bool inlineDataProgrammingRequired,
        const iOpenCL::SPatchThreadPayload &threadPayload);

    static void dispatchProfilingCommandsStart(
        TagNode<HwTimeStamps> &hwTimeStamps,
        OCLRT::LinearStream *commandStream);

    static void dispatchProfilingCommandsEnd(
        TagNode<HwTimeStamps> &hwTimeStamps,
        OCLRT::LinearStream *commandStream);

    static void dispatchPerfCountersNoopidRegisterCommands(
        CommandQueue &commandQueue,
        OCLRT::HwPerfCounter &hwPerfCounter,
        OCLRT::LinearStream *commandStream,
        bool start);

    static void dispatchPerfCountersReadFreqRegisterCommands(
        CommandQueue &commandQueue,
        OCLRT::HwPerfCounter &hwPerfCounter,
        OCLRT::LinearStream *commandStream,
        bool start);

    static void dispatchPerfCountersGeneralPurposeCounterCommands(
        CommandQueue &commandQueue,
        OCLRT::HwPerfCounter &hwPerfCounter,
        OCLRT::LinearStream *commandStream,
        bool start);

    static void dispatchPerfCountersUserCounterCommands(
        CommandQueue &commandQueue,
        OCLRT::HwPerfCounter &hwPerfCounter,
        OCLRT::LinearStream *commandStream,
        bool start);

    static void dispatchPerfCountersOABufferStateCommands(
        CommandQueue &commandQueue,
        OCLRT::HwPerfCounter &hwPerfCounter,
        OCLRT::LinearStream *commandStream);

    static void dispatchPerfCountersCommandsStart(
        CommandQueue &commandQueue,
        OCLRT::HwPerfCounter &hwPerfCounter,
        OCLRT::LinearStream *commandStream);

    static void dispatchPerfCountersCommandsEnd(
        CommandQueue &commandQueue,
        OCLRT::HwPerfCounter &hwPerfCounter,
        OCLRT::LinearStream *commandStream);

    static void setupTimestampPacket(
        LinearStream *cmdStream,
        WALKER_TYPE<GfxFamily> *walkerCmd,
        TagNode<TimestampPacket> *timestampPacketNode,
        TimestampPacket::WriteOperationType writeOperationType);

    static void dispatchScheduler(
        CommandQueue &commandQueue,
        LinearStream &commandStream,
        DeviceQueueHw<GfxFamily> &devQueueHw,
        PreemptionMode preemptionMode,
        SchedulerKernel &scheduler,
        IndirectHeap *ssh,
        IndirectHeap *dsh);

    static void adjustMiStoreRegMemMode(MI_STORE_REG_MEM<GfxFamily> *storeCmd);
};

template <typename GfxFamily>
struct EnqueueOperation {
    using PIPE_CONTROL = typename GfxFamily::PIPE_CONTROL;
    static size_t getTotalSizeRequiredCS(uint32_t eventType, const CsrDependencies &csrDeps, bool reserveProfilingCmdsSpace, bool reservePerfCounters, CommandQueue &commandQueue, const MultiDispatchInfo &multiDispatchInfo);
    static size_t getSizeRequiredCS(uint32_t cmdType, bool reserveProfilingCmdsSpace, bool reservePerfCounters, CommandQueue &commandQueue, const Kernel *pKernel);
    static size_t getSizeRequiredForTimestampPacketWrite();

  private:
    static size_t getSizeRequiredCSKernel(bool reserveProfilingCmdsSpace, bool reservePerfCounters, CommandQueue &commandQueue, const Kernel *pKernel);
    static size_t getSizeRequiredCSNonKernel(bool reserveProfilingCmdsSpace, bool reservePerfCounters, CommandQueue &commandQueue);
};

template <typename GfxFamily, uint32_t eventType>
LinearStream &getCommandStream(CommandQueue &commandQueue, bool reserveProfilingCmdsSpace, bool reservePerfCounterCmdsSpace, const Kernel *pKernel) {
    auto expectedSizeCS = EnqueueOperation<GfxFamily>::getSizeRequiredCS(eventType, reserveProfilingCmdsSpace, reservePerfCounterCmdsSpace, commandQueue, pKernel);
    return commandQueue.getCS(expectedSizeCS);
}

template <typename GfxFamily, uint32_t eventType>
LinearStream &getCommandStream(CommandQueue &commandQueue, const CsrDependencies &csrDeps, bool reserveProfilingCmdsSpace, bool reservePerfCounterCmdsSpace, const MultiDispatchInfo &multiDispatchInfo) {
    size_t expectedSizeCS = EnqueueOperation<GfxFamily>::getTotalSizeRequiredCS(eventType, csrDeps, reserveProfilingCmdsSpace, reservePerfCounterCmdsSpace, commandQueue, multiDispatchInfo);
    return commandQueue.getCS(expectedSizeCS);
}

template <typename GfxFamily, IndirectHeap::Type heapType>
IndirectHeap &getIndirectHeap(CommandQueue &commandQueue, const MultiDispatchInfo &multiDispatchInfo) {
    size_t expectedSize = 0;
    IndirectHeap *ih = nullptr;

    // clang-format off
    switch (heapType) {
    case IndirectHeap::DYNAMIC_STATE:   expectedSize = KernelCommandsHelper<GfxFamily>::getTotalSizeRequiredDSH(multiDispatchInfo); break;
    case IndirectHeap::INDIRECT_OBJECT: expectedSize = KernelCommandsHelper<GfxFamily>::getTotalSizeRequiredIOH(multiDispatchInfo); break;
    case IndirectHeap::SURFACE_STATE:   expectedSize = KernelCommandsHelper<GfxFamily>::getTotalSizeRequiredSSH(multiDispatchInfo); break;
    }
    // clang-format on

    if (Kernel *parentKernel = multiDispatchInfo.peekParentKernel()) {
        if (heapType == IndirectHeap::SURFACE_STATE) {
            expectedSize += KernelCommandsHelper<GfxFamily>::template getSizeRequiredForExecutionModel<heapType>(*parentKernel);
        } else //if (heapType == IndirectHeap::DYNAMIC_STATE || heapType == IndirectHeap::INDIRECT_OBJECT)
        {
            DeviceQueueHw<GfxFamily> *pDevQueue = castToObject<DeviceQueueHw<GfxFamily>>(commandQueue.getContext().getDefaultDeviceQueue());
            DEBUG_BREAK_IF(pDevQueue == nullptr);
            ih = pDevQueue->getIndirectHeap(IndirectHeap::DYNAMIC_STATE);
        }
    }

    if (ih == nullptr)
        ih = &commandQueue.getIndirectHeap(heapType, expectedSize);

    return *ih;
}

} // namespace OCLRT
