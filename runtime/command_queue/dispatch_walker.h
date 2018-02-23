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
#include "runtime/context/context.h"
#include "runtime/command_queue/local_id_gen.h"
#include "runtime/command_queue/command_queue.h"
#include "runtime/command_queue/dispatch_walker_helper.h"
#include "runtime/command_stream/command_stream_receiver.h"
#include "runtime/command_stream/preemption.h"
#include "runtime/device/device_info.h"
#include "runtime/device_queue/device_queue_hw.h"
#include "runtime/event/perf_counter.h"
#include "runtime/event/user_event.h"
#include "runtime/indirect_heap/indirect_heap.h"
#include "runtime/helpers/aligned_memory.h"
#include "runtime/helpers/debug_helpers.h"
#include "runtime/helpers/kernel_commands.h"
#include "runtime/helpers/task_information.h"
#include "runtime/helpers/validators.h"
#include "runtime/helpers/dispatch_info.h"
#include "runtime/kernel/kernel.h"
#include "runtime/mem_obj/mem_obj.h"
#include "runtime/memory_manager/graphics_allocation.h"
#include <algorithm>
#include <cmath>

namespace OCLRT {

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

Vec3<size_t> canonizeWorkgroup(
    Vec3<size_t> workgroup);

inline uint32_t calculateDispatchDim(Vec3<size_t> dispatchSize, Vec3<size_t> dispatchOffset) {
    return std::max(1U, std::max(dispatchSize.getSimplifiedDim(), dispatchOffset.getSimplifiedDim()));
}

template <typename GfxFamily>
inline size_t setGpgpuWalkerThreadData(
    typename GfxFamily::GPGPU_WALKER *pCmd,
    const size_t globalOffsets[3],
    const size_t startWorkGroups[3],
    const size_t numWorkGroups[3],
    const size_t localWorkSizesIn[3],
    uint32_t simd) {
    typedef typename GfxFamily::GPGPU_WALKER GPGPU_WALKER;

    auto localWorkSize = localWorkSizesIn[0] * localWorkSizesIn[1] * localWorkSizesIn[2];

    auto threadsPerWorkGroup = getThreadsPerWG(simd, localWorkSize);
    pCmd->setThreadWidthCounterMaximum((uint32_t)threadsPerWorkGroup);

    pCmd->setThreadGroupIdXDimension((uint32_t)numWorkGroups[0]);
    pCmd->setThreadGroupIdYDimension((uint32_t)numWorkGroups[1]);
    pCmd->setThreadGroupIdZDimension((uint32_t)numWorkGroups[2]);

    // compute RightExecutionMask
    auto remainderSimdLanes = localWorkSize & (simd - 1);
    uint64_t executionMask = (1ull << remainderSimdLanes) - 1;
    if (!executionMask)
        executionMask = ~executionMask;

    pCmd->setRightExecutionMask((uint32_t)executionMask);

    pCmd->setBottomExecutionMask((uint32_t)0xffffffff);
    pCmd->setSimdSize((typename GPGPU_WALKER::SIMD_SIZE)(simd >> 4));

    pCmd->setThreadGroupIdStartingX((uint32_t)startWorkGroups[0]);
    pCmd->setThreadGroupIdStartingY((uint32_t)startWorkGroups[1]);
    pCmd->setThreadGroupIdStartingResumeZ((uint32_t)startWorkGroups[2]);

    return localWorkSize;
}

inline cl_uint computeDimensions(const size_t workItems[3]) {
    return (workItems[2] > 1) ? 3 : (workItems[1] > 1) ? 2 : 1;
}

void provideLocalWorkGroupSizeHints(Context *context, uint32_t maxWorkGroupSize, DispatchInfo dispatchInfo);

template <typename SizeAndAllocCalcT, typename... CalcArgsT>
IndirectHeap *allocateIndirectHeap(SizeAndAllocCalcT &&calc, CalcArgsT &&... args) {
    size_t alignment = MemoryConstants::pageSize;
    size_t size = calc(std::forward<CalcArgsT>(args)...);
    return new IndirectHeap(alignedMalloc(size, alignment), size);
}

template <typename GfxFamily>
void dispatchProfilingCommandsStart(
    HwTimeStamps &hwTimeStamps,
    OCLRT::LinearStream *commandStream) {
    using MI_STORE_REGISTER_MEM = typename GfxFamily::MI_STORE_REGISTER_MEM;
    using PIPE_CONTROL = typename GfxFamily::PIPE_CONTROL;

    // PIPE_CONTROL for global timestamp
    uint64_t TimeStampAddress = reinterpret_cast<uint64_t>(&(hwTimeStamps.GlobalStartTS));

    auto pPipeControlCmd = (PIPE_CONTROL *)commandStream->getSpace(sizeof(PIPE_CONTROL));
    *pPipeControlCmd = PIPE_CONTROL::sInit();
    pPipeControlCmd->setCommandStreamerStallEnable(true);
    pPipeControlCmd->setPostSyncOperation(PIPE_CONTROL::POST_SYNC_OPERATION_WRITE_TIMESTAMP);
    pPipeControlCmd->setAddress(static_cast<uint32_t>(TimeStampAddress & 0x0000FFFFFFFFULL));
    pPipeControlCmd->setAddressHigh(static_cast<uint32_t>(TimeStampAddress >> 32));

    //MI_STORE_REGISTER_MEM for context local timestamp
    TimeStampAddress = reinterpret_cast<uint64_t>(&(hwTimeStamps.ContextStartTS));

    //low part
    auto pMICmdLow = (MI_STORE_REGISTER_MEM *)commandStream->getSpace(sizeof(MI_STORE_REGISTER_MEM));
    *pMICmdLow = MI_STORE_REGISTER_MEM::sInit();
    pMICmdLow->setRegisterAddress(GP_THREAD_TIME_REG_ADDRESS_OFFSET_LOW);
    pMICmdLow->setMemoryAddress(TimeStampAddress);
}

template <typename GfxFamily>
void dispatchProfilingCommandsEnd(
    HwTimeStamps &hwTimeStamps,
    OCLRT::LinearStream *commandStream) {

    using MI_STORE_REGISTER_MEM = typename GfxFamily::MI_STORE_REGISTER_MEM;
    using PIPE_CONTROL = typename GfxFamily::PIPE_CONTROL;

    // PIPE_CONTROL for global timestamp
    auto pPipeControlCmd = (PIPE_CONTROL *)commandStream->getSpace(sizeof(PIPE_CONTROL));
    *pPipeControlCmd = PIPE_CONTROL::sInit();
    pPipeControlCmd->setCommandStreamerStallEnable(true);

    //MI_STORE_REGISTER_MEM for context local timestamp
    uint64_t TimeStampAddress = reinterpret_cast<uint64_t>(&(hwTimeStamps.ContextEndTS));

    //low part
    auto pMICmdLow = (MI_STORE_REGISTER_MEM *)commandStream->getSpace(sizeof(MI_STORE_REGISTER_MEM));
    *pMICmdLow = MI_STORE_REGISTER_MEM::sInit();
    pMICmdLow->setRegisterAddress(GP_THREAD_TIME_REG_ADDRESS_OFFSET_LOW);
    pMICmdLow->setMemoryAddress(TimeStampAddress);
}

template <typename GfxFamily>
void dispatchPerfCountersNoopidRegisterCommands(
    CommandQueue &commandQueue,
    OCLRT::HwPerfCounter &hwPerfCounter,
    OCLRT::LinearStream *commandStream,
    bool start) {

    using MI_STORE_REGISTER_MEM = typename GfxFamily::MI_STORE_REGISTER_MEM;

    uint64_t address = start ? reinterpret_cast<uint64_t>(&(hwPerfCounter.HWPerfCounters.DMAFenceIdBegin))
                             : reinterpret_cast<uint64_t>(&(hwPerfCounter.HWPerfCounters.DMAFenceIdEnd));

    auto pNoopIdRegister = (MI_STORE_REGISTER_MEM *)commandStream->getSpace(sizeof(MI_STORE_REGISTER_MEM));
    *pNoopIdRegister = MI_STORE_REGISTER_MEM::sInit();
    pNoopIdRegister->setRegisterAddress(OCLRT::INSTR_MMIO_NOOPID);
    pNoopIdRegister->setMemoryAddress(address);
}

template <typename GfxFamily>
void dispatchPerfCountersReadFreqRegisterCommands(
    CommandQueue &commandQueue,
    OCLRT::HwPerfCounter &hwPerfCounter,
    OCLRT::LinearStream *commandStream,
    bool start) {

    using MI_STORE_REGISTER_MEM = typename GfxFamily::MI_STORE_REGISTER_MEM;

    uint64_t address = start ? reinterpret_cast<uint64_t>(&(hwPerfCounter.HWPerfCounters.CoreFreqBegin))
                             : reinterpret_cast<uint64_t>(&(hwPerfCounter.HWPerfCounters.CoreFreqEnd));

    auto pCoreFreqRegister = (MI_STORE_REGISTER_MEM *)commandStream->getSpace(sizeof(MI_STORE_REGISTER_MEM));
    *pCoreFreqRegister = MI_STORE_REGISTER_MEM::sInit();
    pCoreFreqRegister->setRegisterAddress(OCLRT::INSTR_MMIO_RPSTAT1);
    pCoreFreqRegister->setMemoryAddress(address);
}

template <typename GfxFamily>
void dispatchPerfCountersGeneralPurposeCounterCommands(
    CommandQueue &commandQueue,
    OCLRT::HwPerfCounter &hwPerfCounter,
    OCLRT::LinearStream *commandStream,
    bool start) {

    using MI_STORE_REGISTER_MEM = typename GfxFamily::MI_STORE_REGISTER_MEM;
    uint64_t address = 0;
    const uint64_t baseAddress = start ? reinterpret_cast<uint64_t>(&(hwPerfCounter.HWPerfCounters.HwPerfReportBegin.Gp))
                                       : reinterpret_cast<uint64_t>(&(hwPerfCounter.HWPerfCounters.HwPerfReportEnd.Gp));

    // Read General Purpose counters
    for (uint16_t i = 0; i < OCLRT::INSTR_GENERAL_PURPOSE_COUNTERS_COUNT; i++) {
        auto pGeneralPurposeRegister = (MI_STORE_REGISTER_MEM *)commandStream->getSpace(sizeof(MI_STORE_REGISTER_MEM));
        *pGeneralPurposeRegister = MI_STORE_REGISTER_MEM::sInit();
        uint32_t regAddr = INSTR_GFX_OFFSETS::INSTR_PERF_CNT_1_DW0 + i * sizeof(cl_uint);
        pGeneralPurposeRegister->setRegisterAddress(regAddr);
        //Gp field is 2*uint64 wide so it can hold 4 uint32
        address = baseAddress + i * sizeof(cl_uint);
        pGeneralPurposeRegister->setMemoryAddress(address);
    }
}

template <typename GfxFamily>
void dispatchPerfCountersUserCounterCommands(
    CommandQueue &commandQueue,
    OCLRT::HwPerfCounter &hwPerfCounter,
    OCLRT::LinearStream *commandStream,
    bool start) {

    using MI_STORE_REGISTER_MEM = typename GfxFamily::MI_STORE_REGISTER_MEM;

    uint64_t address = 0;
    const uint64_t baseAddr = start ? reinterpret_cast<uint64_t>(&(hwPerfCounter.HWPerfCounters.HwPerfReportBegin.User))
                                    : reinterpret_cast<uint64_t>(&(hwPerfCounter.HWPerfCounters.HwPerfReportEnd.User));
    uint32_t cmdNum = 0;
    uint32_t regAddr = 0;
    auto configData = commandQueue.getPerfCountersConfigData();
    auto userRegs = &configData->ReadRegs;

    for (uint32_t i = 0; i < userRegs->RegsCount; i++) {
        auto pRegister = (MI_STORE_REGISTER_MEM *)commandStream->getSpace(sizeof(MI_STORE_REGISTER_MEM));
        *pRegister = MI_STORE_REGISTER_MEM::sInit();

        regAddr = userRegs->Reg[i].Offset;
        pRegister->setRegisterAddress(regAddr);
        //offset between base (low) registers is cl_ulong wide
        address = baseAddr + i * sizeof(cl_ulong);
        pRegister->setMemoryAddress(address);
        cmdNum++;

        if (userRegs->Reg[i].BitSize > 32) {
            pRegister = (MI_STORE_REGISTER_MEM *)commandStream->getSpace(sizeof(MI_STORE_REGISTER_MEM));
            *pRegister = MI_STORE_REGISTER_MEM::sInit();

            regAddr += sizeof(cl_uint);
            pRegister->setRegisterAddress(regAddr);
            address += sizeof(cl_uint);
            pRegister->setMemoryAddress(address);
            cmdNum++;
        }
    }
}

template <typename GfxFamily>
void dispatchPerfCountersOABufferStateCommands(
    CommandQueue &commandQueue,
    OCLRT::HwPerfCounter &hwPerfCounter,
    OCLRT::LinearStream *commandStream) {

    using MI_STORE_REGISTER_MEM = typename GfxFamily::MI_STORE_REGISTER_MEM;

    uint64_t address = 0;
    //OA Status
    auto pOaRegister = (MI_STORE_REGISTER_MEM *)commandStream->getSpace(sizeof(MI_STORE_REGISTER_MEM));
    *pOaRegister = MI_STORE_REGISTER_MEM::sInit();
    pOaRegister->setRegisterAddress(INSTR_GFX_OFFSETS::INSTR_OA_STATUS);
    address = reinterpret_cast<uint64_t>(&(hwPerfCounter.HWPerfCounters.OaStatus));
    pOaRegister->setMemoryAddress(address);

    //OA Head
    pOaRegister = (MI_STORE_REGISTER_MEM *)commandStream->getSpace(sizeof(MI_STORE_REGISTER_MEM));
    *pOaRegister = MI_STORE_REGISTER_MEM::sInit();
    pOaRegister->setRegisterAddress(INSTR_GFX_OFFSETS::INSTR_OA_HEAD_PTR);
    address = reinterpret_cast<uint64_t>(&(hwPerfCounter.HWPerfCounters.OaHead));
    pOaRegister->setMemoryAddress(address);

    //OA Tail
    pOaRegister = (MI_STORE_REGISTER_MEM *)commandStream->getSpace(sizeof(MI_STORE_REGISTER_MEM));
    *pOaRegister = MI_STORE_REGISTER_MEM::sInit();
    pOaRegister->setRegisterAddress(INSTR_GFX_OFFSETS::INSTR_OA_TAIL_PTR);
    address = reinterpret_cast<uint64_t>(&(hwPerfCounter.HWPerfCounters.OaTail));
    pOaRegister->setMemoryAddress(address);
}

template <typename GfxFamily>
void dispatchPerfCountersCommandsStart(
    CommandQueue &commandQueue,
    OCLRT::HwPerfCounter &hwPerfCounter,
    OCLRT::LinearStream *commandStream) {

    using PIPE_CONTROL = typename GfxFamily::PIPE_CONTROL;
    using MI_STORE_REGISTER_MEM = typename GfxFamily::MI_STORE_REGISTER_MEM;
    using MI_REPORT_PERF_COUNT = typename GfxFamily::MI_REPORT_PERF_COUNT;

    auto perfCounters = commandQueue.getPerfCounters();

    uint32_t currentReportId = perfCounters->getCurrentReportId();
    uint64_t address = 0;
    //flush command streamer
    auto pPipeControlCmd = (PIPE_CONTROL *)commandStream->getSpace(sizeof(PIPE_CONTROL));
    *pPipeControlCmd = PIPE_CONTROL::sInit();
    pPipeControlCmd->setCommandStreamerStallEnable(true);

    //Store value of NOOPID register
    dispatchPerfCountersNoopidRegisterCommands<GfxFamily>(commandQueue, hwPerfCounter, commandStream, true);

    //Read Core Frequency
    dispatchPerfCountersReadFreqRegisterCommands<GfxFamily>(commandQueue, hwPerfCounter, commandStream, true);

    dispatchPerfCountersGeneralPurposeCounterCommands<GfxFamily>(commandQueue, hwPerfCounter, commandStream, true);

    auto pReportPerfCount = (MI_REPORT_PERF_COUNT *)commandStream->getSpace(sizeof(MI_REPORT_PERF_COUNT));
    *pReportPerfCount = MI_REPORT_PERF_COUNT::sInit();
    pReportPerfCount->setReportId(currentReportId);
    address = reinterpret_cast<uint64_t>(&(hwPerfCounter.HWPerfCounters.HwPerfReportBegin.Oa));
    pReportPerfCount->setMemoryAddress(address);

    //Timestamp: Global Start
    pPipeControlCmd = (PIPE_CONTROL *)commandStream->getSpace(sizeof(PIPE_CONTROL));
    *pPipeControlCmd = PIPE_CONTROL::sInit();
    pPipeControlCmd->setCommandStreamerStallEnable(true);
    pPipeControlCmd->setPostSyncOperation(PIPE_CONTROL::POST_SYNC_OPERATION_WRITE_TIMESTAMP);
    address = reinterpret_cast<uint64_t>(&(hwPerfCounter.HWTimeStamp.GlobalStartTS));
    pPipeControlCmd->setAddress(static_cast<uint32_t>(address & ((uint64_t)UINT32_MAX)));
    pPipeControlCmd->setAddressHigh(static_cast<uint32_t>(address >> 32));

    dispatchPerfCountersUserCounterCommands<GfxFamily>(commandQueue, hwPerfCounter, commandStream, true);

    commandQueue.sendPerfCountersConfig();
}

template <typename GfxFamily>
void dispatchPerfCountersCommandsEnd(
    CommandQueue &commandQueue,
    OCLRT::HwPerfCounter &hwPerfCounter,
    OCLRT::LinearStream *commandStream) {

    using PIPE_CONTROL = typename GfxFamily::PIPE_CONTROL;
    using MI_STORE_REGISTER_MEM = typename GfxFamily::MI_STORE_REGISTER_MEM;
    using MI_REPORT_PERF_COUNT = typename GfxFamily::MI_REPORT_PERF_COUNT;

    auto perfCounters = commandQueue.getPerfCounters();

    uint32_t currentReportId = perfCounters->getCurrentReportId();
    uint64_t address = 0;

    //flush command streamer
    auto pPipeControlCmd = (PIPE_CONTROL *)commandStream->getSpace(sizeof(PIPE_CONTROL));
    *pPipeControlCmd = PIPE_CONTROL::sInit();
    pPipeControlCmd->setCommandStreamerStallEnable(true);

    dispatchPerfCountersOABufferStateCommands<GfxFamily>(commandQueue, hwPerfCounter, commandStream);

    //Timestamp: Global End
    pPipeControlCmd = (PIPE_CONTROL *)commandStream->getSpace(sizeof(PIPE_CONTROL));
    *pPipeControlCmd = PIPE_CONTROL::sInit();
    pPipeControlCmd->setCommandStreamerStallEnable(true);
    pPipeControlCmd->setPostSyncOperation(PIPE_CONTROL::POST_SYNC_OPERATION_WRITE_TIMESTAMP);
    address = reinterpret_cast<uint64_t>(&(hwPerfCounter.HWTimeStamp.GlobalEndTS));
    pPipeControlCmd->setAddress(static_cast<uint32_t>(address & ((uint64_t)UINT32_MAX)));
    pPipeControlCmd->setAddressHigh(static_cast<uint32_t>(address >> 32));

    auto pReportPerfCount = (MI_REPORT_PERF_COUNT *)commandStream->getSpace(sizeof(MI_REPORT_PERF_COUNT));
    *pReportPerfCount = MI_REPORT_PERF_COUNT::sInit();
    pReportPerfCount->setReportId(currentReportId);
    address = reinterpret_cast<uint64_t>(&(hwPerfCounter.HWPerfCounters.HwPerfReportEnd.Oa));
    pReportPerfCount->setMemoryAddress(address);

    dispatchPerfCountersGeneralPurposeCounterCommands<GfxFamily>(commandQueue, hwPerfCounter, commandStream, false);

    //Store value of NOOPID register
    dispatchPerfCountersNoopidRegisterCommands<GfxFamily>(commandQueue, hwPerfCounter, commandStream, false);

    //Read Core Frequency
    dispatchPerfCountersReadFreqRegisterCommands<GfxFamily>(commandQueue, hwPerfCounter, commandStream, false);

    dispatchPerfCountersUserCounterCommands<GfxFamily>(commandQueue, hwPerfCounter, commandStream, false);

    perfCounters->setCpuTimestamp();
}

template <typename GfxFamily>
void dispatchWalker(
    CommandQueue &commandQueue,
    const MultiDispatchInfo &multiDispatchInfo,
    cl_uint numEventsInWaitList,
    const cl_event *eventWaitList,
    KernelOperation **blockedCommandsData,
    HwTimeStamps *hwTimeStamps,
    OCLRT::HwPerfCounter *hwPerfCounter,
    bool blockQueue = false,
    unsigned int commandType = 0) {

    OCLRT::LinearStream *commandStream = nullptr;
    OCLRT::IndirectHeap *dsh = nullptr, *ish = nullptr, *ioh = nullptr, *ssh = nullptr;
    bool executionModelKernel = multiDispatchInfo.begin()->getKernel()->isParentKernel;

    for (auto &dispatchInfo : multiDispatchInfo) {
        // Compute local workgroup sizes
        if (dispatchInfo.getLocalWorkgroupSize().x == 0) {
            const auto lws = generateWorkgroupSize(dispatchInfo);
            const_cast<DispatchInfo &>(dispatchInfo).setLWS(lws);
        }
    }

    // Allocate command stream and indirect heaps
    size_t cmdQInstructionHeapReservedBlockSize = 0;
    if (blockQueue) {
        using KCH = KernelCommandsHelper<GfxFamily>;
        commandStream = new LinearStream(alignedMalloc(MemoryConstants::pageSize, MemoryConstants::pageSize), MemoryConstants::pageSize);
        if (executionModelKernel) {
            uint32_t offsetDsh = commandQueue.getContext().getDefaultDeviceQueue()->getDshOffset();
            uint32_t colorCalcSize = commandQueue.getContext().getDefaultDeviceQueue()->colorCalcStateSize;

            dsh = allocateIndirectHeap([&multiDispatchInfo, offsetDsh] { return KCH::getTotalSizeRequiredDSH(multiDispatchInfo) + KCH::getTotalSizeRequiredIOH(multiDispatchInfo) + offsetDsh; });
            dsh->getSpace(colorCalcSize);
            ioh = dsh;
        } else {
            dsh = allocateIndirectHeap([&multiDispatchInfo] { return KCH::getTotalSizeRequiredDSH(multiDispatchInfo); });
            ioh = allocateIndirectHeap([&multiDispatchInfo] { return KCH::getTotalSizeRequiredIOH(multiDispatchInfo); });
        }
        ish = allocateIndirectHeap([&multiDispatchInfo] { return KCH::getTotalSizeRequiredIH(multiDispatchInfo); });
        cmdQInstructionHeapReservedBlockSize = commandQueue.getInstructionHeapReservedBlockSize();

        ssh = allocateIndirectHeap([&multiDispatchInfo] { return KCH::getTotalSizeRequiredSSH(multiDispatchInfo); });
        using UniqueIH = std::unique_ptr<IndirectHeap>;
        *blockedCommandsData = new KernelOperation(std::unique_ptr<LinearStream>(commandStream), UniqueIH(dsh),
                                                   UniqueIH(ish), UniqueIH(ioh), UniqueIH(ssh));
        if (executionModelKernel) {
            (*blockedCommandsData)->doNotFreeISH = true;
        }
    } else {
        commandStream = &commandQueue.getCS(0);
        if (executionModelKernel && (commandQueue.getIndirectHeap(IndirectHeap::SURFACE_STATE, 0).getUsed() > 0)) {
            commandQueue.releaseIndirectHeap(IndirectHeap::SURFACE_STATE);
        }
        dsh = &getIndirectHeap<GfxFamily, IndirectHeap::DYNAMIC_STATE>(commandQueue, multiDispatchInfo);
        ish = &getIndirectHeap<GfxFamily, IndirectHeap::INSTRUCTION>(commandQueue, multiDispatchInfo);
        ioh = &getIndirectHeap<GfxFamily, IndirectHeap::INDIRECT_OBJECT>(commandQueue, multiDispatchInfo);
        ssh = &getIndirectHeap<GfxFamily, IndirectHeap::SURFACE_STATE>(commandQueue, multiDispatchInfo);
    }

    using INTERFACE_DESCRIPTOR_DATA = typename GfxFamily::INTERFACE_DESCRIPTOR_DATA;

    dsh->align(KernelCommandsHelper<GfxFamily>::alignInterfaceDescriptorData);

    const size_t offsetInterfaceDescriptorTable = dsh->getUsed();
    uint32_t interfaceDescriptorIndex = 0;
    size_t totalInterfaceDescriptorTableSize = sizeof(INTERFACE_DESCRIPTOR_DATA);
    size_t numDispatches = multiDispatchInfo.size();
    totalInterfaceDescriptorTableSize *= numDispatches;

    if (!executionModelKernel) {
        dsh->getSpace(totalInterfaceDescriptorTableSize);
    } else {
        dsh->getSpace(commandQueue.getContext().getDefaultDeviceQueue()->getDshOffset() - dsh->getUsed());
    }

    // Program media interface descriptor load
    KernelCommandsHelper<GfxFamily>::sendMediaInterfaceDescriptorLoad(
        *commandStream,
        offsetInterfaceDescriptorTable,
        totalInterfaceDescriptorTableSize);

    DEBUG_BREAK_IF(offsetInterfaceDescriptorTable % 64 != 0);

    for (auto &dispatchInfo : multiDispatchInfo) {
        auto &kernel = *dispatchInfo.getKernel();

        DEBUG_BREAK_IF(!(dispatchInfo.getDim() >= 1 && dispatchInfo.getDim() <= 3));
        DEBUG_BREAK_IF(!(dispatchInfo.getGWS().z == 1 || dispatchInfo.getDim() == 3));
        DEBUG_BREAK_IF(!(dispatchInfo.getGWS().y == 1 || dispatchInfo.getDim() >= 2));
        DEBUG_BREAK_IF(!(dispatchInfo.getOffset().z == 0 || dispatchInfo.getDim() == 3));
        DEBUG_BREAK_IF(!(dispatchInfo.getOffset().y == 0 || dispatchInfo.getDim() >= 2));

        // Determine SIMD size
        uint32_t simd = kernel.getKernelInfo().getMaxSimdSize();

        // If we don't have a required WGS, compute one opportunistically
        auto maxWorkGroupSize = static_cast<uint32_t>(commandQueue.getDevice().getDeviceInfo().maxWorkGroupSize);
        if (commandType == CL_COMMAND_NDRANGE_KERNEL) {
            provideLocalWorkGroupSizeHints(commandQueue.getContextPtr(), maxWorkGroupSize, dispatchInfo);
        }

        //Get dispatch geometry
        uint32_t dim = dispatchInfo.getDim();
        Vec3<size_t> gws = dispatchInfo.getGWS();
        Vec3<size_t> offset = dispatchInfo.getOffset();
        Vec3<size_t> swgs = dispatchInfo.getStartOfWorkgroups();

        // Compute local workgroup sizes
        Vec3<size_t> lws = dispatchInfo.getLocalWorkgroupSize();
        Vec3<size_t> elws = (dispatchInfo.getEnqueuedWorkgroupSize().x > 0) ? dispatchInfo.getEnqueuedWorkgroupSize() : lws;

        // Compute number of work groups
        Vec3<size_t> twgs = (dispatchInfo.getTotalNumberOfWorkgroups().x > 0) ? dispatchInfo.getTotalNumberOfWorkgroups() : generateWorkgroupsNumber(gws, lws);
        Vec3<size_t> nwgs = (dispatchInfo.getNumberOfWorkgroups().x > 0) ? dispatchInfo.getNumberOfWorkgroups() : twgs;

        // Patch our kernel constants
        *kernel.globalWorkOffsetX = static_cast<uint32_t>(offset.x);
        *kernel.globalWorkOffsetY = static_cast<uint32_t>(offset.y);
        *kernel.globalWorkOffsetZ = static_cast<uint32_t>(offset.z);

        *kernel.globalWorkSizeX = static_cast<uint32_t>(gws.x);
        *kernel.globalWorkSizeY = static_cast<uint32_t>(gws.y);
        *kernel.globalWorkSizeZ = static_cast<uint32_t>(gws.z);

        if ((&dispatchInfo == &*multiDispatchInfo.begin()) || (kernel.localWorkSizeX2 == &Kernel::dummyPatchLocation)) {
            *kernel.localWorkSizeX = static_cast<uint32_t>(lws.x);
            *kernel.localWorkSizeY = static_cast<uint32_t>(lws.y);
            *kernel.localWorkSizeZ = static_cast<uint32_t>(lws.z);
        }

        *kernel.localWorkSizeX2 = static_cast<uint32_t>(lws.x);
        *kernel.localWorkSizeY2 = static_cast<uint32_t>(lws.y);
        *kernel.localWorkSizeZ2 = static_cast<uint32_t>(lws.z);

        *kernel.enqueuedLocalWorkSizeX = static_cast<uint32_t>(elws.x);
        *kernel.enqueuedLocalWorkSizeY = static_cast<uint32_t>(elws.y);
        *kernel.enqueuedLocalWorkSizeZ = static_cast<uint32_t>(elws.z);

        if (&dispatchInfo == &*multiDispatchInfo.begin()) {
            *kernel.numWorkGroupsX = static_cast<uint32_t>(twgs.x);
            *kernel.numWorkGroupsY = static_cast<uint32_t>(twgs.y);
            *kernel.numWorkGroupsZ = static_cast<uint32_t>(twgs.z);
        }

        *kernel.workDim = dim;

        // Send our indirect object data
        size_t localWorkSizes[3] = {lws.x, lws.y, lws.z};

        auto offsetCrossThreadData = KernelCommandsHelper<GfxFamily>::sendIndirectState(
            *commandStream,
            *dsh,
            *ish,
            cmdQInstructionHeapReservedBlockSize,
            *ioh,
            *ssh,
            kernel,
            simd,
            localWorkSizes,
            offsetInterfaceDescriptorTable,
            interfaceDescriptorIndex);

        if (&dispatchInfo == &*multiDispatchInfo.begin()) {
            // If hwTimeStampAlloc is passed (not nullptr), then we know that profiling is enabled
            if (hwTimeStamps != nullptr) {
                dispatchProfilingCommandsStart<GfxFamily>(*hwTimeStamps, commandStream);
            }
            if (hwPerfCounter != nullptr) {
                dispatchPerfCountersCommandsStart<GfxFamily>(commandQueue, *hwPerfCounter, commandStream);
            }
        }

        PreemptionHelper::applyPreemptionWaCmdsBegin<GfxFamily>(commandStream, commandQueue.getDevice());

        // Implement enabling special WA DisableLSQCROPERFforOCL if needed
        applyWADisableLSQCROPERFforOCL<GfxFamily>(commandStream, kernel, true);

        // Program the walker.  Invokes execution so all state should already be programmed
        typedef typename GfxFamily::GPGPU_WALKER GPGPU_WALKER;
        auto pGpGpuWalkerCmd = (GPGPU_WALKER *)commandStream->getSpace(sizeof(GPGPU_WALKER));
        *pGpGpuWalkerCmd = GfxFamily::cmdInitGpgpuWalker;

        size_t globalOffsets[3] = {offset.x, offset.y, offset.z};
        size_t startWorkGroups[3] = {swgs.x, swgs.y, swgs.z};
        size_t numWorkGroups[3] = {nwgs.x, nwgs.y, nwgs.z};
        auto localWorkSize = setGpgpuWalkerThreadData<GfxFamily>(pGpGpuWalkerCmd, globalOffsets, startWorkGroups, numWorkGroups, localWorkSizes, simd);

        pGpGpuWalkerCmd->setIndirectDataStartAddress((uint32_t)offsetCrossThreadData);
        DEBUG_BREAK_IF(offsetCrossThreadData % 64 != 0);
        pGpGpuWalkerCmd->setInterfaceDescriptorOffset(interfaceDescriptorIndex++);

        auto threadPayload = kernel.getKernelInfo().patchInfo.threadPayload;
        DEBUG_BREAK_IF(nullptr == threadPayload);

        auto numChannels = PerThreadDataHelper::getNumLocalIdChannels(*threadPayload);
        auto localIdSizePerThread = PerThreadDataHelper::getLocalIdSizePerThread(simd, numChannels);
        localIdSizePerThread = std::max(localIdSizePerThread, sizeof(GRF));

        auto sizePerThreadDataTotal = getThreadsPerWG(simd, localWorkSize) * localIdSizePerThread;
        DEBUG_BREAK_IF(sizePerThreadDataTotal == 0); // Hardware requires at least 1 GRF of perThreadData for each thread in thread group

        auto sizeCrossThreadData = kernel.getCrossThreadDataSize();
        auto IndirectDataLength = alignUp((uint32_t)(sizeCrossThreadData + sizePerThreadDataTotal), GPGPU_WALKER::INDIRECTDATASTARTADDRESS_ALIGN_SIZE);
        pGpGpuWalkerCmd->setIndirectDataLength(IndirectDataLength);

        // Implement disabling special WA DisableLSQCROPERFforOCL if needed
        applyWADisableLSQCROPERFforOCL<GfxFamily>(commandStream, kernel, false);

        PreemptionHelper::applyPreemptionWaCmdsEnd<GfxFamily>(commandStream, commandQueue.getDevice());
    }

    // If hwTimeStamps is passed (not nullptr), then we know that profiling is enabled
    if (hwTimeStamps != nullptr) {
        dispatchProfilingCommandsEnd<GfxFamily>(*hwTimeStamps, commandStream);
    }
    if (hwPerfCounter != nullptr) {
        dispatchPerfCountersCommandsEnd<GfxFamily>(commandQueue, *hwPerfCounter, commandStream);
    }
}

template <typename GfxFamily>
void dispatchWalker(
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
    bool blockQueue = false) {

    DispatchInfo dispatchInfo(const_cast<Kernel *>(&kernel), workDim, workItems, localWorkSizesIn, globalOffsets);
    dispatchWalker<GfxFamily>(commandQueue, dispatchInfo, numEventsInWaitList, eventWaitList,
                              blockedCommandsData, hwTimeStamps, hwPerfCounter, blockQueue);
}

template <typename GfxFamily>
void dispatchScheduler(
    CommandQueue &commandQueue,
    DeviceQueueHw<GfxFamily> &devQueueHw,
    SchedulerKernel &scheduler) {

    using INTERFACE_DESCRIPTOR_DATA = typename GfxFamily::INTERFACE_DESCRIPTOR_DATA;
    using GPGPU_WALKER = typename GfxFamily::GPGPU_WALKER;
    using PIPE_CONTROL = typename GfxFamily::PIPE_CONTROL;
    using MI_BATCH_BUFFER_START = typename GfxFamily::MI_BATCH_BUFFER_START;

    OCLRT::LinearStream *commandStream = nullptr;
    OCLRT::IndirectHeap *dsh = nullptr, *ish = nullptr, *ioh = nullptr, *ssh = nullptr;

    commandStream = &commandQueue.getCS(0);
    // note : below code assumes that caller to dispatchScheduler "preallocated" memory
    //        required for execution model in below heap managers
    dsh = devQueueHw.getIndirectHeap(IndirectHeap::DYNAMIC_STATE);
    ish = &commandQueue.getIndirectHeap(IndirectHeap::INSTRUCTION);
    ssh = &commandQueue.getIndirectHeap(IndirectHeap::SURFACE_STATE);

    bool dcFlush = false;
    commandQueue.getDevice().getCommandStreamReceiver().addPipeControl(*commandStream, dcFlush);

    uint32_t interfaceDescriptorIndex = devQueueHw.schedulerIDIndex;
    const size_t offsetInterfaceDescriptorTable = devQueueHw.colorCalcStateSize;
    const size_t offsetInterfaceDescriptor = offsetInterfaceDescriptorTable;
    const size_t totalInterfaceDescriptorTableSize = devQueueHw.interfaceDescriptorEntries * sizeof(INTERFACE_DESCRIPTOR_DATA);

    // Program media interface descriptor load
    KernelCommandsHelper<GfxFamily>::sendMediaInterfaceDescriptorLoad(
        *commandStream,
        offsetInterfaceDescriptor,
        totalInterfaceDescriptorTableSize);

    DEBUG_BREAK_IF(offsetInterfaceDescriptorTable % 64 != 0);

    // Determine SIMD size
    uint32_t simd = scheduler.getKernelInfo().getMaxSimdSize();
    DEBUG_BREAK_IF(simd != PARALLEL_SCHEDULER_COMPILATION_SIZE_20);

    // Patch our kernel constants
    *scheduler.globalWorkOffsetX = 0;
    *scheduler.globalWorkOffsetY = 0;
    *scheduler.globalWorkOffsetZ = 0;

    *scheduler.globalWorkSizeX = (uint32_t)scheduler.getGws();
    *scheduler.globalWorkSizeY = 1;
    *scheduler.globalWorkSizeZ = 1;

    *scheduler.localWorkSizeX = (uint32_t)scheduler.getLws();
    *scheduler.localWorkSizeY = 1;
    *scheduler.localWorkSizeZ = 1;

    *scheduler.localWorkSizeX2 = (uint32_t)scheduler.getLws();
    *scheduler.localWorkSizeY2 = 1;
    *scheduler.localWorkSizeZ2 = 1;

    *scheduler.enqueuedLocalWorkSizeX = (uint32_t)scheduler.getLws();
    *scheduler.enqueuedLocalWorkSizeY = 1;
    *scheduler.enqueuedLocalWorkSizeZ = 1;

    *scheduler.numWorkGroupsX = (uint32_t)(scheduler.getGws() / scheduler.getLws());
    *scheduler.numWorkGroupsY = 0;
    *scheduler.numWorkGroupsZ = 0;

    *scheduler.workDim = 1;

    // Send our indirect object data
    size_t localWorkSizes[3] = {scheduler.getLws(), 1, 1};

    // Create indirectHeap for IOH that is located at the end of device enqueue DSH
    size_t curbeOffset = devQueueHw.setSchedulerCrossThreadData(scheduler);
    IndirectHeap indirectObjectHeap(dsh->getBase(), dsh->getMaxAvailableSpace());
    indirectObjectHeap.getSpace(curbeOffset);
    ioh = &indirectObjectHeap;

    auto offsetCrossThreadData = KernelCommandsHelper<GfxFamily>::sendIndirectState(
        *commandStream,
        *dsh,
        *ish,
        0,
        *ioh,
        *ssh,
        scheduler,
        simd,
        localWorkSizes,
        offsetInterfaceDescriptorTable,
        interfaceDescriptorIndex);

    // Implement enabling special WA DisableLSQCROPERFforOCL if needed
    applyWADisableLSQCROPERFforOCL<GfxFamily>(commandStream, scheduler, true);

    // Program the walker.  Invokes execution so all state should already be programmed
    auto pGpGpuWalkerCmd = (GPGPU_WALKER *)commandStream->getSpace(sizeof(GPGPU_WALKER));
    *pGpGpuWalkerCmd = GfxFamily::cmdInitGpgpuWalker;

    size_t globalOffsets[3] = {0, 0, 0};
    size_t workGroups[3] = {(scheduler.getGws() / scheduler.getLws()), 1, 1};
    auto localWorkSize = setGpgpuWalkerThreadData<GfxFamily>(pGpGpuWalkerCmd, globalOffsets, globalOffsets, workGroups, localWorkSizes, simd);

    pGpGpuWalkerCmd->setIndirectDataStartAddress((uint32_t)offsetCrossThreadData);
    DEBUG_BREAK_IF(offsetCrossThreadData % 64 != 0);
    pGpGpuWalkerCmd->setInterfaceDescriptorOffset(interfaceDescriptorIndex);

    auto threadPayload = scheduler.getKernelInfo().patchInfo.threadPayload;
    DEBUG_BREAK_IF(nullptr == threadPayload);

    auto numChannels = PerThreadDataHelper::getNumLocalIdChannels(*threadPayload);
    auto localIdSizePerThread = PerThreadDataHelper::getLocalIdSizePerThread(simd, numChannels);
    localIdSizePerThread = std::max(localIdSizePerThread, sizeof(GRF));

    auto sizePerThreadDataTotal = getThreadsPerWG(simd, localWorkSize) * localIdSizePerThread;
    DEBUG_BREAK_IF(sizePerThreadDataTotal == 0); // Hardware requires at least 1 GRF of perThreadData for each thread in thread group

    auto sizeCrossThreadData = scheduler.getCrossThreadDataSize();
    auto IndirectDataLength = alignUp((uint32_t)(sizeCrossThreadData + sizePerThreadDataTotal), GPGPU_WALKER::INDIRECTDATASTARTADDRESS_ALIGN_SIZE);
    pGpGpuWalkerCmd->setIndirectDataLength(IndirectDataLength);

    // Implement disabling special WA DisableLSQCROPERFforOCL if needed
    applyWADisableLSQCROPERFforOCL<GfxFamily>(commandStream, scheduler, false);

    // Do not put BB_START only when returning in first Scheduler run
    if (devQueueHw.getSchedulerReturnInstance() != 1) {

        commandQueue.getDevice().getCommandStreamReceiver().addPipeControl(*commandStream, true);

        // Add BB Start Cmd to the SLB in the Primary Batch Buffer
        auto *bbStart = (MI_BATCH_BUFFER_START *)commandStream->getSpace(sizeof(MI_BATCH_BUFFER_START));
        *bbStart = MI_BATCH_BUFFER_START::sInit();
        bbStart->setSecondLevelBatchBuffer(MI_BATCH_BUFFER_START::SECOND_LEVEL_BATCH_BUFFER_FIRST_LEVEL_BATCH);
        uint64_t slbAddress = devQueueHw.getSlbBuffer()->getGpuAddress();
        bbStart->setBatchBufferStartAddressGraphicsaddress472(slbAddress);
    }
}

template <typename GfxFamily, unsigned int eventType>
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
            size += getSizeForWADisableLSQCROPERFforOCL<GfxFamily>(&kernel);
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
        size += getSizeForWADisableLSQCROPERFforOCL<GfxFamily>(pKernel);

        return size;
    }
};

template <typename GfxFamily, unsigned int eventType>
LinearStream &getCommandStream(CommandQueue &commandQueue, bool reserveProfilingCmdsSpace, bool reservePerfCounterCmdsSpace, const Kernel *pKernel) {
    auto expectedSizeCS = EnqueueOperation<GfxFamily, eventType>::getSizeRequiredCS(reserveProfilingCmdsSpace, reservePerfCounterCmdsSpace, commandQueue, pKernel);
    return commandQueue.getCS(expectedSizeCS);
}

template <typename GfxFamily, unsigned int eventType>
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
    switch(heapType) {
    case IndirectHeap::DYNAMIC_STATE:   expectedSize = KernelCommandsHelper<GfxFamily>::getTotalSizeRequiredDSH(multiDispatchInfo); break;
    case IndirectHeap::INSTRUCTION:     expectedSize = KernelCommandsHelper<GfxFamily>::getTotalSizeRequiredIH( multiDispatchInfo); break;
    case IndirectHeap::INDIRECT_OBJECT: expectedSize = KernelCommandsHelper<GfxFamily>::getTotalSizeRequiredIOH(multiDispatchInfo); break;
    case IndirectHeap::SURFACE_STATE:   expectedSize = KernelCommandsHelper<GfxFamily>::getTotalSizeRequiredSSH(multiDispatchInfo); break;
    }
    // clang-format on

    if (multiDispatchInfo.begin()->getKernel()->isParentKernel) {
        if (heapType == IndirectHeap::INSTRUCTION || heapType == IndirectHeap::SURFACE_STATE) {
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
