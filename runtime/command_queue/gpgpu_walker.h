/*
 * Copyright (c) 2018, Intel Corporation
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

#include "runtime/built_ins/built_ins.h"
#include "runtime/context/context.h"
#include "runtime/command_queue/command_queue.h"
#include "runtime/command_stream/linear_stream.h"
#include "runtime/command_stream/preemption.h"
#include "runtime/device_queue/device_queue_hw.h"
#include "runtime/event/hw_timestamps.h"
#include "runtime/event/perf_counter.h"
#include "runtime/helpers/dispatch_info.h"
#include "runtime/helpers/kernel_commands.h"
#include "runtime/helpers/task_information.h"
#include "runtime/indirect_heap/indirect_heap.h"
#include "runtime/kernel/kernel.h"
#include "runtime/program/kernel_info.h"
#include "runtime/utilities/vec.h"

namespace OCLRT {

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

template <typename SizeAndAllocCalcT, typename... CalcArgsT>
IndirectHeap *allocateIndirectHeap(SizeAndAllocCalcT &&calc, CalcArgsT &&... args) {
    size_t alignment = MemoryConstants::pageSize;
    size_t size = calc(std::forward<CalcArgsT>(args)...);
    return new IndirectHeap(alignedMalloc(size, alignment), size);
}

template <typename GfxFamily>
class GpgpuWalkerHelper {
  public:
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
        typename GfxFamily::GPGPU_WALKER *pCmd,
        const size_t globalOffsets[3],
        const size_t startWorkGroups[3],
        const size_t numWorkGroups[3],
        const size_t localWorkSizesIn[3],
        uint32_t simd);

    static void dispatchProfilingCommandsStart(
        HwTimeStamps &hwTimeStamps,
        OCLRT::LinearStream *commandStream);

    static void dispatchProfilingCommandsEnd(
        HwTimeStamps &hwTimeStamps,
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

    static void dispatchWalker(
        CommandQueue &commandQueue,
        const MultiDispatchInfo &multiDispatchInfo,
        cl_uint numEventsInWaitList,
        const cl_event *eventWaitList,
        KernelOperation **blockedCommandsData,
        HwTimeStamps *hwTimeStamps,
        OCLRT::HwPerfCounter *hwPerfCounter,
        PreemptionMode preemptionMode,
        bool blockQueue,
        unsigned int commandType = 0);

    static void dispatchWalker(
        CommandQueue &commandQueue,
        const Kernel &kernel,
        cl_uint workDim,
        const size_t globalOffsets[3],
        const size_t workItems[3],
        const size_t *localWorkSizesIn,
        cl_uint numEventsInWaitList,
        const cl_event *eventWaitList,
        KernelOperation **blockedCommandsData,
        HwTimeStamps *hwTimeStamps,
        HwPerfCounter *hwPerfCounter,
        PreemptionMode preemptionMode,
        bool blockQueue);

    static void dispatchScheduler(
        CommandQueue &commandQueue,
        DeviceQueueHw<GfxFamily> &devQueueHw,
        PreemptionMode preemptionMode,
        SchedulerKernel &scheduler);
};

template <typename GfxFamily, uint32_t eventType>
struct EnqueueOperation {
    static_assert(eventType != CL_COMMAND_NDRANGE_KERNEL, "for eventType CL_COMMAND_NDRANGE_KERNEL use specialization class");
    static_assert(eventType != CL_COMMAND_MARKER, "for eventType CL_COMMAND_MARKER use specialization class");
    static_assert(eventType != CL_COMMAND_MIGRATE_MEM_OBJECTS, "for eventType CL_COMMAND_MIGRATE_MEM_OBJECTS use specialization class");
    static size_t getTotalSizeRequiredCS(bool reserveProfilingCmdsSpace, bool reservePerfCounters, CommandQueue &commandQueue, const MultiDispatchInfo &multiDispatchInfo) {
        size_t size = KernelCommandsHelper<GfxFamily>::getSizeRequiredCS() +
                      sizeof(typename GfxFamily::PIPE_CONTROL) * (KernelCommandsHelper<GfxFamily>::isPipeControlWArequired() ? 2 : 1);
        if (reserveProfilingCmdsSpace) {
            size += 2 * sizeof(typename GfxFamily::PIPE_CONTROL) + 4 * sizeof(typename GfxFamily::MI_STORE_REGISTER_MEM);
        }
        if (reservePerfCounters) {
            //start cmds
            //P_C: flush CS & TimeStamp BEGIN
            size += 2 * sizeof(typename GfxFamily::PIPE_CONTROL);
            //SRM NOOPID & Frequency
            size += 2 * sizeof(typename GfxFamily::MI_STORE_REGISTER_MEM);
            //gp registers
            size += OCLRT::INSTR_GENERAL_PURPOSE_COUNTERS_COUNT * sizeof(typename GfxFamily::MI_STORE_REGISTER_MEM);
            //report perf count
            size += sizeof(typename GfxFamily::MI_REPORT_PERF_COUNT);
            //user registers
            size += commandQueue.getPerfCountersUserRegistersNumber() * sizeof(typename GfxFamily::MI_STORE_REGISTER_MEM);

            //end cmds
            //P_C: flush CS & TimeStamp END;
            size += 2 * sizeof(typename GfxFamily::PIPE_CONTROL);
            //OA buffer (status head, tail)
            size += 3 * sizeof(typename GfxFamily::MI_STORE_REGISTER_MEM);
            //report perf count
            size += sizeof(typename GfxFamily::MI_REPORT_PERF_COUNT);
            //gp registers
            size += OCLRT::INSTR_GENERAL_PURPOSE_COUNTERS_COUNT * sizeof(typename GfxFamily::MI_STORE_REGISTER_MEM);
            //SRM NOOPID & Frequency
            size += 2 * sizeof(typename GfxFamily::MI_STORE_REGISTER_MEM);
            //user registers
            size += commandQueue.getPerfCountersUserRegistersNumber() * sizeof(typename GfxFamily::MI_STORE_REGISTER_MEM);
        }
        Device &device = commandQueue.getDevice();
        for (auto &dispatchInfo : multiDispatchInfo) {
            auto &kernel = *dispatchInfo.getKernel();
            size += sizeof(typename GfxFamily::GPGPU_WALKER);
            size += GpgpuWalkerHelper<GfxFamily>::getSizeForWADisableLSQCROPERFforOCL(&kernel);
            size += PreemptionHelper::getPreemptionWaCsSize<GfxFamily>(device);
        }
        return size;
    }

    static size_t getSizeRequiredCS(bool reserveProfilingCmdsSpace, bool reservePerfCounters, CommandQueue &commandQueue, const Kernel *pKernel) {
        size_t size = sizeof(typename GfxFamily::GPGPU_WALKER) + KernelCommandsHelper<GfxFamily>::getSizeRequiredCS() +
                      sizeof(typename GfxFamily::PIPE_CONTROL) * (KernelCommandsHelper<GfxFamily>::isPipeControlWArequired() ? 2 : 1);
        size += PreemptionHelper::getPreemptionWaCsSize<GfxFamily>(commandQueue.getDevice());
        if (reserveProfilingCmdsSpace) {
            size += 2 * sizeof(typename GfxFamily::PIPE_CONTROL) + 2 * sizeof(typename GfxFamily::MI_STORE_REGISTER_MEM);
        }
        if (reservePerfCounters) {
            //start cmds
            //P_C: flush CS & TimeStamp BEGIN
            size += 2 * sizeof(typename GfxFamily::PIPE_CONTROL);
            //SRM NOOPID & Frequency
            size += 2 * sizeof(typename GfxFamily::MI_STORE_REGISTER_MEM);
            //gp registers
            size += OCLRT::INSTR_GENERAL_PURPOSE_COUNTERS_COUNT * sizeof(typename GfxFamily::MI_STORE_REGISTER_MEM);
            //report perf count
            size += sizeof(typename GfxFamily::MI_REPORT_PERF_COUNT);
            //user registers
            size += commandQueue.getPerfCountersUserRegistersNumber() * sizeof(typename GfxFamily::MI_STORE_REGISTER_MEM);

            //end cmds
            //P_C: flush CS & TimeStamp END;
            size += 2 * sizeof(typename GfxFamily::PIPE_CONTROL);
            //OA buffer (status head, tail)
            size += 3 * sizeof(typename GfxFamily::MI_STORE_REGISTER_MEM);
            //report perf count
            size += sizeof(typename GfxFamily::MI_REPORT_PERF_COUNT);
            //gp registers
            size += OCLRT::INSTR_GENERAL_PURPOSE_COUNTERS_COUNT * sizeof(typename GfxFamily::MI_STORE_REGISTER_MEM);
            //SRM NOOPID & Frequency
            size += 2 * sizeof(typename GfxFamily::MI_STORE_REGISTER_MEM);
            //user registers
            size += commandQueue.getPerfCountersUserRegistersNumber() * sizeof(typename GfxFamily::MI_STORE_REGISTER_MEM);
        }
        size += GpgpuWalkerHelper<GfxFamily>::getSizeForWADisableLSQCROPERFforOCL(pKernel);

        return size;
    }
};

template <typename GfxFamily, uint32_t eventType>
LinearStream &getCommandStream(CommandQueue &commandQueue, bool reserveProfilingCmdsSpace, bool reservePerfCounterCmdsSpace, const Kernel *pKernel) {
    auto expectedSizeCS = EnqueueOperation<GfxFamily, eventType>::getSizeRequiredCS(reserveProfilingCmdsSpace, reservePerfCounterCmdsSpace, commandQueue, pKernel);
    return commandQueue.getCS(expectedSizeCS);
}

template <typename GfxFamily, uint32_t eventType>
LinearStream &getCommandStream(CommandQueue &commandQueue, bool reserveProfilingCmdsSpace, bool reservePerfCounterCmdsSpace, const MultiDispatchInfo &multiDispatchInfo) {
    size_t expectedSizeCS = 0;
    Kernel *parentKernel = multiDispatchInfo.size() > 0 ? multiDispatchInfo.begin()->getKernel() : nullptr;
    for (auto &dispatchInfo : multiDispatchInfo) {
        expectedSizeCS += EnqueueOperation<GfxFamily, eventType>::getSizeRequiredCS(reserveProfilingCmdsSpace, reservePerfCounterCmdsSpace, commandQueue, dispatchInfo.getKernel());
    }
    if (parentKernel && parentKernel->isParentKernel) {
        SchedulerKernel &scheduler = BuiltIns::getInstance().getSchedulerKernel(parentKernel->getContext());
        expectedSizeCS += EnqueueOperation<GfxFamily, eventType>::getSizeRequiredCS(reserveProfilingCmdsSpace, reservePerfCounterCmdsSpace, commandQueue, &scheduler);
    }
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

    if (multiDispatchInfo.begin()->getKernel()->isParentKernel) {
        if (heapType == IndirectHeap::SURFACE_STATE) {
            expectedSize += KernelCommandsHelper<GfxFamily>::template getSizeRequiredForExecutionModel<heapType>(const_cast<const Kernel &>(*(multiDispatchInfo.begin()->getKernel())));
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
