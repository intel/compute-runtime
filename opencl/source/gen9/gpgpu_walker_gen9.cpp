/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gen9/hw_cmds_base.h"

#include "opencl/source/command_queue/gpgpu_walker_bdw_and_later.inl"
#include "opencl/source/command_queue/hardware_interface_bdw_and_later.inl"

namespace NEO {

template <>
void GpgpuWalkerHelper<SKLFamily>::applyWADisableLSQCROPERFforOCL(NEO::LinearStream *pCommandStream, const Kernel &kernel, bool disablePerfMode) {
    if (disablePerfMode) {
        if (kernel.getKernelInfo().kernelDescriptor.kernelAttributes.flags.usesFencesForReadWriteImages) {
            // Set bit L3SQC_BIT_LQSC_RO_PERF_DIS in L3SQC_REG4
            GpgpuWalkerHelper<SKLFamily>::addAluReadModifyWriteRegister(pCommandStream, L3SQC_REG4, AluRegisters::OPCODE_OR, L3SQC_BIT_LQSC_RO_PERF_DIS);
        }
    } else {
        if (kernel.getKernelInfo().kernelDescriptor.kernelAttributes.flags.usesFencesForReadWriteImages) {
            // Add PIPE_CONTROL with CS_Stall to wait till GPU finishes its work
            typedef typename SKLFamily::PIPE_CONTROL PIPE_CONTROL;
            auto pipeControlSpace = reinterpret_cast<PIPE_CONTROL *>(pCommandStream->getSpace(sizeof(PIPE_CONTROL)));
            auto pipeControl = SKLFamily::cmdInitPipeControl;
            pipeControl.setCommandStreamerStallEnable(true);
            *pipeControlSpace = pipeControl;

            // Clear bit L3SQC_BIT_LQSC_RO_PERF_DIS in L3SQC_REG4
            GpgpuWalkerHelper<SKLFamily>::addAluReadModifyWriteRegister(pCommandStream, L3SQC_REG4, AluRegisters::OPCODE_AND, ~L3SQC_BIT_LQSC_RO_PERF_DIS);
        }
    }
}

template <>
size_t GpgpuWalkerHelper<SKLFamily>::getSizeForWADisableLSQCROPERFforOCL(const Kernel *pKernel) {
    typedef typename SKLFamily::MI_LOAD_REGISTER_REG MI_LOAD_REGISTER_REG;
    typedef typename SKLFamily::MI_LOAD_REGISTER_IMM MI_LOAD_REGISTER_IMM;
    typedef typename SKLFamily::PIPE_CONTROL PIPE_CONTROL;
    typedef typename SKLFamily::MI_MATH MI_MATH;
    typedef typename SKLFamily::MI_MATH_ALU_INST_INLINE MI_MATH_ALU_INST_INLINE;
    size_t n = 0;
    if (pKernel->getKernelInfo().kernelDescriptor.kernelAttributes.flags.usesFencesForReadWriteImages) {
        n += sizeof(PIPE_CONTROL) +
             (2 * sizeof(MI_LOAD_REGISTER_REG) +
              sizeof(MI_LOAD_REGISTER_IMM) +
              sizeof(PIPE_CONTROL) +
              sizeof(MI_MATH) +
              NUM_ALU_INST_FOR_READ_MODIFY_WRITE * sizeof(MI_MATH_ALU_INST_INLINE)) *
                 2; // For 2 WADisableLSQCROPERFforOCL WAs
    }
    return n;
}

template class HardwareInterface<SKLFamily>;

template class GpgpuWalkerHelper<SKLFamily>;

template struct EnqueueOperation<SKLFamily>;

} // namespace NEO
