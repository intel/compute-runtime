/*
 * Copyright (C) 2022-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/command_stream/linear_stream.h"
#include "shared/source/helpers/register_offsets.h"
#include "shared/source/helpers/string.h"

#include <array>
#include <tuple>

namespace NEO {
template <typename GfxFamily, size_t aluCount>
class EncodeAluHelper {
  public:
    using MI_MATH_ALU_INST_INLINE = typename GfxFamily::MI_MATH_ALU_INST_INLINE;
    using MI_MATH = typename GfxFamily::MI_MATH;

    using OperandTupleT = std::tuple<AluRegisters, AluRegisters, AluRegisters>;
    using OperandArrayT = std::array<OperandTupleT, aluCount>;

    constexpr EncodeAluHelper() {
        aluOps.miMath.DW0.BitField.InstructionType = MI_MATH::COMMAND_TYPE_MI_COMMAND;
        aluOps.miMath.DW0.BitField.InstructionOpcode = MI_MATH::MI_COMMAND_OPCODE_MI_MATH;
        aluOps.miMath.DW0.BitField.DwordLength = aluCount - 1;
    }

    consteval EncodeAluHelper(OperandArrayT inputParams) : EncodeAluHelper() {
        for (auto &param : inputParams) {
            auto &[opcode, operand1, operand2] = param;
            if (opcode == AluRegisters::opcodeNone) {
                break;
            }
            setNextAlu(opcode, operand1, operand2);
        }
    }

    void setMocs([[maybe_unused]] uint32_t mocs) {
        if constexpr (GfxFamily::isUsingMiMathMocs) {
            aluOps.miMath.DW0.BitField.MemoryObjectControlState = mocs;
        }
    }

    constexpr void setNextAlu(AluRegisters opcode) {
        setNextAlu(opcode, AluRegisters::opcodeNone, AluRegisters::opcodeNone);
    }

    constexpr void setNextAlu(AluRegisters opcode, AluRegisters operand1, AluRegisters operand2) {
        aluOps.aliInst[aluIndex].DW0.BitField.ALUOpcode = static_cast<uint32_t>(opcode);
        aluOps.aliInst[aluIndex].DW0.BitField.Operand1 = static_cast<uint32_t>(operand1);
        aluOps.aliInst[aluIndex].DW0.BitField.Operand2 = static_cast<uint32_t>(operand2);

        aluIndex++;
    }
    void copyToCmdStream(LinearStream &cmdStream) const {
        UNRECOVERABLE_IF(aluIndex != aluCount);

        auto cmds = cmdStream.getSpace(sizeof(AluOps));
        memcpy_s(cmds, sizeof(AluOps), &aluOps, sizeof(AluOps));
    }

    static constexpr size_t getCmdsSize() {
        return (sizeof(MI_MATH) + (aluCount * sizeof(MI_MATH_ALU_INST_INLINE)));
    }

  protected:
    struct alignas(1) AluOps {
        MI_MATH miMath = {};
        MI_MATH_ALU_INST_INLINE aliInst[aluCount];
    } aluOps;

    size_t aluIndex = 0;

    static_assert(sizeof(AluOps) == getCmdsSize(), "This structure is consumed by GPU and must follow specific restrictions for padding and size");
};
} // namespace NEO
