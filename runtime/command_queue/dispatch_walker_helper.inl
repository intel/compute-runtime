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

#include "runtime/command_queue/dispatch_walker_helper.h"

namespace OCLRT {

// Performs ReadModifyWrite operation on value of a register: Register = Register Operation Mask
template <typename GfxFamily>
void addAluReadModifyWriteRegister(
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
    typedef typename GfxFamily::PIPE_CONTROL PIPE_CONTROL;
    auto pCmd5 = reinterpret_cast<PIPE_CONTROL *>(pCommandStream->getSpace(sizeof(PIPE_CONTROL)));
    *pCmd5 = PIPE_CONTROL::sInit();
    pCmd5->setCommandStreamerStallEnable(true);
    pCmd5->setDcFlushEnable(true);
    pCmd5->setTextureCacheInvalidationEnable(true);
    pCmd5->setPipeControlFlushEnable(true);
    pCmd5->setStateCacheInvalidationEnable(true);
}
} // namespace OCLRT
