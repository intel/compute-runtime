/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/command_stream/linear_stream.h"
#include "shared/source/helpers/register_offsets.h"
#include "shared/source/helpers/string.h"

namespace NEO {
template <typename GfxFamily, size_t AluCount>
class EncodeAluHelper {
  public:
    using MI_MATH_ALU_INST_INLINE = typename GfxFamily::MI_MATH_ALU_INST_INLINE;
    using MI_MATH = typename GfxFamily::MI_MATH;

    EncodeAluHelper() {
        aluOps.miMath.DW0.BitField.InstructionType = MI_MATH::COMMAND_TYPE_MI_COMMAND;
        aluOps.miMath.DW0.BitField.InstructionOpcode = MI_MATH::MI_COMMAND_OPCODE_MI_MATH;
        aluOps.miMath.DW0.BitField.DwordLength = AluCount - 1;
    }

    void setNextAlu(AluRegisters opcode) {
        setNextAlu(opcode, AluRegisters::OPCODE_NONE, AluRegisters::OPCODE_NONE);
    }

    void setNextAlu(AluRegisters opcode, AluRegisters operand1, AluRegisters operand2) {
        aluOps.aliInst[aluIndex].DW0.BitField.ALUOpcode = static_cast<uint32_t>(opcode);
        aluOps.aliInst[aluIndex].DW0.BitField.Operand1 = static_cast<uint32_t>(operand1);
        aluOps.aliInst[aluIndex].DW0.BitField.Operand2 = static_cast<uint32_t>(operand2);

        aluIndex++;
    }
    void copyToCmdStream(LinearStream &cmdStream) {
        UNRECOVERABLE_IF(aluIndex != AluCount);

        auto cmds = cmdStream.getSpace(sizeof(AluOps));
        memcpy_s(cmds, sizeof(AluOps), &aluOps, sizeof(AluOps));
    }

    static constexpr size_t getCmdsSize() {
        return (sizeof(MI_MATH) + (AluCount * sizeof(MI_MATH_ALU_INST_INLINE)));
    }

  protected:
    struct alignas(1) AluOps {
        MI_MATH miMath = {};
        MI_MATH_ALU_INST_INLINE aliInst[AluCount];
    } aluOps;

    size_t aluIndex = 0;

    static_assert(sizeof(AluOps) == getCmdsSize(), "This structure is consumed by GPU and must follow specific restrictions for padding and size");
};
} // namespace NEO