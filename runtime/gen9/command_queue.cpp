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

#include "runtime/memory_manager/svm_memory_manager.h"
#include "runtime/command_queue/command_queue_hw.h"
#include "runtime/command_queue/command_queue_hw.inl"
#include "runtime/command_queue/dispatch_walker_helper.h"
#include "runtime/command_queue/dispatch_walker_helper.inl"

namespace OCLRT {

typedef SKLFamily Family;
static auto gfxCore = IGFX_GEN9_CORE;

template <>
void populateFactoryTable<CommandQueueHw<Family>>() {
    extern CommandQueueCreateFunc commandQueueFactory[IGFX_MAX_CORE];
    commandQueueFactory[gfxCore] = CommandQueueHw<Family>::create;
}

template <>
void applyWADisableLSQCROPERFforOCL<Family>(OCLRT::LinearStream *pCommandStream, const Kernel &kernel, bool disablePerfMode) {
    if (disablePerfMode) {
        if (kernel.getKernelInfo().patchInfo.executionEnvironment->UsesFencesForReadWriteImages) {
            // Set bit L3SQC_BIT_LQSC_RO_PERF_DIS in L3SQC_REG4
            addAluReadModifyWriteRegister<Family>(pCommandStream, L3SQC_REG4, ALU_OPCODE_OR, L3SQC_BIT_LQSC_RO_PERF_DIS);
        }
    } else {
        if (kernel.getKernelInfo().patchInfo.executionEnvironment->UsesFencesForReadWriteImages) {
            // Add PIPE_CONTROL with CS_Stall to wait till GPU finishes its work
            typedef typename Family::PIPE_CONTROL PIPE_CONTROL;
            auto pCmd = reinterpret_cast<PIPE_CONTROL *>(pCommandStream->getSpace(sizeof(PIPE_CONTROL)));
            *pCmd = PIPE_CONTROL::sInit();
            pCmd->setCommandStreamerStallEnable(true);
            // Clear bit L3SQC_BIT_LQSC_RO_PERF_DIS in L3SQC_REG4
            addAluReadModifyWriteRegister<Family>(pCommandStream, L3SQC_REG4, ALU_OPCODE_AND, ~L3SQC_BIT_LQSC_RO_PERF_DIS);
        }
    }
}

template <>
size_t getSizeForWADisableLSQCROPERFforOCL<Family>(const Kernel *pKernel) {
    typedef typename Family::MI_LOAD_REGISTER_REG MI_LOAD_REGISTER_REG;
    typedef typename Family::MI_LOAD_REGISTER_IMM MI_LOAD_REGISTER_IMM;
    typedef typename Family::PIPE_CONTROL PIPE_CONTROL;
    typedef typename Family::MI_MATH MI_MATH;
    typedef typename Family::MI_MATH_ALU_INST_INLINE MI_MATH_ALU_INST_INLINE;
    size_t n = 0;
    if ((pKernel != nullptr) && pKernel->getKernelInfo().patchInfo.executionEnvironment->UsesFencesForReadWriteImages) {
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
} // namespace OCLRT
