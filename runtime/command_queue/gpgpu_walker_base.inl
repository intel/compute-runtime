/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "runtime/command_queue/command_queue.h"
#include "runtime/command_queue/gpgpu_walker.h"
#include "runtime/command_queue/local_id_gen.h"
#include "runtime/command_stream/command_stream_receiver.h"
#include "runtime/device/device_info.h"
#include "runtime/event/perf_counter.h"
#include "runtime/event/user_event.h"
#include "runtime/helpers/aligned_memory.h"
#include "runtime/helpers/debug_helpers.h"
#include "runtime/helpers/hardware_commands_helper.h"
#include "runtime/helpers/hw_helper.h"
#include "runtime/helpers/queue_helpers.h"
#include "runtime/helpers/validators.h"
#include "runtime/indirect_heap/indirect_heap.h"
#include "runtime/mem_obj/mem_obj.h"
#include "runtime/memory_manager/graphics_allocation.h"
#include "runtime/utilities/tag_allocator.h"

#include "instrumentation.h"

#include <algorithm>
#include <cmath>

namespace NEO {

// Performs ReadModifyWrite operation on value of a register: Register = Register Operation Mask
template <typename GfxFamily>
void GpgpuWalkerHelper<GfxFamily>::addAluReadModifyWriteRegister(
    NEO::LinearStream *pCommandStream,
    uint32_t aluRegister,
    uint32_t operation,
    uint32_t mask) {
    // Load "Register" value into CS_GPR_R0
    typedef typename GfxFamily::MI_LOAD_REGISTER_REG MI_LOAD_REGISTER_REG;
    typedef typename GfxFamily::MI_MATH MI_MATH;
    typedef typename GfxFamily::MI_MATH_ALU_INST_INLINE MI_MATH_ALU_INST_INLINE;
    auto pCmd = pCommandStream->getSpaceForCmd<MI_LOAD_REGISTER_REG>();
    *pCmd = GfxFamily::cmdInitLoadRegisterReg;
    pCmd->setSourceRegisterAddress(aluRegister);
    pCmd->setDestinationRegisterAddress(CS_GPR_R0);

    // Load "Mask" into CS_GPR_R1
    typedef typename GfxFamily::MI_LOAD_REGISTER_IMM MI_LOAD_REGISTER_IMM;
    auto pCmd2 = pCommandStream->getSpaceForCmd<MI_LOAD_REGISTER_IMM>();
    *pCmd2 = GfxFamily::cmdInitLoadRegisterImm;
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
    auto pCmd4 = pCommandStream->getSpaceForCmd<MI_LOAD_REGISTER_REG>();
    *pCmd4 = GfxFamily::cmdInitLoadRegisterReg;
    pCmd4->setSourceRegisterAddress(CS_GPR_R0);
    pCmd4->setDestinationRegisterAddress(aluRegister);

    // Add PIPE_CONTROL to flush caches
    auto pCmd5 = pCommandStream->getSpaceForCmd<PIPE_CONTROL>();
    *pCmd5 = GfxFamily::cmdInitPipeControl;
    pCmd5->setCommandStreamerStallEnable(true);
    pCmd5->setDcFlushEnable(true);
    pCmd5->setTextureCacheInvalidationEnable(true);
    pCmd5->setPipeControlFlushEnable(true);
    pCmd5->setStateCacheInvalidationEnable(true);
}

template <typename GfxFamily>
void GpgpuWalkerHelper<GfxFamily>::dispatchProfilingCommandsStart(
    TagNode<HwTimeStamps> &hwTimeStamps,
    LinearStream *commandStream) {

    using MI_STORE_REGISTER_MEM = typename GfxFamily::MI_STORE_REGISTER_MEM;

    // PIPE_CONTROL for global timestamp
    uint64_t timeStampAddress = hwTimeStamps.getGpuAddress() + offsetof(HwTimeStamps, GlobalStartTS);

    PipeControlHelper<GfxFamily>::obtainPipeControlAndProgramPostSyncOperation(commandStream, PIPE_CONTROL::POST_SYNC_OPERATION_WRITE_TIMESTAMP, timeStampAddress, 0llu, false);

    //MI_STORE_REGISTER_MEM for context local timestamp
    timeStampAddress = hwTimeStamps.getGpuAddress() + offsetof(HwTimeStamps, ContextStartTS);

    //low part
    auto pMICmdLow = commandStream->getSpaceForCmd<MI_STORE_REGISTER_MEM>();
    *pMICmdLow = GfxFamily::cmdInitStoreRegisterMem;
    adjustMiStoreRegMemMode(pMICmdLow);
    pMICmdLow->setRegisterAddress(GP_THREAD_TIME_REG_ADDRESS_OFFSET_LOW);
    pMICmdLow->setMemoryAddress(timeStampAddress);
}

template <typename GfxFamily>
void GpgpuWalkerHelper<GfxFamily>::dispatchProfilingCommandsEnd(
    TagNode<HwTimeStamps> &hwTimeStamps,
    LinearStream *commandStream) {

    using MI_STORE_REGISTER_MEM = typename GfxFamily::MI_STORE_REGISTER_MEM;

    // PIPE_CONTROL for global timestamp
    auto pPipeControlCmd = commandStream->getSpaceForCmd<PIPE_CONTROL>();
    *pPipeControlCmd = GfxFamily::cmdInitPipeControl;
    pPipeControlCmd->setCommandStreamerStallEnable(true);

    //MI_STORE_REGISTER_MEM for context local timestamp
    uint64_t timeStampAddress = hwTimeStamps.getGpuAddress() + offsetof(HwTimeStamps, ContextEndTS);

    //low part
    auto pMICmdLow = commandStream->getSpaceForCmd<MI_STORE_REGISTER_MEM>();
    *pMICmdLow = GfxFamily::cmdInitStoreRegisterMem;
    adjustMiStoreRegMemMode(pMICmdLow);
    pMICmdLow->setRegisterAddress(GP_THREAD_TIME_REG_ADDRESS_OFFSET_LOW);
    pMICmdLow->setMemoryAddress(timeStampAddress);
}

template <typename GfxFamily>
void GpgpuWalkerHelper<GfxFamily>::dispatchStoreRegisterCommand(
    LinearStream *commandStream,
    uint64_t memoryAddress,
    uint32_t registerAddress) {

    using MI_STORE_REGISTER_MEM = typename GfxFamily::MI_STORE_REGISTER_MEM;

    auto pCmd = commandStream->getSpaceForCmd<MI_STORE_REGISTER_MEM>();
    *pCmd = GfxFamily::cmdInitStoreRegisterMem;
    pCmd->setRegisterAddress(registerAddress);
    pCmd->setMemoryAddress(memoryAddress);
}

template <typename GfxFamily>
void GpgpuWalkerHelper<GfxFamily>::dispatchPerfCountersGeneralPurposeCounterCommands(
    LinearStream *commandStream,
    uint64_t baseAddress) {

    // Read General Purpose counters
    for (auto i = 0u; i < NEO::INSTR_GENERAL_PURPOSE_COUNTERS_COUNT; i++) {
        uint32_t regAddr = INSTR_GFX_OFFSETS::INSTR_PERF_CNT_1_DW0 + i * sizeof(cl_uint);
        //Gp field is 2*uint64 wide so it can hold 4 uint32
        uint64_t address = baseAddress + i * sizeof(cl_uint);
        dispatchStoreRegisterCommand(commandStream, address, regAddr);
    }
}

template <typename GfxFamily>
void GpgpuWalkerHelper<GfxFamily>::dispatchPerfCountersUserCounterCommands(
    CommandQueue &commandQueue,
    LinearStream *commandStream,
    uint64_t baseAddress) {

    auto userRegs = &commandQueue.getPerfCountersConfigData()->ReadRegs;

    for (uint32_t i = 0; i < userRegs->RegsCount; i++) {
        uint32_t regAddr = userRegs->Reg[i].Offset;
        //offset between base (low) registers is cl_ulong wide
        uint64_t address = baseAddress + i * sizeof(cl_ulong);
        dispatchStoreRegisterCommand(commandStream, address, regAddr);

        if (userRegs->Reg[i].BitSize > 32) {
            dispatchStoreRegisterCommand(commandStream, address + sizeof(cl_uint), regAddr + sizeof(cl_uint));
        }
    }
}

template <typename GfxFamily>
void GpgpuWalkerHelper<GfxFamily>::dispatchPerfCountersOABufferStateCommands(
    TagNode<HwPerfCounter> &hwPerfCounter,
    LinearStream *commandStream) {

    dispatchStoreRegisterCommand(commandStream, hwPerfCounter.getGpuAddress() + offsetof(HwPerfCounter, HWPerfCounters.OaStatus), INSTR_GFX_OFFSETS::INSTR_OA_STATUS);
    dispatchStoreRegisterCommand(commandStream, hwPerfCounter.getGpuAddress() + offsetof(HwPerfCounter, HWPerfCounters.OaHead), INSTR_GFX_OFFSETS::INSTR_OA_HEAD_PTR);
    dispatchStoreRegisterCommand(commandStream, hwPerfCounter.getGpuAddress() + offsetof(HwPerfCounter, HWPerfCounters.OaTail), INSTR_GFX_OFFSETS::INSTR_OA_TAIL_PTR);
}

template <typename GfxFamily>
void GpgpuWalkerHelper<GfxFamily>::dispatchPerfCountersCommandsStart(
    CommandQueue &commandQueue,
    TagNode<HwPerfCounter> &hwPerfCounter,
    LinearStream *commandStream) {

    using MI_REPORT_PERF_COUNT = typename GfxFamily::MI_REPORT_PERF_COUNT;

    auto perfCounters = commandQueue.getPerfCounters();

    uint32_t currentReportId = perfCounters->getCurrentReportId();
    uint64_t address = 0;
    //flush command streamer
    auto pPipeControlCmd = commandStream->getSpaceForCmd<PIPE_CONTROL>();
    *pPipeControlCmd = GfxFamily::cmdInitPipeControl;
    pPipeControlCmd->setCommandStreamerStallEnable(true);

    //Store value of NOOPID register
    GpgpuWalkerHelper<GfxFamily>::dispatchStoreRegisterCommand(commandStream, hwPerfCounter.getGpuAddress() + offsetof(HwPerfCounter, HWPerfCounters.DMAFenceIdBegin), INSTR_MMIO_NOOPID);

    //Read Core Frequency
    GpgpuWalkerHelper<GfxFamily>::dispatchStoreRegisterCommand(commandStream, hwPerfCounter.getGpuAddress() + offsetof(HwPerfCounter, HWPerfCounters.CoreFreqBegin), INSTR_MMIO_RPSTAT1);

    GpgpuWalkerHelper<GfxFamily>::dispatchPerfCountersGeneralPurposeCounterCommands(commandStream, hwPerfCounter.getGpuAddress() + offsetof(HwPerfCounter, HWPerfCounters.HwPerfReportBegin.Gp));

    auto pReportPerfCount = commandStream->getSpaceForCmd<MI_REPORT_PERF_COUNT>();
    *pReportPerfCount = GfxFamily::cmdInitReportPerfCount;
    pReportPerfCount->setReportId(currentReportId);
    address = hwPerfCounter.getGpuAddress() + offsetof(HwPerfCounter, HWPerfCounters.HwPerfReportBegin.Oa);
    pReportPerfCount->setMemoryAddress(address);

    address = hwPerfCounter.getGpuAddress() + offsetof(HwPerfCounter, HWTimeStamp.GlobalStartTS);

    PipeControlHelper<GfxFamily>::obtainPipeControlAndProgramPostSyncOperation(commandStream, PIPE_CONTROL::POST_SYNC_OPERATION_WRITE_TIMESTAMP, address, 0llu, false);

    GpgpuWalkerHelper<GfxFamily>::dispatchPerfCountersUserCounterCommands(commandQueue, commandStream, hwPerfCounter.getGpuAddress() + offsetof(HwPerfCounter, HWPerfCounters.HwPerfReportBegin.User));

    commandQueue.sendPerfCountersConfig();
}

template <typename GfxFamily>
void GpgpuWalkerHelper<GfxFamily>::dispatchPerfCountersCommandsEnd(
    CommandQueue &commandQueue,
    TagNode<HwPerfCounter> &hwPerfCounter,
    LinearStream *commandStream) {

    using MI_REPORT_PERF_COUNT = typename GfxFamily::MI_REPORT_PERF_COUNT;

    auto perfCounters = commandQueue.getPerfCounters();

    uint32_t currentReportId = perfCounters->getCurrentReportId();

    //flush command streamer
    auto pPipeControlCmd = commandStream->getSpaceForCmd<PIPE_CONTROL>();
    *pPipeControlCmd = GfxFamily::cmdInitPipeControl;
    pPipeControlCmd->setCommandStreamerStallEnable(true);

    GpgpuWalkerHelper<GfxFamily>::dispatchPerfCountersOABufferStateCommands(hwPerfCounter, commandStream);

    //Timestamp: Global End
    uint64_t address = hwPerfCounter.getGpuAddress() + offsetof(HwPerfCounter, HWTimeStamp.GlobalEndTS);
    PipeControlHelper<GfxFamily>::obtainPipeControlAndProgramPostSyncOperation(commandStream, PIPE_CONTROL::POST_SYNC_OPERATION_WRITE_TIMESTAMP, address, 0llu, false);

    auto pReportPerfCount = commandStream->getSpaceForCmd<MI_REPORT_PERF_COUNT>();
    *pReportPerfCount = GfxFamily::cmdInitReportPerfCount;
    pReportPerfCount->setReportId(currentReportId);
    address = hwPerfCounter.getGpuAddress() + offsetof(HwPerfCounter, HWPerfCounters.HwPerfReportEnd.Oa);
    pReportPerfCount->setMemoryAddress(address);

    GpgpuWalkerHelper<GfxFamily>::dispatchPerfCountersGeneralPurposeCounterCommands(commandStream, hwPerfCounter.getGpuAddress() + offsetof(HwPerfCounter, HWPerfCounters.HwPerfReportEnd.Gp));

    //Store value of NOOPID register
    GpgpuWalkerHelper<GfxFamily>::dispatchStoreRegisterCommand(commandStream, hwPerfCounter.getGpuAddress() + offsetof(HwPerfCounter, HWPerfCounters.DMAFenceIdEnd), INSTR_MMIO_NOOPID);

    //Read Core Frequency
    GpgpuWalkerHelper<GfxFamily>::dispatchStoreRegisterCommand(commandStream, hwPerfCounter.getGpuAddress() + offsetof(HwPerfCounter, HWPerfCounters.CoreFreqEnd), INSTR_MMIO_RPSTAT1);

    GpgpuWalkerHelper<GfxFamily>::dispatchPerfCountersUserCounterCommands(commandQueue, commandStream, hwPerfCounter.getGpuAddress() + offsetof(HwPerfCounter, HWPerfCounters.HwPerfReportEnd.User));

    perfCounters->setCpuTimestamp();
}

template <typename GfxFamily>
void GpgpuWalkerHelper<GfxFamily>::applyWADisableLSQCROPERFforOCL(NEO::LinearStream *pCommandStream, const Kernel &kernel, bool disablePerfMode) {
}

template <typename GfxFamily>
size_t GpgpuWalkerHelper<GfxFamily>::getSizeForWADisableLSQCROPERFforOCL(const Kernel *pKernel) {
    return (size_t)0;
}

template <typename GfxFamily>
void GpgpuWalkerHelper<GfxFamily>::adjustMiStoreRegMemMode(MI_STORE_REG_MEM<GfxFamily> *storeCmd) {
}

template <typename GfxFamily>
size_t EnqueueOperation<GfxFamily>::getTotalSizeRequiredCS(uint32_t eventType, const CsrDependencies &csrDeps, bool reserveProfilingCmdsSpace, bool reservePerfCounters, CommandQueue &commandQueue, const MultiDispatchInfo &multiDispatchInfo) {
    size_t expectedSizeCS = 0;
    Kernel *parentKernel = multiDispatchInfo.peekParentKernel();
    if (multiDispatchInfo.peekMainKernel() && multiDispatchInfo.peekMainKernel()->isAuxTranslationRequired()) {
        expectedSizeCS += sizeof(PIPE_CONTROL);
    }
    for (auto &dispatchInfo : multiDispatchInfo) {
        expectedSizeCS += EnqueueOperation<GfxFamily>::getSizeRequiredCS(eventType, reserveProfilingCmdsSpace, reservePerfCounters, commandQueue, dispatchInfo.getKernel());
        if (dispatchInfo.isPipeControlRequired()) {
            expectedSizeCS += sizeof(PIPE_CONTROL);
        }
    }
    if (parentKernel) {
        SchedulerKernel &scheduler = commandQueue.getDevice().getExecutionEnvironment()->getBuiltIns()->getSchedulerKernel(parentKernel->getContext());
        expectedSizeCS += EnqueueOperation<GfxFamily>::getSizeRequiredCS(eventType, reserveProfilingCmdsSpace, reservePerfCounters, commandQueue, &scheduler);
    }
    if (commandQueue.getCommandStreamReceiver().peekTimestampPacketWriteEnabled()) {
        expectedSizeCS += EnqueueOperation<GfxFamily>::getSizeRequiredForTimestampPacketWrite();
        expectedSizeCS += TimestampPacketHelper::getRequiredCmdStreamSize<GfxFamily>(csrDeps);
    }
    return expectedSizeCS;
}

template <typename GfxFamily>
size_t EnqueueOperation<GfxFamily>::getSizeRequiredCS(uint32_t cmdType, bool reserveProfilingCmdsSpace, bool reservePerfCounters, CommandQueue &commandQueue, const Kernel *pKernel) {
    if (isCommandWithoutKernel(cmdType)) {
        return EnqueueOperation<GfxFamily>::getSizeRequiredCSNonKernel(reserveProfilingCmdsSpace, reservePerfCounters, commandQueue);
    } else {
        return EnqueueOperation<GfxFamily>::getSizeRequiredCSKernel(reserveProfilingCmdsSpace, reservePerfCounters, commandQueue, pKernel);
    }
}

template <typename GfxFamily>
size_t EnqueueOperation<GfxFamily>::getSizeRequiredCSNonKernel(bool reserveProfilingCmdsSpace, bool reservePerfCounters, CommandQueue &commandQueue) {
    size_t size = 0;
    if (reserveProfilingCmdsSpace) {
        size += 2 * sizeof(PIPE_CONTROL) + 4 * sizeof(typename GfxFamily::MI_STORE_REGISTER_MEM);
    }
    return size;
}

} // namespace NEO
