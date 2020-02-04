/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/command_container/command_encoder.h"
#include "core/helpers/register_offsets.h"
#include "test.h"
#include "unit_tests/fixtures/device_fixture.h"
#include "unit_tests/gen_common/gen_cmd_parse.h"

using namespace NEO;

using EncodeMathMMIOTest = testing::Test;

HWTEST_F(EncodeMathMMIOTest, encodeAluAddHasCorrectOpcodesOperands) {
    using MI_MATH_ALU_INST_INLINE = typename FamilyType::MI_MATH_ALU_INST_INLINE;

    MI_MATH_ALU_INST_INLINE aluParam[5];
    uint32_t regA = ALU_REGISTER_R_0;
    uint32_t regB = ALU_REGISTER_R_1;

    memset(aluParam, 0, sizeof(MI_MATH_ALU_INST_INLINE) * 5);

    EncodeMathMMIO<FamilyType>::encodeAluAdd(aluParam, regA, regB);

    EXPECT_EQ(aluParam[0].DW0.BitField.ALUOpcode, ALU_OPCODE_LOAD);
    EXPECT_EQ(aluParam[0].DW0.BitField.Operand1, ALU_REGISTER_R_SRCA);
    EXPECT_EQ(aluParam[0].DW0.BitField.Operand2, regA);

    EXPECT_EQ(aluParam[1].DW0.BitField.ALUOpcode, ALU_OPCODE_LOAD);
    EXPECT_EQ(aluParam[1].DW0.BitField.Operand1, ALU_REGISTER_R_SRCB);
    EXPECT_EQ(aluParam[1].DW0.BitField.Operand2, regB);

    EXPECT_EQ(aluParam[2].DW0.BitField.ALUOpcode, ALU_OPCODE_ADD);
    EXPECT_EQ(aluParam[2].DW0.BitField.Operand1, 0u);
    EXPECT_EQ(aluParam[2].DW0.BitField.Operand2, 0u);

    EXPECT_EQ(aluParam[3].DW0.BitField.ALUOpcode, ALU_OPCODE_STORE);
    EXPECT_EQ(aluParam[3].DW0.BitField.Operand1, ALU_REGISTER_R_0);
    EXPECT_EQ(aluParam[3].DW0.BitField.Operand2, ALU_REGISTER_R_ACCU);

    EXPECT_EQ(aluParam[4].DW0.Value, 0u);
}

HWTEST_F(EncodeMathMMIOTest, encodeAluSubStoreCarryHasCorrectOpcodesOperands) {
    using MI_MATH_ALU_INST_INLINE = typename FamilyType::MI_MATH_ALU_INST_INLINE;

    MI_MATH_ALU_INST_INLINE aluParam[5];
    uint32_t regA = ALU_REGISTER_R_0;
    uint32_t regB = ALU_REGISTER_R_1;

    memset(aluParam, 0, sizeof(MI_MATH_ALU_INST_INLINE) * 5);

    EncodeMathMMIO<FamilyType>::encodeAluSubStoreCarry(aluParam, regA, regB);

    EXPECT_EQ(aluParam[0].DW0.BitField.ALUOpcode, ALU_OPCODE_LOAD);
    EXPECT_EQ(aluParam[0].DW0.BitField.Operand1, ALU_REGISTER_R_SRCA);
    EXPECT_EQ(aluParam[0].DW0.BitField.Operand2, regA);

    EXPECT_EQ(aluParam[1].DW0.BitField.ALUOpcode, ALU_OPCODE_LOAD);
    EXPECT_EQ(aluParam[1].DW0.BitField.Operand1, ALU_REGISTER_R_SRCB);
    EXPECT_EQ(aluParam[1].DW0.BitField.Operand2, regB);

    EXPECT_EQ(aluParam[2].DW0.BitField.ALUOpcode, ALU_OPCODE_SUB);
    EXPECT_EQ(aluParam[2].DW0.BitField.Operand1, 0u);
    EXPECT_EQ(aluParam[2].DW0.BitField.Operand2, 0u);

    EXPECT_EQ(aluParam[3].DW0.BitField.ALUOpcode, ALU_OPCODE_STORE);
    EXPECT_EQ(aluParam[3].DW0.BitField.Operand1, ALU_REGISTER_R_0);
    EXPECT_EQ(aluParam[3].DW0.BitField.Operand2, ALU_REGISTER_R_CF);

    EXPECT_EQ(aluParam[4].DW0.Value, 0u);
}

using CommandEncoderMathTest = Test<DeviceFixture>;

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
    EXPECT_EQ(cmdREG->getSourceRegisterAddress(), CS_GPR_R0);
    EXPECT_EQ(cmdREG->getDestinationRegisterAddress(), CS_PREDICATE_RESULT);

    auto cmdALU = reinterpret_cast<MI_MATH_ALU_INST_INLINE *>(cmdMATH + 3);
    EXPECT_EQ(cmdALU->DW0.BitField.ALUOpcode, ALU_OPCODE_SUB);
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
