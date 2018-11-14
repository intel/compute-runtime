/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
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
#include "runtime/utilities/tag_allocator.h"
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
inline void GpgpuWalkerHelper<GfxFamily>::dispatchOnDeviceWaitlistSemaphores(LinearStream *commandStream, Device &currentDevice,
                                                                             cl_uint numEventsInWaitList, const cl_event *eventWaitList) {
    for (cl_uint i = 0; i < numEventsInWaitList; i++) {
        auto event = castToObjectOrAbort<Event>(eventWaitList[i]);
        if (event->isUserEvent() || (&event->getCommandQueue()->getDevice() != &currentDevice)) {
            continue;
        }

        for (auto &node : event->getTimestampPacketNodes()->peekNodes()) {
            TimestampPacketHelper::programSemaphoreWithImplicitDependency<GfxFamily>(*commandStream, *node->tag);
        }
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
size_t EnqueueOperation<GfxFamily>::getTotalSizeRequiredCS(uint32_t eventType, cl_uint numEventsInWaitList, bool reserveProfilingCmdsSpace, bool reservePerfCounters, CommandQueue &commandQueue, const MultiDispatchInfo &multiDispatchInfo) {
    size_t expectedSizeCS = 0;
    Kernel *parentKernel = multiDispatchInfo.peekParentKernel();
    for (auto &dispatchInfo : multiDispatchInfo) {
        expectedSizeCS += EnqueueOperation<GfxFamily>::getSizeRequiredCS(eventType, reserveProfilingCmdsSpace, reservePerfCounters, commandQueue, dispatchInfo.getKernel());
    }
    if (parentKernel) {
        SchedulerKernel &scheduler = commandQueue.getDevice().getExecutionEnvironment()->getBuiltIns()->getSchedulerKernel(parentKernel->getContext());
        expectedSizeCS += EnqueueOperation<GfxFamily>::getSizeRequiredCS(eventType, reserveProfilingCmdsSpace, reservePerfCounters, commandQueue, &scheduler);
    }
    if (commandQueue.getDevice().getCommandStreamReceiver().peekTimestampPacketWriteEnabled()) {
        auto semaphoreSize = sizeof(typename GfxFamily::MI_SEMAPHORE_WAIT);
        auto atomicSize = sizeof(typename GfxFamily::MI_ATOMIC);

        expectedSizeCS += EnqueueOperation<GfxFamily>::getSizeRequiredForTimestampPacketWrite();
        expectedSizeCS += numEventsInWaitList * (semaphoreSize + atomicSize);
        if (!commandQueue.isOOQEnabled()) {
            expectedSizeCS += semaphoreSize + atomicSize;
        }
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
    return sizeof(PIPE_CONTROL);
}

} // namespace OCLRT
