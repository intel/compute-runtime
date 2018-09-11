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

#pragma once
#include "runtime/command_queue/gpgpu_walker.h"
#include "runtime/command_queue/command_queue.h"
#include "runtime/command_queue/local_id_gen.h"
#include "runtime/command_stream/command_stream_receiver.h"
#include "runtime/device/device_info.h"
#include "runtime/event/perf_counter.h"
#include "runtime/event/user_event.h"
#include "runtime/indirect_heap/indirect_heap.h"
#include "instrumentation.h"
#include "runtime/helpers/aligned_memory.h"
#include "runtime/helpers/debug_helpers.h"
#include "runtime/helpers/kernel_commands.h"
#include "runtime/helpers/validators.h"
#include "runtime/mem_obj/mem_obj.h"
#include "runtime/memory_manager/graphics_allocation.h"
#include <algorithm>
#include <cmath>

namespace OCLRT {

// Performs ReadModifyWrite operation on value of a register: Register = Register Operation Mask
template <typename GfxFamily>
void GpgpuWalkerHelper<GfxFamily>::addAluReadModifyWriteRegister(
    OCLRT::LinearStream *pCommandStream,
    uint32_t aluRegister,
    uint32_t operation,
    uint32_t mask) {
    // Load "Register" value into CS_GPR_R0
    typedef typename GfxFamily::MI_LOAD_REGISTER_REG MI_LOAD_REGISTER_REG;
    typedef typename GfxFamily::MI_MATH MI_MATH;
    typedef typename GfxFamily::MI_MATH_ALU_INST_INLINE MI_MATH_ALU_INST_INLINE;
    auto pCmd = reinterpret_cast<MI_LOAD_REGISTER_REG *>(pCommandStream->getSpace(sizeof(MI_LOAD_REGISTER_REG)));
    *pCmd = MI_LOAD_REGISTER_REG::sInit();
    pCmd->setSourceRegisterAddress(aluRegister);
    pCmd->setDestinationRegisterAddress(CS_GPR_R0);

    // Load "Mask" into CS_GPR_R1
    typedef typename GfxFamily::MI_LOAD_REGISTER_IMM MI_LOAD_REGISTER_IMM;
    auto pCmd2 = reinterpret_cast<MI_LOAD_REGISTER_IMM *>(pCommandStream->getSpace(sizeof(MI_LOAD_REGISTER_IMM)));
    *pCmd2 = MI_LOAD_REGISTER_IMM::sInit();
    pCmd2->setRegisterOffset(CS_GPR_R1);
    pCmd2->setDataDword(mask);

    // Add instruction MI_MATH with 4 MI_MATH_ALU_INST_INLINE operands
    auto pCmd3 = reinterpret_cast<uint32_t *>(pCommandStream->getSpace(sizeof(MI_MATH) + NUM_ALU_INST_FOR_READ_MODIFY_WRITE * sizeof(MI_MATH_ALU_INST_INLINE)));
    reinterpret_cast<MI_MATH *>(pCmd3)->DW0.Value = 0x0;
    reinterpret_cast<MI_MATH *>(pCmd3)->DW0.BitField.InstructionType = MI_MATH::COMMAND_TYPE_MI_COMMAND;
    reinterpret_cast<MI_MATH *>(pCmd3)->DW0.BitField.InstructionOpcode = MI_MATH::MI_COMMAND_OPCODE_MI_MATH;
    // 0x3 - 5 Dwords length cmd (-2): 1 for MI_MATH, 4 for MI_MATH_ALU_INST_INLINE
    reinterpret_cast<MI_MATH *>(pCmd3)->DW0.BitField.DwordLength = NUM_ALU_INST_FOR_READ_MODIFY_WRITE - 1;
    pCmd3++;
    MI_MATH_ALU_INST_INLINE *pAluParam = reinterpret_cast<MI_MATH_ALU_INST_INLINE *>(pCmd3);

    // Setup first operand of MI_MATH - load CS_GPR_R0 into register A
    pAluParam->DW0.BitField.ALUOpcode = ALU_OPCODE_LOAD;
    pAluParam->DW0.BitField.Operand1 = ALU_REGISTER_R_SRCA;
    pAluParam->DW0.BitField.Operand2 = ALU_REGISTER_R_0;
    pAluParam++;

    // Setup second operand of MI_MATH - load CS_GPR_R1 into register B
    pAluParam->DW0.BitField.ALUOpcode = ALU_OPCODE_LOAD;
    pAluParam->DW0.BitField.Operand1 = ALU_REGISTER_R_SRCB;
    pAluParam->DW0.BitField.Operand2 = ALU_REGISTER_R_1;
    pAluParam++;

    // Setup third operand of MI_MATH - "Operation" on registers A and B
    pAluParam->DW0.BitField.ALUOpcode = operation;
    pAluParam->DW0.BitField.Operand1 = 0;
    pAluParam->DW0.BitField.Operand2 = 0;
    pAluParam++;

    // Setup fourth operand of MI_MATH - store result into CS_GPR_R0
    pAluParam->DW0.BitField.ALUOpcode = ALU_OPCODE_STORE;
    pAluParam->DW0.BitField.Operand1 = ALU_REGISTER_R_0;
    pAluParam->DW0.BitField.Operand2 = ALU_REGISTER_R_ACCU;

    // LOAD value of CS_GPR_R0 into "Register"
    auto pCmd4 = reinterpret_cast<MI_LOAD_REGISTER_REG *>(pCommandStream->getSpace(sizeof(MI_LOAD_REGISTER_REG)));
    *pCmd4 = MI_LOAD_REGISTER_REG::sInit();
    pCmd4->setSourceRegisterAddress(CS_GPR_R0);
    pCmd4->setDestinationRegisterAddress(aluRegister);

    // Add PIPE_CONTROL to flush caches
    auto pCmd5 = reinterpret_cast<PIPE_CONTROL *>(pCommandStream->getSpace(sizeof(PIPE_CONTROL)));
    *pCmd5 = PIPE_CONTROL::sInit();
    pCmd5->setCommandStreamerStallEnable(true);
    pCmd5->setDcFlushEnable(true);
    pCmd5->setTextureCacheInvalidationEnable(true);
    pCmd5->setPipeControlFlushEnable(true);
    pCmd5->setStateCacheInvalidationEnable(true);
}

template <typename GfxFamily>
inline size_t GpgpuWalkerHelper<GfxFamily>::setGpgpuWalkerThreadData(
    WALKER_HANDLE pCmdData,
    const size_t globalOffsets[3],
    const size_t startWorkGroups[3],
    const size_t numWorkGroups[3],
    const size_t localWorkSizesIn[3],
    uint32_t simd) {
    WALKER_TYPE<GfxFamily> *pCmd = static_cast<WALKER_TYPE<GfxFamily> *>(pCmdData);

    auto localWorkSize = localWorkSizesIn[0] * localWorkSizesIn[1] * localWorkSizesIn[2];

    auto threadsPerWorkGroup = getThreadsPerWG(simd, localWorkSize);
    pCmd->setThreadWidthCounterMaximum(static_cast<uint32_t>(threadsPerWorkGroup));

    pCmd->setThreadGroupIdXDimension(static_cast<uint32_t>(numWorkGroups[0]));
    pCmd->setThreadGroupIdYDimension(static_cast<uint32_t>(numWorkGroups[1]));
    pCmd->setThreadGroupIdZDimension(static_cast<uint32_t>(numWorkGroups[2]));

    // compute executionMask - to tell which SIMD lines are active within thread
    auto remainderSimdLanes = localWorkSize & (simd - 1);
    uint64_t executionMask = (1ull << remainderSimdLanes) - 1;
    if (!executionMask)
        executionMask = ~executionMask;

    using SIMD_SIZE = typename WALKER_TYPE<GfxFamily>::SIMD_SIZE;

    pCmd->setRightExecutionMask(static_cast<uint32_t>(executionMask));
    pCmd->setBottomExecutionMask(static_cast<uint32_t>(0xffffffff));
    pCmd->setSimdSize(static_cast<SIMD_SIZE>(simd >> 4));

    pCmd->setThreadGroupIdStartingX(static_cast<uint32_t>(startWorkGroups[0]));
    pCmd->setThreadGroupIdStartingY(static_cast<uint32_t>(startWorkGroups[1]));
    pCmd->setThreadGroupIdStartingResumeZ(static_cast<uint32_t>(startWorkGroups[2]));

    return localWorkSize;
}

template <typename GfxFamily>
void GpgpuWalkerHelper<GfxFamily>::dispatchProfilingCommandsStart(
    HwTimeStamps &hwTimeStamps,
    OCLRT::LinearStream *commandStream) {
    using MI_STORE_REGISTER_MEM = typename GfxFamily::MI_STORE_REGISTER_MEM;

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
void GpgpuWalkerHelper<GfxFamily>::dispatchProfilingCommandsEnd(
    HwTimeStamps &hwTimeStamps,
    OCLRT::LinearStream *commandStream) {

    using MI_STORE_REGISTER_MEM = typename GfxFamily::MI_STORE_REGISTER_MEM;

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
void GpgpuWalkerHelper<GfxFamily>::dispatchPerfCountersNoopidRegisterCommands(
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
void GpgpuWalkerHelper<GfxFamily>::dispatchPerfCountersReadFreqRegisterCommands(
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
void GpgpuWalkerHelper<GfxFamily>::dispatchPerfCountersGeneralPurposeCounterCommands(
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
void GpgpuWalkerHelper<GfxFamily>::dispatchPerfCountersUserCounterCommands(
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
void GpgpuWalkerHelper<GfxFamily>::dispatchPerfCountersOABufferStateCommands(
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
void GpgpuWalkerHelper<GfxFamily>::dispatchPerfCountersCommandsStart(
    CommandQueue &commandQueue,
    OCLRT::HwPerfCounter &hwPerfCounter,
    OCLRT::LinearStream *commandStream) {

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
    GpgpuWalkerHelper<GfxFamily>::dispatchPerfCountersNoopidRegisterCommands(commandQueue, hwPerfCounter, commandStream, true);

    //Read Core Frequency
    GpgpuWalkerHelper<GfxFamily>::dispatchPerfCountersReadFreqRegisterCommands(commandQueue, hwPerfCounter, commandStream, true);

    GpgpuWalkerHelper<GfxFamily>::dispatchPerfCountersGeneralPurposeCounterCommands(commandQueue, hwPerfCounter, commandStream, true);

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

    GpgpuWalkerHelper<GfxFamily>::dispatchPerfCountersUserCounterCommands(commandQueue, hwPerfCounter, commandStream, true);

    commandQueue.sendPerfCountersConfig();
}

template <typename GfxFamily>
void GpgpuWalkerHelper<GfxFamily>::dispatchPerfCountersCommandsEnd(
    CommandQueue &commandQueue,
    OCLRT::HwPerfCounter &hwPerfCounter,
    OCLRT::LinearStream *commandStream) {

    using MI_STORE_REGISTER_MEM = typename GfxFamily::MI_STORE_REGISTER_MEM;
    using MI_REPORT_PERF_COUNT = typename GfxFamily::MI_REPORT_PERF_COUNT;

    auto perfCounters = commandQueue.getPerfCounters();

    uint32_t currentReportId = perfCounters->getCurrentReportId();
    uint64_t address = 0;

    //flush command streamer
    auto pPipeControlCmd = (PIPE_CONTROL *)commandStream->getSpace(sizeof(PIPE_CONTROL));
    *pPipeControlCmd = PIPE_CONTROL::sInit();
    pPipeControlCmd->setCommandStreamerStallEnable(true);

    GpgpuWalkerHelper<GfxFamily>::dispatchPerfCountersOABufferStateCommands(commandQueue, hwPerfCounter, commandStream);

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

    GpgpuWalkerHelper<GfxFamily>::dispatchPerfCountersGeneralPurposeCounterCommands(commandQueue, hwPerfCounter, commandStream, false);

    //Store value of NOOPID register
    GpgpuWalkerHelper<GfxFamily>::dispatchPerfCountersNoopidRegisterCommands(commandQueue, hwPerfCounter, commandStream, false);

    //Read Core Frequency
    GpgpuWalkerHelper<GfxFamily>::dispatchPerfCountersReadFreqRegisterCommands(commandQueue, hwPerfCounter, commandStream, false);

    GpgpuWalkerHelper<GfxFamily>::dispatchPerfCountersUserCounterCommands(commandQueue, hwPerfCounter, commandStream, false);

    perfCounters->setCpuTimestamp();
}

template <typename GfxFamily>
void GpgpuWalkerHelper<GfxFamily>::dispatchWalker(
    CommandQueue &commandQueue,
    const MultiDispatchInfo &multiDispatchInfo,
    cl_uint numEventsInWaitList,
    const cl_event *eventWaitList,
    KernelOperation **blockedCommandsData,
    HwTimeStamps *hwTimeStamps,
    OCLRT::HwPerfCounter *hwPerfCounter,
    TimestampPacket *timestampPacket,
    PreemptionMode preemptionMode,
    bool blockQueue,
    uint32_t commandType) {

    OCLRT::LinearStream *commandStream = nullptr;
    OCLRT::IndirectHeap *dsh = nullptr, *ioh = nullptr, *ssh = nullptr;
    auto parentKernel = multiDispatchInfo.peekParentKernel();

    for (auto &dispatchInfo : multiDispatchInfo) {
        // Compute local workgroup sizes
        if (dispatchInfo.getLocalWorkgroupSize().x == 0) {
            const auto lws = generateWorkgroupSize(dispatchInfo);
            const_cast<DispatchInfo &>(dispatchInfo).setLWS(lws);
        }
    }

    // Allocate command stream and indirect heaps
    if (blockQueue) {
        using KCH = KernelCommandsHelper<GfxFamily>;
        commandStream = new LinearStream(alignedMalloc(MemoryConstants::pageSize, MemoryConstants::pageSize),
                                         MemoryConstants::pageSize);
        if (parentKernel) {
            uint32_t colorCalcSize = commandQueue.getContext().getDefaultDeviceQueue()->colorCalcStateSize;

            commandQueue.allocateHeapMemory(
                IndirectHeap::DYNAMIC_STATE,
                commandQueue.getContext().getDefaultDeviceQueue()->getDshBuffer()->getUnderlyingBufferSize(),
                dsh);

            dsh->getSpace(colorCalcSize);
            ioh = dsh;
            commandQueue.allocateHeapMemory(IndirectHeap::SURFACE_STATE,
                                            KernelCommandsHelper<GfxFamily>::template getSizeRequiredForExecutionModel<
                                                IndirectHeap::SURFACE_STATE>(*parentKernel) +
                                                KCH::getTotalSizeRequiredSSH(multiDispatchInfo),
                                            ssh);
        } else {
            commandQueue.allocateHeapMemory(IndirectHeap::DYNAMIC_STATE, KCH::getTotalSizeRequiredDSH(multiDispatchInfo), dsh);
            commandQueue.allocateHeapMemory(IndirectHeap::INDIRECT_OBJECT, KCH::getTotalSizeRequiredIOH(multiDispatchInfo), ioh);
            commandQueue.allocateHeapMemory(IndirectHeap::SURFACE_STATE, KCH::getTotalSizeRequiredSSH(multiDispatchInfo), ssh);
        }

        using UniqueIH = std::unique_ptr<IndirectHeap>;
        *blockedCommandsData = new KernelOperation(std::unique_ptr<LinearStream>(commandStream), UniqueIH(dsh), UniqueIH(ioh),
                                                   UniqueIH(ssh), *commandQueue.getDevice().getMemoryManager());
        if (parentKernel) {
            (*blockedCommandsData)->doNotFreeISH = true;
        }
    } else {
        commandStream = &commandQueue.getCS(0);
        if (parentKernel && (commandQueue.getIndirectHeap(IndirectHeap::SURFACE_STATE, 0).getUsed() > 0)) {
            commandQueue.releaseIndirectHeap(IndirectHeap::SURFACE_STATE);
        }
        dsh = &getIndirectHeap<GfxFamily, IndirectHeap::DYNAMIC_STATE>(commandQueue, multiDispatchInfo);
        ioh = &getIndirectHeap<GfxFamily, IndirectHeap::INDIRECT_OBJECT>(commandQueue, multiDispatchInfo);
        ssh = &getIndirectHeap<GfxFamily, IndirectHeap::SURFACE_STATE>(commandQueue, multiDispatchInfo);
    }

    if (commandQueue.getDevice().getCommandStreamReceiver().peekTimestampPacketWriteEnabled()) {
        GpgpuWalkerHelper<GfxFamily>::dispatchOnDeviceWaitlistSemaphores(commandStream, commandQueue.getDevice(),
                                                                         numEventsInWaitList, eventWaitList);
    }

    dsh->align(KernelCommandsHelper<GfxFamily>::alignInterfaceDescriptorData);

    uint32_t interfaceDescriptorIndex = 0;
    const size_t offsetInterfaceDescriptorTable = dsh->getUsed();

    size_t totalInterfaceDescriptorTableSize = sizeof(INTERFACE_DESCRIPTOR_DATA);

    getDefaultDshSpace(offsetInterfaceDescriptorTable, commandQueue, multiDispatchInfo, totalInterfaceDescriptorTableSize,
                       parentKernel, dsh, commandStream);

    // Program media interface descriptor load
    KernelCommandsHelper<GfxFamily>::sendMediaInterfaceDescriptorLoad(
        *commandStream,
        offsetInterfaceDescriptorTable,
        totalInterfaceDescriptorTableSize);

    DEBUG_BREAK_IF(offsetInterfaceDescriptorTable % 64 != 0);

    size_t currentDispatchIndex = 0;
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
        Vec3<size_t> twgs = (dispatchInfo.getTotalNumberOfWorkgroups().x > 0) ? dispatchInfo.getTotalNumberOfWorkgroups()
                                                                              : generateWorkgroupsNumber(gws, lws);
        Vec3<size_t> nwgs = (dispatchInfo.getNumberOfWorkgroups().x > 0) ? dispatchInfo.getNumberOfWorkgroups() : twgs;

        // Patch our kernel constants
        *kernel.globalWorkOffsetX = static_cast<uint32_t>(offset.x);
        *kernel.globalWorkOffsetY = static_cast<uint32_t>(offset.y);
        *kernel.globalWorkOffsetZ = static_cast<uint32_t>(offset.z);

        *kernel.globalWorkSizeX = static_cast<uint32_t>(gws.x);
        *kernel.globalWorkSizeY = static_cast<uint32_t>(gws.y);
        *kernel.globalWorkSizeZ = static_cast<uint32_t>(gws.z);

        if ((&kernel == multiDispatchInfo.peekMainKernel()) || (kernel.localWorkSizeX2 == &Kernel::dummyPatchLocation)) {
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

        if (&kernel == multiDispatchInfo.peekMainKernel()) {
            *kernel.numWorkGroupsX = static_cast<uint32_t>(twgs.x);
            *kernel.numWorkGroupsY = static_cast<uint32_t>(twgs.y);
            *kernel.numWorkGroupsZ = static_cast<uint32_t>(twgs.z);
        }

        *kernel.workDim = dim;

        // Send our indirect object data
        size_t localWorkSizes[3] = {lws.x, lws.y, lws.z};

        dispatchProfilingPerfStartCommands(dispatchInfo, multiDispatchInfo, hwTimeStamps,
                                           hwPerfCounter, commandStream, commandQueue);

        dispatchWorkarounds(commandStream, commandQueue, kernel, true);

        bool setupTimestampPacket = timestampPacket && (currentDispatchIndex == multiDispatchInfo.size() - 1);
        if (setupTimestampPacket) {
            GpgpuWalkerHelper<GfxFamily>::setupTimestampPacket(commandStream, nullptr, timestampPacket,
                                                               TimestampPacket::WriteOperationType::BeforeWalker);
        }

        // Program the walker.  Invokes execution so all state should already be programmed
        auto pWalkerCmd = static_cast<WALKER_TYPE<GfxFamily> *>(commandStream->getSpace(sizeof(WALKER_TYPE<GfxFamily>)));
        *pWalkerCmd = GfxFamily::cmdInitGpgpuWalker;

        if (setupTimestampPacket) {
            GpgpuWalkerHelper<GfxFamily>::setupTimestampPacket(commandStream, pWalkerCmd, timestampPacket,
                                                               TimestampPacket::WriteOperationType::AfterWalker);
        }

        auto idd = obtainInterfaceDescriptorData(pWalkerCmd);

        auto offsetCrossThreadData = KernelCommandsHelper<GfxFamily>::sendIndirectState(
            *commandStream,
            *dsh,
            *ioh,
            *ssh,
            kernel,
            simd,
            localWorkSizes,
            offsetInterfaceDescriptorTable,
            interfaceDescriptorIndex,
            preemptionMode,
            idd);

        size_t globalOffsets[3] = {offset.x, offset.y, offset.z};
        size_t startWorkGroups[3] = {swgs.x, swgs.y, swgs.z};
        size_t numWorkGroups[3] = {nwgs.x, nwgs.y, nwgs.z};
        auto localWorkSize = GpgpuWalkerHelper<GfxFamily>::setGpgpuWalkerThreadData(pWalkerCmd, globalOffsets, startWorkGroups,
                                                                                    numWorkGroups, localWorkSizes, simd);

        DEBUG_BREAK_IF(offsetCrossThreadData % 64 != 0);
        setOffsetCrossThreadData(pWalkerCmd, offsetCrossThreadData, interfaceDescriptorIndex);

        auto threadPayload = kernel.getKernelInfo().patchInfo.threadPayload;
        DEBUG_BREAK_IF(nullptr == threadPayload);

        auto numChannels = PerThreadDataHelper::getNumLocalIdChannels(*threadPayload);
        auto localIdSizePerThread = PerThreadDataHelper::getLocalIdSizePerThread(simd, numChannels);
        localIdSizePerThread = std::max(localIdSizePerThread, sizeof(GRF));

        auto sizePerThreadDataTotal = getThreadsPerWG(simd, localWorkSize) * localIdSizePerThread;
        DEBUG_BREAK_IF(sizePerThreadDataTotal == 0); // Hardware requires at least 1 GRF of perThreadData for each thread in thread group

        auto sizeCrossThreadData = kernel.getCrossThreadDataSize();
        auto IndirectDataLength = alignUp(static_cast<uint32_t>(sizeCrossThreadData + sizePerThreadDataTotal),
                                          WALKER_TYPE<GfxFamily>::INDIRECTDATASTARTADDRESS_ALIGN_SIZE);
        pWalkerCmd->setIndirectDataLength(IndirectDataLength);

        dispatchWorkarounds(commandStream, commandQueue, kernel, false);
        currentDispatchIndex++;
    }
    dispatchProfilingPerfEndCommands(hwTimeStamps, hwPerfCounter, commandStream, commandQueue);
}

template <typename GfxFamily>
inline void GpgpuWalkerHelper<GfxFamily>::dispatchOnDeviceWaitlistSemaphores(LinearStream *commandStream, Device &currentDevice,
                                                                             cl_uint numEventsInWaitList, const cl_event *eventWaitList) {
    using MI_SEMAPHORE_WAIT = typename GfxFamily::MI_SEMAPHORE_WAIT;

    for (cl_uint i = 0; i < numEventsInWaitList; i++) {
        auto event = castToObjectOrAbort<Event>(eventWaitList[i]);
        if (event->isUserEvent() || (&event->getCommandQueue()->getDevice() != &currentDevice)) {
            continue;
        }
        auto timestampPacket = event->getTimestampPacket();

        auto compareAddress = timestampPacket->pickAddressForDataWrite(TimestampPacket::DataIndex::ContextEnd);

        KernelCommandsHelper<GfxFamily>::programMiSemaphoreWait(*commandStream, compareAddress, 1);
    }
}

template <typename GfxFamily>
inline void GpgpuWalkerHelper<GfxFamily>::getDefaultDshSpace(
    const size_t &offsetInterfaceDescriptorTable,
    CommandQueue &commandQueue,
    const MultiDispatchInfo &multiDispatchInfo,
    size_t &totalInterfaceDescriptorTableSize,
    OCLRT::Kernel *parentKernel,
    OCLRT::IndirectHeap *dsh,
    OCLRT::LinearStream *commandStream) {

    size_t numDispatches = multiDispatchInfo.size();
    totalInterfaceDescriptorTableSize *= numDispatches;

    if (!parentKernel) {
        dsh->getSpace(totalInterfaceDescriptorTableSize);
    } else {
        dsh->getSpace(commandQueue.getContext().getDefaultDeviceQueue()->getDshOffset() - dsh->getUsed());
    }
}

template <typename GfxFamily>
inline typename GpgpuWalkerHelper<GfxFamily>::INTERFACE_DESCRIPTOR_DATA *GpgpuWalkerHelper<GfxFamily>::obtainInterfaceDescriptorData(
    WALKER_HANDLE pCmdData) {

    return nullptr;
}

template <typename GfxFamily>
inline void GpgpuWalkerHelper<GfxFamily>::setOffsetCrossThreadData(
    WALKER_HANDLE pCmdData,
    size_t &offsetCrossThreadData,
    uint32_t &interfaceDescriptorIndex) {

    WALKER_TYPE<GfxFamily> *pCmd = static_cast<WALKER_TYPE<GfxFamily> *>(pCmdData);
    pCmd->setIndirectDataStartAddress(static_cast<uint32_t>(offsetCrossThreadData));
    pCmd->setInterfaceDescriptorOffset(interfaceDescriptorIndex++);
}

template <typename GfxFamily>
inline void GpgpuWalkerHelper<GfxFamily>::dispatchWorkarounds(
    OCLRT::LinearStream *commandStream,
    CommandQueue &commandQueue,
    OCLRT::Kernel &kernel,
    const bool &enable) {

    if (enable) {
        PreemptionHelper::applyPreemptionWaCmdsBegin<GfxFamily>(commandStream, commandQueue.getDevice());
        // Implement enabling special WA DisableLSQCROPERFforOCL if needed
        GpgpuWalkerHelper<GfxFamily>::applyWADisableLSQCROPERFforOCL(commandStream, kernel, enable);
    } else {
        // Implement disabling special WA DisableLSQCROPERFforOCL if needed
        GpgpuWalkerHelper<GfxFamily>::applyWADisableLSQCROPERFforOCL(commandStream, kernel, enable);
        PreemptionHelper::applyPreemptionWaCmdsEnd<GfxFamily>(commandStream, commandQueue.getDevice());
    }
}

template <typename GfxFamily>
inline void GpgpuWalkerHelper<GfxFamily>::dispatchProfilingPerfStartCommands(
    const OCLRT::DispatchInfo &dispatchInfo,
    const MultiDispatchInfo &multiDispatchInfo,
    HwTimeStamps *hwTimeStamps,
    OCLRT::HwPerfCounter *hwPerfCounter,
    OCLRT::LinearStream *commandStream,
    CommandQueue &commandQueue) {

    if (&dispatchInfo == &*multiDispatchInfo.begin()) {
        // If hwTimeStampAlloc is passed (not nullptr), then we know that profiling is enabled
        if (hwTimeStamps != nullptr) {
            GpgpuWalkerHelper<GfxFamily>::dispatchProfilingCommandsStart(*hwTimeStamps, commandStream);
        }
        if (hwPerfCounter != nullptr) {
            GpgpuWalkerHelper<GfxFamily>::dispatchPerfCountersCommandsStart(commandQueue, *hwPerfCounter, commandStream);
        }
    }
}

template <typename GfxFamily>
inline void GpgpuWalkerHelper<GfxFamily>::dispatchProfilingPerfEndCommands(
    HwTimeStamps *hwTimeStamps,
    OCLRT::HwPerfCounter *hwPerfCounter,
    OCLRT::LinearStream *commandStream,
    CommandQueue &commandQueue) {

    // If hwTimeStamps is passed (not nullptr), then we know that profiling is enabled
    if (hwTimeStamps != nullptr) {
        GpgpuWalkerHelper<GfxFamily>::dispatchProfilingCommandsEnd(*hwTimeStamps, commandStream);
    }
    if (hwPerfCounter != nullptr) {
        GpgpuWalkerHelper<GfxFamily>::dispatchPerfCountersCommandsEnd(commandQueue, *hwPerfCounter, commandStream);
    }
}

template <typename GfxFamily>
void GpgpuWalkerHelper<GfxFamily>::setupTimestampPacket(
    LinearStream *cmdStream,
    WALKER_HANDLE walkerHandle,
    TimestampPacket *timestampPacket,
    TimestampPacket::WriteOperationType writeOperationType) {

    uint64_t address;
    if (TimestampPacket::WriteOperationType::BeforeWalker == writeOperationType) {
        address = timestampPacket->pickAddressForDataWrite(TimestampPacket::DataIndex::Submit);
    } else {
        address = timestampPacket->pickAddressForDataWrite(TimestampPacket::DataIndex::ContextEnd);
    }

    auto pipeControlCmd = cmdStream->getSpaceForCmd<PIPE_CONTROL>();
    *pipeControlCmd = PIPE_CONTROL::sInit();
    pipeControlCmd->setCommandStreamerStallEnable(true);
    pipeControlCmd->setPostSyncOperation(PIPE_CONTROL::POST_SYNC_OPERATION_WRITE_IMMEDIATE_DATA);
    pipeControlCmd->setAddress(static_cast<uint32_t>(address & 0x0000FFFFFFFFULL));
    pipeControlCmd->setAddressHigh(static_cast<uint32_t>(address >> 32));
    pipeControlCmd->setImmediateData(0);
}

template <typename GfxFamily>
void GpgpuWalkerHelper<GfxFamily>::dispatchScheduler(
    CommandQueue &commandQueue,
    DeviceQueueHw<GfxFamily> &devQueueHw,
    PreemptionMode preemptionMode,
    SchedulerKernel &scheduler,
    IndirectHeap *ssh,
    IndirectHeap *dsh) {

    using INTERFACE_DESCRIPTOR_DATA = typename GfxFamily::INTERFACE_DESCRIPTOR_DATA;
    using GPGPU_WALKER = typename GfxFamily::GPGPU_WALKER;
    using MI_BATCH_BUFFER_START = typename GfxFamily::MI_BATCH_BUFFER_START;

    OCLRT::LinearStream *commandStream = nullptr;
    OCLRT::IndirectHeap *ioh = nullptr;

    commandStream = &commandQueue.getCS(0);

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
    IndirectHeap indirectObjectHeap(dsh->getCpuBase(), dsh->getMaxAvailableSpace());
    indirectObjectHeap.getSpace(curbeOffset);
    ioh = &indirectObjectHeap;

    auto offsetCrossThreadData = KernelCommandsHelper<GfxFamily>::sendIndirectState(
        *commandStream,
        *dsh,
        *ioh,
        *ssh,
        scheduler,
        simd,
        localWorkSizes,
        offsetInterfaceDescriptorTable,
        interfaceDescriptorIndex,
        preemptionMode,
        nullptr);

    // Implement enabling special WA DisableLSQCROPERFforOCL if needed
    GpgpuWalkerHelper<GfxFamily>::applyWADisableLSQCROPERFforOCL(commandStream, scheduler, true);

    // Program the walker.  Invokes execution so all state should already be programmed
    auto pGpGpuWalkerCmd = (GPGPU_WALKER *)commandStream->getSpace(sizeof(GPGPU_WALKER));
    *pGpGpuWalkerCmd = GfxFamily::cmdInitGpgpuWalker;

    size_t globalOffsets[3] = {0, 0, 0};
    size_t workGroups[3] = {(scheduler.getGws() / scheduler.getLws()), 1, 1};
    auto localWorkSize = GpgpuWalkerHelper<GfxFamily>::setGpgpuWalkerThreadData(pGpGpuWalkerCmd, globalOffsets, globalOffsets, workGroups, localWorkSizes, simd);

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
    GpgpuWalkerHelper<GfxFamily>::applyWADisableLSQCROPERFforOCL(commandStream, scheduler, false);

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

template <typename GfxFamily>
void GpgpuWalkerHelper<GfxFamily>::applyWADisableLSQCROPERFforOCL(OCLRT::LinearStream *pCommandStream, const Kernel &kernel, bool disablePerfMode) {
}

template <typename GfxFamily>
size_t GpgpuWalkerHelper<GfxFamily>::getSizeForWADisableLSQCROPERFforOCL(const Kernel *pKernel) {
    return (size_t)0;
}

template <typename GfxFamily>
size_t EnqueueOperation<GfxFamily>::getTotalSizeRequiredCS(bool reserveProfilingCmdsSpace, bool reservePerfCounters, CommandQueue &commandQueue, const MultiDispatchInfo &multiDispatchInfo) {
    size_t size = KernelCommandsHelper<GfxFamily>::getSizeRequiredCS() +
                  sizeof(PIPE_CONTROL) * (KernelCommandsHelper<GfxFamily>::isPipeControlWArequired() ? 2 : 1);
    if (reserveProfilingCmdsSpace) {
        size += 2 * sizeof(PIPE_CONTROL) + 4 * sizeof(typename GfxFamily::MI_STORE_REGISTER_MEM);
    }
    if (reservePerfCounters) {
        //start cmds
        //P_C: flush CS & TimeStamp BEGIN
        size += 2 * sizeof(PIPE_CONTROL);
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
        size += 2 * sizeof(PIPE_CONTROL);
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

template <typename GfxFamily>
size_t EnqueueOperation<GfxFamily>::getSizeRequiredCS(uint32_t cmdType, bool reserveProfilingCmdsSpace, bool reservePerfCounters, CommandQueue &commandQueue, const Kernel *pKernel) {
    switch (cmdType) {
    case CL_COMMAND_MIGRATE_MEM_OBJECTS:
    case CL_COMMAND_MARKER:
        return EnqueueOperation<GfxFamily>::getSizeRequiredCSNonKernel(reserveProfilingCmdsSpace, reservePerfCounters, commandQueue);
    case CL_COMMAND_NDRANGE_KERNEL:
    default:
        return EnqueueOperation<GfxFamily>::getSizeRequiredCSKernel(reserveProfilingCmdsSpace, reservePerfCounters, commandQueue, pKernel);
    }
}

template <typename GfxFamily>
size_t EnqueueOperation<GfxFamily>::getSizeRequiredCSKernel(bool reserveProfilingCmdsSpace, bool reservePerfCounters, CommandQueue &commandQueue, const Kernel *pKernel) {
    size_t size = sizeof(typename GfxFamily::GPGPU_WALKER) + KernelCommandsHelper<GfxFamily>::getSizeRequiredCS() +
                  sizeof(PIPE_CONTROL) * (KernelCommandsHelper<GfxFamily>::isPipeControlWArequired() ? 2 : 1);
    size += PreemptionHelper::getPreemptionWaCsSize<GfxFamily>(commandQueue.getDevice());
    if (reserveProfilingCmdsSpace) {
        size += 2 * sizeof(PIPE_CONTROL) + 2 * sizeof(typename GfxFamily::MI_STORE_REGISTER_MEM);
    }
    if (reservePerfCounters) {
        //start cmds
        //P_C: flush CS & TimeStamp BEGIN
        size += 2 * sizeof(PIPE_CONTROL);
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
        size += 2 * sizeof(PIPE_CONTROL);
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

template <typename GfxFamily>
size_t EnqueueOperation<GfxFamily>::getSizeRequiredCSNonKernel(bool reserveProfilingCmdsSpace, bool reservePerfCounters, CommandQueue &commandQueue) {
    size_t size = 0;
    if (reserveProfilingCmdsSpace) {
        size += 2 * sizeof(PIPE_CONTROL) + 4 * sizeof(typename GfxFamily::MI_STORE_REGISTER_MEM);
    }
    return size;
}

template <typename GfxFamily>
size_t EnqueueOperation<GfxFamily>::getSizeRequiredForTimestampPacketWrite() {
    return 2 * sizeof(PIPE_CONTROL);
}

} // namespace OCLRT
