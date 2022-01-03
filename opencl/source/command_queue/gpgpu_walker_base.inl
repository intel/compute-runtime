/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/command_container/command_encoder.h"
#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/helpers/aligned_memory.h"
#include "shared/source/helpers/debug_helpers.h"
#include "shared/source/helpers/engine_node_helper.h"
#include "shared/source/helpers/hw_helper.h"
#include "shared/source/helpers/local_id_gen.h"
#include "shared/source/helpers/pipe_control_args.h"
#include "shared/source/indirect_heap/indirect_heap.h"
#include "shared/source/memory_manager/graphics_allocation.h"
#include "shared/source/os_interface/os_context.h"
#include "shared/source/utilities/perf_counter.h"
#include "shared/source/utilities/tag_allocator.h"

#include "opencl/source/command_queue/command_queue.h"
#include "opencl/source/command_queue/command_queue_hw.h"
#include "opencl/source/command_queue/gpgpu_walker.h"
#include "opencl/source/event/user_event.h"
#include "opencl/source/helpers/cl_validators.h"
#include "opencl/source/helpers/hardware_commands_helper.h"
#include "opencl/source/helpers/queue_helpers.h"
#include "opencl/source/mem_obj/mem_obj.h"

#include <algorithm>
#include <cmath>

namespace NEO {

// Performs ReadModifyWrite operation on value of a register: Register = Register Operation Mask
template <typename GfxFamily>
void GpgpuWalkerHelper<GfxFamily>::addAluReadModifyWriteRegister(
    LinearStream *pCommandStream,
    uint32_t aluRegister,
    AluRegisters operation,
    uint32_t mask) {
    // Load "Register" value into CS_GPR_R0
    using MI_LOAD_REGISTER_REG = typename GfxFamily::MI_LOAD_REGISTER_REG;
    using MI_MATH = typename GfxFamily::MI_MATH;
    using MI_MATH_ALU_INST_INLINE = typename GfxFamily::MI_MATH_ALU_INST_INLINE;

    auto pCmd = pCommandStream->getSpaceForCmd<MI_LOAD_REGISTER_REG>();
    MI_LOAD_REGISTER_REG cmdReg = GfxFamily::cmdInitLoadRegisterReg;
    cmdReg.setSourceRegisterAddress(aluRegister);
    cmdReg.setDestinationRegisterAddress(CS_GPR_R0);
    *pCmd = cmdReg;

    // Load "Mask" into CS_GPR_R1
    LriHelper<GfxFamily>::program(pCommandStream,
                                  CS_GPR_R1,
                                  mask,
                                  false);

    // Add instruction MI_MATH with 4 MI_MATH_ALU_INST_INLINE operands
    auto pCmd3 = reinterpret_cast<uint32_t *>(pCommandStream->getSpace(sizeof(MI_MATH) + NUM_ALU_INST_FOR_READ_MODIFY_WRITE * sizeof(MI_MATH_ALU_INST_INLINE)));
    MI_MATH mathCmd;
    mathCmd.DW0.Value = 0x0;
    mathCmd.DW0.BitField.InstructionType = MI_MATH::COMMAND_TYPE_MI_COMMAND;
    mathCmd.DW0.BitField.InstructionOpcode = MI_MATH::MI_COMMAND_OPCODE_MI_MATH;
    // 0x3 - 5 Dwords length cmd (-2): 1 for MI_MATH, 4 for MI_MATH_ALU_INST_INLINE
    mathCmd.DW0.BitField.DwordLength = NUM_ALU_INST_FOR_READ_MODIFY_WRITE - 1;
    *reinterpret_cast<MI_MATH *>(pCmd3) = mathCmd;

    pCmd3++;
    MI_MATH_ALU_INST_INLINE *pAluParam = reinterpret_cast<MI_MATH_ALU_INST_INLINE *>(pCmd3);
    MI_MATH_ALU_INST_INLINE cmdAluParam;
    cmdAluParam.DW0.Value = 0x0;

    // Setup first operand of MI_MATH - load CS_GPR_R0 into register A
    cmdAluParam.DW0.BitField.ALUOpcode =
        static_cast<uint32_t>(AluRegisters::OPCODE_LOAD);
    cmdAluParam.DW0.BitField.Operand1 =
        static_cast<uint32_t>(AluRegisters::R_SRCA);
    cmdAluParam.DW0.BitField.Operand2 =
        static_cast<uint32_t>(AluRegisters::R_0);
    *pAluParam = cmdAluParam;
    pAluParam++;

    cmdAluParam.DW0.Value = 0x0;
    // Setup second operand of MI_MATH - load CS_GPR_R1 into register B
    cmdAluParam.DW0.BitField.ALUOpcode =
        static_cast<uint32_t>(AluRegisters::OPCODE_LOAD);
    cmdAluParam.DW0.BitField.Operand1 =
        static_cast<uint32_t>(AluRegisters::R_SRCB);
    cmdAluParam.DW0.BitField.Operand2 =
        static_cast<uint32_t>(AluRegisters::R_1);
    *pAluParam = cmdAluParam;
    pAluParam++;

    cmdAluParam.DW0.Value = 0x0;
    // Setup third operand of MI_MATH - "Operation" on registers A and B
    cmdAluParam.DW0.BitField.ALUOpcode = static_cast<uint32_t>(operation);
    cmdAluParam.DW0.BitField.Operand1 = 0;
    cmdAluParam.DW0.BitField.Operand2 = 0;
    *pAluParam = cmdAluParam;
    pAluParam++;

    cmdAluParam.DW0.Value = 0x0;
    // Setup fourth operand of MI_MATH - store result into CS_GPR_R0
    cmdAluParam.DW0.BitField.ALUOpcode =
        static_cast<uint32_t>(AluRegisters::OPCODE_STORE);
    cmdAluParam.DW0.BitField.Operand1 =
        static_cast<uint32_t>(AluRegisters::R_0);
    cmdAluParam.DW0.BitField.Operand2 =
        static_cast<uint32_t>(AluRegisters::R_ACCU);
    *pAluParam = cmdAluParam;

    // LOAD value of CS_GPR_R0 into "Register"
    auto pCmd4 = pCommandStream->getSpaceForCmd<MI_LOAD_REGISTER_REG>();
    cmdReg = GfxFamily::cmdInitLoadRegisterReg;
    cmdReg.setSourceRegisterAddress(CS_GPR_R0);
    cmdReg.setDestinationRegisterAddress(aluRegister);
    *pCmd4 = cmdReg;

    // Add PIPE_CONTROL to flush caches
    auto pCmd5 = pCommandStream->getSpaceForCmd<PIPE_CONTROL>();
    PIPE_CONTROL cmdPipeControl = GfxFamily::cmdInitPipeControl;
    cmdPipeControl.setCommandStreamerStallEnable(true);
    cmdPipeControl.setDcFlushEnable(true);
    cmdPipeControl.setTextureCacheInvalidationEnable(true);
    cmdPipeControl.setPipeControlFlushEnable(true);
    cmdPipeControl.setStateCacheInvalidationEnable(true);
    *pCmd5 = cmdPipeControl;
}

template <typename GfxFamily>
void GpgpuWalkerHelper<GfxFamily>::dispatchPerfCountersCommandsStart(
    CommandQueue &commandQueue,
    TagNodeBase &hwPerfCounter,
    LinearStream *commandStream) {

    const auto pPerformanceCounters = commandQueue.getPerfCounters();
    const auto commandBufferType = EngineHelpers::isCcs(commandQueue.getGpgpuEngine().osContext->getEngineType())
                                       ? MetricsLibraryApi::GpuCommandBufferType::Compute
                                       : MetricsLibraryApi::GpuCommandBufferType::Render;
    const uint32_t size = pPerformanceCounters->getGpuCommandsSize(commandBufferType, true);
    void *pBuffer = commandStream->getSpace(size);

    pPerformanceCounters->getGpuCommands(commandBufferType, hwPerfCounter, true, size, pBuffer);
}

template <typename GfxFamily>
void GpgpuWalkerHelper<GfxFamily>::dispatchPerfCountersCommandsEnd(
    CommandQueue &commandQueue,
    TagNodeBase &hwPerfCounter,
    LinearStream *commandStream) {

    const auto pPerformanceCounters = commandQueue.getPerfCounters();
    const auto commandBufferType = EngineHelpers::isCcs(commandQueue.getGpgpuEngine().osContext->getEngineType())
                                       ? MetricsLibraryApi::GpuCommandBufferType::Compute
                                       : MetricsLibraryApi::GpuCommandBufferType::Render;
    const uint32_t size = pPerformanceCounters->getGpuCommandsSize(commandBufferType, false);
    void *pBuffer = commandStream->getSpace(size);

    pPerformanceCounters->getGpuCommands(commandBufferType, hwPerfCounter, false, size, pBuffer);
}

template <typename GfxFamily>
void GpgpuWalkerHelper<GfxFamily>::applyWADisableLSQCROPERFforOCL(NEO::LinearStream *pCommandStream, const Kernel &kernel, bool disablePerfMode) {
}

template <typename GfxFamily>
size_t GpgpuWalkerHelper<GfxFamily>::getSizeForWADisableLSQCROPERFforOCL(const Kernel *pKernel) {
    return (size_t)0;
}

template <typename GfxFamily>
size_t GpgpuWalkerHelper<GfxFamily>::getSizeForWaDisableRccRhwoOptimization(const Kernel *pKernel) {
    return 0u;
}

template <typename GfxFamily>
size_t EnqueueOperation<GfxFamily>::getTotalSizeRequiredCS(uint32_t eventType, const CsrDependencies &csrDeps, bool reserveProfilingCmdsSpace, bool reservePerfCounters, bool blitEnqueue, CommandQueue &commandQueue, const MultiDispatchInfo &multiDispatchInfo, bool isMarkerWithProfiling, bool eventsInWaitlist) {
    size_t expectedSizeCS = 0;
    auto &hwInfo = commandQueue.getDevice().getHardwareInfo();
    auto &commandQueueHw = static_cast<CommandQueueHw<GfxFamily> &>(commandQueue);

    if (blitEnqueue) {
        size_t expectedSizeCS = TimestampPacketHelper::getRequiredCmdStreamSizeForNodeDependencyWithBlitEnqueue<GfxFamily>();
        if (commandQueueHw.isCacheFlushForBcsRequired()) {
            expectedSizeCS += MemorySynchronizationCommands<GfxFamily>::getSizeForPipeControlWithPostSyncOperation(hwInfo);
        }

        return expectedSizeCS;
    }

    for (auto &dispatchInfo : multiDispatchInfo) {
        expectedSizeCS += EnqueueOperation<GfxFamily>::getSizeRequiredCS(eventType, reserveProfilingCmdsSpace, reservePerfCounters, commandQueue, dispatchInfo.getKernel(), dispatchInfo);
        size_t kernelObjAuxCount = multiDispatchInfo.getKernelObjsForAuxTranslation() != nullptr ? multiDispatchInfo.getKernelObjsForAuxTranslation()->size() : 0;
        expectedSizeCS += dispatchInfo.dispatchInitCommands.estimateCommandsSize(kernelObjAuxCount, hwInfo, commandQueueHw.isCacheFlushForBcsRequired());
        expectedSizeCS += dispatchInfo.dispatchEpilogueCommands.estimateCommandsSize(kernelObjAuxCount, hwInfo, commandQueueHw.isCacheFlushForBcsRequired());
    }
    if (commandQueue.getGpgpuCommandStreamReceiver().peekTimestampPacketWriteEnabled()) {
        expectedSizeCS += TimestampPacketHelper::getRequiredCmdStreamSize<GfxFamily>(csrDeps);
        expectedSizeCS += EnqueueOperation<GfxFamily>::getSizeRequiredForTimestampPacketWrite();
        if (isMarkerWithProfiling) {
            if (!eventsInWaitlist) {
                expectedSizeCS += commandQueue.getGpgpuCommandStreamReceiver().getCmdsSizeForComputeBarrierCommand();
            }
            expectedSizeCS += 4 * EncodeStoreMMIO<GfxFamily>::size;
        }
    } else if (isMarkerWithProfiling) {
        expectedSizeCS += 2 * MemorySynchronizationCommands<GfxFamily>::getSizeForSinglePipeControl();
        if (!HwHelper::get(hwInfo.platform.eRenderCoreFamily).useOnlyGlobalTimestamps()) {
            expectedSizeCS += 2 * EncodeStoreMMIO<GfxFamily>::size;
        }
    }
    if (multiDispatchInfo.peekMainKernel()) {
        expectedSizeCS += EnqueueOperation<GfxFamily>::getSizeForCacheFlushAfterWalkerCommands(*multiDispatchInfo.peekMainKernel(), commandQueue);
    }

    if (DebugManager.flags.PauseOnEnqueue.get() != -1) {
        expectedSizeCS += MemorySynchronizationCommands<GfxFamily>::getSizeForSinglePipeControl() * 2;
        expectedSizeCS += sizeof(typename GfxFamily::MI_SEMAPHORE_WAIT) * 2;
    }

    if (DebugManager.flags.GpuScratchRegWriteAfterWalker.get() != -1) {
        expectedSizeCS += sizeof(typename GfxFamily::MI_LOAD_REGISTER_IMM);
    }

    expectedSizeCS += TimestampPacketHelper::getRequiredCmdStreamSizeForTaskCountContainer<GfxFamily>(csrDeps);

    return expectedSizeCS;
}

template <typename GfxFamily>
size_t EnqueueOperation<GfxFamily>::getSizeRequiredCS(uint32_t cmdType, bool reserveProfilingCmdsSpace, bool reservePerfCounters, CommandQueue &commandQueue, const Kernel *pKernel, const DispatchInfo &dispatchInfo) {
    if (isCommandWithoutKernel(cmdType)) {
        return EnqueueOperation<GfxFamily>::getSizeRequiredCSNonKernel(reserveProfilingCmdsSpace, reservePerfCounters, commandQueue);
    } else {
        return EnqueueOperation<GfxFamily>::getSizeRequiredCSKernel(reserveProfilingCmdsSpace, reservePerfCounters, commandQueue, pKernel, dispatchInfo);
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
