/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/command_encoder.h"
#include "shared/source/helpers/register_offsets.h"
#include "shared/test/common/cmd_parse/gen_cmd_parse.h"
#include "shared/test/common/fixtures/device_fixture.h"
#include "shared/test/common/test_macros/hw_test.h"

using namespace NEO;

using EncodeMathMMIOTest = testing::Test;

HWTEST_F(EncodeMathMMIOTest, WhenEncodingAluThenCorrectOpcodesOperandsAdded) {
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

HWTEST_F(EncodeMathMMIOTest, WhenEncodingAluSubStoreCarryThenCorrectOpcodesOperandsAdded) {
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

HWTEST_F(EncodeMathMMIOTest, givenAluRegistersWhenEncodeAluAndIsCalledThenAluParamHasCorrectOpcodesAndOperands) {
    using MI_MATH_ALU_INST_INLINE = typename FamilyType::MI_MATH_ALU_INST_INLINE;

    MI_MATH_ALU_INST_INLINE aluParam[5];
    AluRegisters regA = AluRegisters::R_0;
    AluRegisters regB = AluRegisters::R_1;
    AluRegisters finalResultRegister = AluRegisters::R_2;

    memset(aluParam, 0, sizeof(MI_MATH_ALU_INST_INLINE) * 5);

    EncodeMathMMIO<FamilyType>::encodeAluAnd(aluParam, regA, regB,
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
              static_cast<uint32_t>(AluRegisters::OPCODE_AND));
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

using CommandEncoderMathTest = Test<DeviceFixture>;

HWTEST_F(CommandEncoderMathTest, WhenReservingCommandThenBitfieldSetCorrectly) {
    using MI_MATH = typename FamilyType::MI_MATH;
    GenCmdList commands;
    CommandContainer cmdContainer;

    cmdContainer.initialize(pDevice, nullptr, true);

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

HWTEST_F(CommandEncoderMathTest, givenOffsetAndValueWhenEncodeBitwiseAndValIsCalledThenContainerHasCorrectMathCommands) {
    using MI_LOAD_REGISTER_REG = typename FamilyType::MI_LOAD_REGISTER_REG;
    using MI_LOAD_REGISTER_IMM = typename FamilyType::MI_LOAD_REGISTER_IMM;
    using MI_MATH = typename FamilyType::MI_MATH;
    using MI_STORE_REGISTER_MEM = typename FamilyType::MI_STORE_REGISTER_MEM;

    GenCmdList commands;
    CommandContainer cmdContainer;
    cmdContainer.initialize(pDevice, nullptr, true);
    constexpr uint32_t regOffset = 0x2000u;
    constexpr uint32_t immVal = 0xbaau;
    constexpr uint64_t dstAddress = 0xDEADCAF0u;
    EncodeMathMMIO<FamilyType>::encodeBitwiseAndVal(cmdContainer, regOffset, immVal, dstAddress, false);

    CmdParse<FamilyType>::parseCommandBuffer(commands,
                                             ptrOffset(cmdContainer.getCommandStream()->getCpuBase(), 0),
                                             cmdContainer.getCommandStream()->getUsed());

    auto itor = find<MI_LOAD_REGISTER_REG *>(commands.begin(), commands.end());

    // load regOffset to R0
    EXPECT_NE(commands.end(), itor);
    auto cmdLoadReg = genCmdCast<MI_LOAD_REGISTER_REG *>(*itor);
    EXPECT_EQ(cmdLoadReg->getSourceRegisterAddress(), regOffset);
    EXPECT_EQ(cmdLoadReg->getDestinationRegisterAddress(), CS_GPR_R0);

    // load immVal to R1
    itor++;
    EXPECT_NE(commands.end(), itor);
    auto cmdLoadImm = genCmdCast<MI_LOAD_REGISTER_IMM *>(*itor);
    EXPECT_EQ(cmdLoadImm->getRegisterOffset(), CS_GPR_R1);
    EXPECT_EQ(cmdLoadImm->getDataDword(), immVal);

    // encodeAluAnd should have its own unit tests, so we only check
    // that the MI_MATH exists and length is set to 3u
    itor++;
    EXPECT_NE(commands.end(), itor);
    auto cmdMath = genCmdCast<MI_MATH *>(*itor);
    EXPECT_EQ(cmdMath->DW0.BitField.DwordLength, 3u);

    // store R2 to address
    itor++;
    EXPECT_NE(commands.end(), itor);
    auto cmdMem = genCmdCast<MI_STORE_REGISTER_MEM *>(*itor);
    EXPECT_EQ(cmdMem->getRegisterAddress(), CS_GPR_R2);
    EXPECT_EQ(cmdMem->getMemoryAddress(), dstAddress);
}

HWTEST_F(CommandEncoderMathTest, WhenSettingGroupSizeIndirectThenCommandsAreCorrect) {
    using MI_MATH = typename FamilyType::MI_MATH;
    using MI_MATH_ALU_INST_INLINE = typename FamilyType::MI_MATH_ALU_INST_INLINE;
    using MI_STORE_REGISTER_MEM = typename FamilyType::MI_STORE_REGISTER_MEM;

    CommandContainer cmdContainer;
    cmdContainer.initialize(pDevice, nullptr, true);

    CrossThreadDataOffset offsets[3] = {0, sizeof(uint32_t), 2 * sizeof(uint32_t)};
    uint32_t crossThreadAdress[3] = {};
    uint32_t lws[3] = {2, 1, 1};

    EncodeIndirectParams<FamilyType>::setGlobalWorkSizeIndirect(cmdContainer, offsets, reinterpret_cast<uint64_t>(crossThreadAdress), lws);

    GenCmdList commands;
    CmdParse<FamilyType>::parseCommandBuffer(commands, ptrOffset(cmdContainer.getCommandStream()->getCpuBase(), 0), cmdContainer.getCommandStream()->getUsed());

    auto itor = commands.begin();

    itor = find<MI_MATH *>(itor, commands.end());
    ASSERT_NE(itor, commands.end());

    itor = find<MI_STORE_REGISTER_MEM *>(itor, commands.end());
    ASSERT_NE(itor, commands.end());
}

HWTEST_F(CommandEncoderMathTest, WhenSettingGroupCountIndirectThenCommandsAreCorrect) {
    using MI_MATH = typename FamilyType::MI_MATH;
    using MI_MATH_ALU_INST_INLINE = typename FamilyType::MI_MATH_ALU_INST_INLINE;
    using MI_STORE_REGISTER_MEM = typename FamilyType::MI_STORE_REGISTER_MEM;

    CommandContainer cmdContainer;
    cmdContainer.initialize(pDevice, nullptr, true);

    CrossThreadDataOffset offsets[3] = {0, sizeof(uint32_t), 2 * sizeof(uint32_t)};
    uint32_t crossThreadAdress[3] = {};

    EncodeIndirectParams<FamilyType>::setGroupCountIndirect(cmdContainer, offsets, reinterpret_cast<uint64_t>(crossThreadAdress));

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
