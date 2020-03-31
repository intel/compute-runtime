/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/command_encoder.h"
#include "shared/source/helpers/register_offsets.h"
#include "shared/test/unit_test/cmd_parse/gen_cmd_parse.h"

#include "opencl/test/unit_test/fixtures/device_fixture.h"
#include "test.h"

using namespace NEO;

using EncodeMathMMIOTest = testing::Test;

HWTEST_F(EncodeMathMMIOTest, encodeAluAddHasCorrectOpcodesOperands) {
    using MI_MATH_ALU_INST_INLINE = typename FamilyType::MI_MATH_ALU_INST_INLINE;

    MI_MATH_ALU_INST_INLINE aluParam[5];
    AluRegisters regA = AluRegisters::R_0;
    AluRegisters regB = AluRegisters::R_1;
    AluRegisters finalResultRegister = AluRegisters::R_2;

    memset(aluParam, 0, sizeof(MI_MATH_ALU_INST_INLINE) * 5);

    EncodeMathMMIO<FamilyType>::encodeAluAdd(aluParam, regA, regB,
                                             finalResultRegister);

    EXPECT_EQ(aluParam[0].DW0.BitField.ALUOpcode,
              static_cast<uint32_t>(AluRegisters::OPCODE_LOAD));
    EXPECT_EQ(aluParam[0].DW0.BitField.Operand1,
              static_cast<uint32_t>(AluRegisters::R_SRCA));
    EXPECT_EQ(aluParam[0].DW0.BitField.Operand2, static_cast<uint32_t>(regA));

    EXPECT_EQ(aluParam[1].DW0.BitField.ALUOpcode,
              static_cast<uint32_t>(AluRegisters::OPCODE_LOAD));
    EXPECT_EQ(aluParam[1].DW0.BitField.Operand1,
              static_cast<uint32_t>(AluRegisters::R_SRCB));
    EXPECT_EQ(aluParam[1].DW0.BitField.Operand2,
              static_cast<uint32_t>(regB));

    EXPECT_EQ(aluParam[2].DW0.BitField.ALUOpcode,
              static_cast<uint32_t>(AluRegisters::OPCODE_ADD));
    EXPECT_EQ(aluParam[2].DW0.BitField.Operand1, 0u);
    EXPECT_EQ(aluParam[2].DW0.BitField.Operand2, 0u);

    EXPECT_EQ(aluParam[3].DW0.BitField.ALUOpcode,
              static_cast<uint32_t>(AluRegisters::OPCODE_STORE));
    EXPECT_EQ(aluParam[3].DW0.BitField.Operand1,
              static_cast<uint32_t>(AluRegisters::R_2));
    EXPECT_EQ(aluParam[3].DW0.BitField.Operand2,
              static_cast<uint32_t>(AluRegisters::R_ACCU));

    EXPECT_EQ(aluParam[4].DW0.Value, 0u);
}

HWTEST_F(EncodeMathMMIOTest, encodeAluSubStoreCarryHasCorrectOpcodesOperands) {
    using MI_MATH_ALU_INST_INLINE = typename FamilyType::MI_MATH_ALU_INST_INLINE;

    MI_MATH_ALU_INST_INLINE aluParam[5];
    AluRegisters regA = AluRegisters::R_0;
    AluRegisters regB = AluRegisters::R_1;
    AluRegisters finalResultRegister = AluRegisters::R_2;

    memset(aluParam, 0, sizeof(MI_MATH_ALU_INST_INLINE) * 5);

    EncodeMathMMIO<FamilyType>::encodeAluSubStoreCarry(aluParam, regA, regB,
                                                       finalResultRegister);

    EXPECT_EQ(aluParam[0].DW0.BitField.ALUOpcode,
              static_cast<uint32_t>(AluRegisters::OPCODE_LOAD));
    EXPECT_EQ(aluParam[0].DW0.BitField.Operand1,
              static_cast<uint32_t>(AluRegisters::R_SRCA));
    EXPECT_EQ(aluParam[0].DW0.BitField.Operand2,
              static_cast<uint32_t>(regA));

    EXPECT_EQ(aluParam[1].DW0.BitField.ALUOpcode,
              static_cast<uint32_t>(AluRegisters::OPCODE_LOAD));
    EXPECT_EQ(aluParam[1].DW0.BitField.Operand1,
              static_cast<uint32_t>(AluRegisters::R_SRCB));
    EXPECT_EQ(aluParam[1].DW0.BitField.Operand2,
              static_cast<uint32_t>(regB));

    EXPECT_EQ(aluParam[2].DW0.BitField.ALUOpcode,
              static_cast<uint32_t>(AluRegisters::OPCODE_SUB));
    EXPECT_EQ(aluParam[2].DW0.BitField.Operand1, 0u);
    EXPECT_EQ(aluParam[2].DW0.BitField.Operand2, 0u);

    EXPECT_EQ(aluParam[3].DW0.BitField.ALUOpcode,
              static_cast<uint32_t>(AluRegisters::OPCODE_STORE));
    EXPECT_EQ(aluParam[3].DW0.BitField.Operand1,
              static_cast<uint32_t>(AluRegisters::R_2));
    EXPECT_EQ(aluParam[3].DW0.BitField.Operand2,
              static_cast<uint32_t>(AluRegisters::R_CF));

    EXPECT_EQ(aluParam[4].DW0.Value, 0u);
}

using CommandEncoderMathTest = Test<DeviceFixture>;

HWTEST_F(CommandEncoderMathTest, commandReserve) {
    using MI_MATH = typename FamilyType::MI_MATH;
    GenCmdList commands;
    CommandContainer cmdContainer;

    cmdContainer.initialize(pDevice);

    EncodeMath<FamilyType>::commandReserve(cmdContainer);

    CmdParse<FamilyType>::parseCommandBuffer(commands,
                                             ptrOffset(cmdContainer.getCommandStream()->getCpuBase(), 0),
                                             cmdContainer.getCommandStream()->getUsed());

    auto itor = commands.begin();
    itor = find<MI_MATH *>(itor, commands.end());
    ASSERT_NE(itor, commands.end());

    auto cmdMATH = genCmdCast<MI_MATH *>(*itor);

    EXPECT_EQ(cmdMATH->DW0.BitField.InstructionType,
              static_cast<uint32_t>(MI_MATH::COMMAND_TYPE_MI_COMMAND));
    EXPECT_EQ(cmdMATH->DW0.BitField.InstructionOpcode,
              static_cast<uint32_t>(MI_MATH::MI_COMMAND_OPCODE_MI_MATH));
    EXPECT_EQ(cmdMATH->DW0.BitField.DwordLength,
              static_cast<uint32_t>(NUM_ALU_INST_FOR_READ_MODIFY_WRITE - 1));
}

HWTEST_F(CommandEncoderMathTest, appendsAGreaterThanPredicate) {
    using MI_LOAD_REGISTER_MEM = typename FamilyType::MI_LOAD_REGISTER_MEM;
    using MI_LOAD_REGISTER_IMM = typename FamilyType::MI_LOAD_REGISTER_IMM;
    using MI_LOAD_REGISTER_REG = typename FamilyType::MI_LOAD_REGISTER_REG;
    using MI_MATH = typename FamilyType::MI_MATH;
    using MI_MATH_ALU_INST_INLINE = typename FamilyType::MI_MATH_ALU_INST_INLINE;

    CommandContainer cmdContainer;
    cmdContainer.initialize(pDevice);

    EncodeMathMMIO<FamilyType>::encodeGreaterThanPredicate(cmdContainer, 0xDEADBEEFCAF0u, 17u);

    GenCmdList commands;
    CmdParse<FamilyType>::parseCommandBuffer(commands, ptrOffset(cmdContainer.getCommandStream()->getCpuBase(), 0), cmdContainer.getCommandStream()->getUsed());

    auto itor = commands.begin();

    itor = find<MI_LOAD_REGISTER_MEM *>(itor, commands.end());
    ASSERT_NE(itor, commands.end());

    auto cmdMEM = genCmdCast<MI_LOAD_REGISTER_MEM *>(*itor);
    EXPECT_EQ(cmdMEM->getRegisterAddress(), CS_GPR_R0);
    EXPECT_EQ(cmdMEM->getMemoryAddress(), 0xDEADBEEFCAF0u);

    itor = find<MI_LOAD_REGISTER_IMM *>(itor, commands.end());
    ASSERT_NE(itor, commands.end());

    auto cmdIMM = genCmdCast<MI_LOAD_REGISTER_IMM *>(*itor);
    EXPECT_EQ(cmdIMM->getRegisterOffset(), CS_GPR_R1);
    EXPECT_EQ(cmdIMM->getDataDword(), 17u);

    itor = find<MI_MATH *>(itor, commands.end());
    ASSERT_NE(itor, commands.end());

    auto cmdMATH = genCmdCast<MI_MATH *>(*itor);
    EXPECT_EQ(cmdMATH->DW0.BitField.DwordLength, 3u);

    itor = find<MI_LOAD_REGISTER_REG *>(itor, commands.end());
    ASSERT_NE(itor, commands.end());

    auto cmdREG = genCmdCast<MI_LOAD_REGISTER_REG *>(*itor);
    EXPECT_EQ(cmdREG->getSourceRegisterAddress(), CS_GPR_R2);
    EXPECT_EQ(cmdREG->getDestinationRegisterAddress(), CS_PREDICATE_RESULT);

    auto cmdALU = reinterpret_cast<MI_MATH_ALU_INST_INLINE *>(cmdMATH + 3);
    EXPECT_EQ(cmdALU->DW0.BitField.ALUOpcode,
              static_cast<uint32_t>(AluRegisters::OPCODE_SUB));
}

HWTEST_F(CommandEncoderMathTest, setGroupSizeIndirect) {
    using MI_MATH = typename FamilyType::MI_MATH;
    using MI_MATH_ALU_INST_INLINE = typename FamilyType::MI_MATH_ALU_INST_INLINE;
    using MI_STORE_REGISTER_MEM = typename FamilyType::MI_STORE_REGISTER_MEM;

    CommandContainer cmdContainer;
    cmdContainer.initialize(pDevice);

    uint32_t offsets[3] = {0, sizeof(uint32_t), 2 * sizeof(uint32_t)};
    uint32_t crossThreadAdress[3] = {};
    uint32_t lws[3] = {2, 1, 1};

    EncodeIndirectParams<FamilyType>::setGroupSizeIndirect(cmdContainer, offsets, crossThreadAdress, lws);

    GenCmdList commands;
    CmdParse<FamilyType>::parseCommandBuffer(commands, ptrOffset(cmdContainer.getCommandStream()->getCpuBase(), 0), cmdContainer.getCommandStream()->getUsed());

    auto itor = commands.begin();

    itor = find<MI_MATH *>(itor, commands.end());
    ASSERT_NE(itor, commands.end());

    itor = find<MI_STORE_REGISTER_MEM *>(itor, commands.end());
    ASSERT_NE(itor, commands.end());
}

HWTEST_F(CommandEncoderMathTest, setGroupCountIndirect) {
    using MI_MATH = typename FamilyType::MI_MATH;
    using MI_MATH_ALU_INST_INLINE = typename FamilyType::MI_MATH_ALU_INST_INLINE;
    using MI_STORE_REGISTER_MEM = typename FamilyType::MI_STORE_REGISTER_MEM;

    CommandContainer cmdContainer;
    cmdContainer.initialize(pDevice);

    uint32_t offsets[3] = {0, sizeof(uint32_t), 2 * sizeof(uint32_t)};
    uint32_t crossThreadAdress[3] = {};

    EncodeIndirectParams<FamilyType>::setGroupCountIndirect(cmdContainer, offsets, crossThreadAdress);

    GenCmdList commands;
    CmdParse<FamilyType>::parseCommandBuffer(commands, ptrOffset(cmdContainer.getCommandStream()->getCpuBase(), 0), cmdContainer.getCommandStream()->getUsed());

    auto itor = commands.begin();

    itor = find<MI_STORE_REGISTER_MEM *>(itor, commands.end());
    ASSERT_NE(itor, commands.end());

    itor = find<MI_STORE_REGISTER_MEM *>(++itor, commands.end());
    ASSERT_NE(itor, commands.end());

    itor = find<MI_STORE_REGISTER_MEM *>(++itor, commands.end());
    ASSERT_NE(itor, commands.end());

    itor = find<MI_STORE_REGISTER_MEM *>(++itor, commands.end());
    ASSERT_EQ(itor, commands.end());
}
