/*
 * Copyright (C) 2020-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/command_encoder.h"
#include "shared/source/helpers/register_offsets.h"
#include "shared/source/indirect_heap/heap_size.h"
#include "shared/test/common/cmd_parse/gen_cmd_parse.h"
#include "shared/test/common/fixtures/device_fixture.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/test_macros/hw_test.h"

using namespace NEO;

using EncodeMathMMIOTest = testing::Test;

HWTEST_F(EncodeMathMMIOTest, WhenEncodingAluThenCorrectOpcodesOperandsAdded) {
    using MI_MATH_ALU_INST_INLINE = typename FamilyType::MI_MATH_ALU_INST_INLINE;

    MI_MATH_ALU_INST_INLINE aluParam[5];
    AluRegisters regA = AluRegisters::gpr0;
    AluRegisters regB = AluRegisters::gpr1;
    AluRegisters finalResultRegister = AluRegisters::gpr2;

    memset(aluParam, 0, sizeof(MI_MATH_ALU_INST_INLINE) * 5);

    EncodeMathMMIO<FamilyType>::encodeAluAdd(aluParam, regA, regB,
                                             finalResultRegister);

    EXPECT_EQ(aluParam[0].DW0.BitField.ALUOpcode,
              static_cast<uint32_t>(AluRegisters::opcodeLoad));
    EXPECT_EQ(aluParam[0].DW0.BitField.Operand1,
              static_cast<uint32_t>(AluRegisters::srca));
    EXPECT_EQ(aluParam[0].DW0.BitField.Operand2, static_cast<uint32_t>(regA));

    EXPECT_EQ(aluParam[1].DW0.BitField.ALUOpcode,
              static_cast<uint32_t>(AluRegisters::opcodeLoad));
    EXPECT_EQ(aluParam[1].DW0.BitField.Operand1,
              static_cast<uint32_t>(AluRegisters::srcb));
    EXPECT_EQ(aluParam[1].DW0.BitField.Operand2,
              static_cast<uint32_t>(regB));

    EXPECT_EQ(aluParam[2].DW0.BitField.ALUOpcode,
              static_cast<uint32_t>(AluRegisters::opcodeAdd));
    EXPECT_EQ(aluParam[2].DW0.BitField.Operand1, 0u);
    EXPECT_EQ(aluParam[2].DW0.BitField.Operand2, 0u);

    EXPECT_EQ(aluParam[3].DW0.BitField.ALUOpcode,
              static_cast<uint32_t>(AluRegisters::opcodeStore));
    EXPECT_EQ(aluParam[3].DW0.BitField.Operand1,
              static_cast<uint32_t>(AluRegisters::gpr2));
    EXPECT_EQ(aluParam[3].DW0.BitField.Operand2,
              static_cast<uint32_t>(AluRegisters::accu));

    EXPECT_EQ(aluParam[4].DW0.Value, 0u);
}

HWTEST_F(EncodeMathMMIOTest, WhenEncodingAluSubStoreCarryThenCorrectOpcodesOperandsAdded) {
    using MI_MATH_ALU_INST_INLINE = typename FamilyType::MI_MATH_ALU_INST_INLINE;

    MI_MATH_ALU_INST_INLINE aluParam[5];
    AluRegisters regA = AluRegisters::gpr0;
    AluRegisters regB = AluRegisters::gpr1;
    AluRegisters finalResultRegister = AluRegisters::gpr2;

    memset(aluParam, 0, sizeof(MI_MATH_ALU_INST_INLINE) * 5);

    EncodeMathMMIO<FamilyType>::encodeAluSubStoreCarry(aluParam, regA, regB,
                                                       finalResultRegister);

    EXPECT_EQ(aluParam[0].DW0.BitField.ALUOpcode,
              static_cast<uint32_t>(AluRegisters::opcodeLoad));
    EXPECT_EQ(aluParam[0].DW0.BitField.Operand1,
              static_cast<uint32_t>(AluRegisters::srca));
    EXPECT_EQ(aluParam[0].DW0.BitField.Operand2,
              static_cast<uint32_t>(regA));

    EXPECT_EQ(aluParam[1].DW0.BitField.ALUOpcode,
              static_cast<uint32_t>(AluRegisters::opcodeLoad));
    EXPECT_EQ(aluParam[1].DW0.BitField.Operand1,
              static_cast<uint32_t>(AluRegisters::srcb));
    EXPECT_EQ(aluParam[1].DW0.BitField.Operand2,
              static_cast<uint32_t>(regB));

    EXPECT_EQ(aluParam[2].DW0.BitField.ALUOpcode,
              static_cast<uint32_t>(AluRegisters::opcodeSub));
    EXPECT_EQ(aluParam[2].DW0.BitField.Operand1, 0u);
    EXPECT_EQ(aluParam[2].DW0.BitField.Operand2, 0u);

    EXPECT_EQ(aluParam[3].DW0.BitField.ALUOpcode,
              static_cast<uint32_t>(AluRegisters::opcodeStore));
    EXPECT_EQ(aluParam[3].DW0.BitField.Operand1,
              static_cast<uint32_t>(AluRegisters::gpr2));
    EXPECT_EQ(aluParam[3].DW0.BitField.Operand2,
              static_cast<uint32_t>(AluRegisters::cf));

    EXPECT_EQ(aluParam[4].DW0.Value, 0u);
}

HWTEST_F(EncodeMathMMIOTest, givenAluRegistersWhenEncodeAluAndIsCalledThenAluParamHasCorrectOpcodesAndOperands) {
    using MI_MATH_ALU_INST_INLINE = typename FamilyType::MI_MATH_ALU_INST_INLINE;

    MI_MATH_ALU_INST_INLINE aluParam[5];
    AluRegisters regA = AluRegisters::gpr0;
    AluRegisters regB = AluRegisters::gpr1;
    AluRegisters finalResultRegister = AluRegisters::gpr2;

    memset(aluParam, 0, sizeof(MI_MATH_ALU_INST_INLINE) * 5);

    EncodeMathMMIO<FamilyType>::encodeAluAnd(aluParam, regA, regB,
                                             finalResultRegister);

    EXPECT_EQ(aluParam[0].DW0.BitField.ALUOpcode,
              static_cast<uint32_t>(AluRegisters::opcodeLoad));
    EXPECT_EQ(aluParam[0].DW0.BitField.Operand1,
              static_cast<uint32_t>(AluRegisters::srca));
    EXPECT_EQ(aluParam[0].DW0.BitField.Operand2, static_cast<uint32_t>(regA));

    EXPECT_EQ(aluParam[1].DW0.BitField.ALUOpcode,
              static_cast<uint32_t>(AluRegisters::opcodeLoad));
    EXPECT_EQ(aluParam[1].DW0.BitField.Operand1,
              static_cast<uint32_t>(AluRegisters::srcb));
    EXPECT_EQ(aluParam[1].DW0.BitField.Operand2,
              static_cast<uint32_t>(regB));

    EXPECT_EQ(aluParam[2].DW0.BitField.ALUOpcode,
              static_cast<uint32_t>(AluRegisters::opcodeAnd));
    EXPECT_EQ(aluParam[2].DW0.BitField.Operand1, 0u);
    EXPECT_EQ(aluParam[2].DW0.BitField.Operand2, 0u);

    EXPECT_EQ(aluParam[3].DW0.BitField.ALUOpcode,
              static_cast<uint32_t>(AluRegisters::opcodeStore));
    EXPECT_EQ(aluParam[3].DW0.BitField.Operand1,
              static_cast<uint32_t>(AluRegisters::gpr2));
    EXPECT_EQ(aluParam[3].DW0.BitField.Operand2,
              static_cast<uint32_t>(AluRegisters::accu));

    EXPECT_EQ(aluParam[4].DW0.Value, 0u);
}

using CommandEncoderMathTest = Test<DeviceFixture>;

HWTEST_F(CommandEncoderMathTest, WhenReservingCommandThenBitfieldSetCorrectly) {
    using MI_MATH = typename FamilyType::MI_MATH;
    GenCmdList commands;
    CommandContainer cmdContainer;

    cmdContainer.initialize(pDevice, nullptr, HeapSize::defaultHeapSize, true, false);

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
              static_cast<uint32_t>(RegisterConstants::numAluInstForReadModifyWrite - 1));
}

HWTEST_F(CommandEncoderMathTest, givenOffsetAndValueWhenEncodeBitwiseAndValIsCalledThenContainerHasCorrectMathCommands) {
    using MI_LOAD_REGISTER_REG = typename FamilyType::MI_LOAD_REGISTER_REG;
    using MI_LOAD_REGISTER_IMM = typename FamilyType::MI_LOAD_REGISTER_IMM;
    using MI_MATH = typename FamilyType::MI_MATH;
    using MI_STORE_REGISTER_MEM = typename FamilyType::MI_STORE_REGISTER_MEM;

    GenCmdList commands;
    CommandContainer cmdContainer;
    cmdContainer.initialize(pDevice, nullptr, HeapSize::defaultHeapSize, true, false);
    constexpr uint32_t regOffset = 0x2000u;
    constexpr uint32_t immVal = 0xbaau;
    constexpr uint64_t dstAddress = 0xDEADCAF0u;
    void *storeRegMem = nullptr;
    EncodeMathMMIO<FamilyType>::encodeBitwiseAndVal(cmdContainer, regOffset, immVal, dstAddress, false, &storeRegMem, false);

    CmdParse<FamilyType>::parseCommandBuffer(commands,
                                             ptrOffset(cmdContainer.getCommandStream()->getCpuBase(), 0),
                                             cmdContainer.getCommandStream()->getUsed());

    auto itor = find<MI_LOAD_REGISTER_REG *>(commands.begin(), commands.end());

    // load regOffset to R13
    EXPECT_NE(commands.end(), itor);
    auto cmdLoadReg = genCmdCast<MI_LOAD_REGISTER_REG *>(*itor);
    EXPECT_EQ(cmdLoadReg->getSourceRegisterAddress(), regOffset);
    EXPECT_EQ(cmdLoadReg->getDestinationRegisterAddress(), RegisterOffsets::csGprR13);

    // load immVal to R14
    itor++;
    EXPECT_NE(commands.end(), itor);
    auto cmdLoadImm = genCmdCast<MI_LOAD_REGISTER_IMM *>(*itor);
    EXPECT_EQ(cmdLoadImm->getRegisterOffset(), RegisterOffsets::csGprR14);
    EXPECT_EQ(cmdLoadImm->getDataDword(), immVal);

    // encodeAluAnd should have its own unit tests, so we only check
    // that the MI_MATH exists and length is set to 3u
    itor++;
    EXPECT_NE(commands.end(), itor);
    auto cmdMath = genCmdCast<MI_MATH *>(*itor);
    EXPECT_EQ(cmdMath->DW0.BitField.DwordLength, 3u);

    // store R15 to address
    itor++;
    EXPECT_NE(commands.end(), itor);
    auto cmdMem = genCmdCast<MI_STORE_REGISTER_MEM *>(*itor);
    EXPECT_EQ(cmdMem, storeRegMem);
    EXPECT_EQ(cmdMem->getRegisterAddress(), RegisterOffsets::csGprR12);
    EXPECT_EQ(cmdMem->getMemoryAddress(), dstAddress);
}

HWTEST_F(CommandEncoderMathTest, WhenSettingGroupSizeIndirectThenCommandsAreCorrect) {
    using MI_MATH = typename FamilyType::MI_MATH;
    using MI_STORE_REGISTER_MEM = typename FamilyType::MI_STORE_REGISTER_MEM;

    CommandContainer cmdContainer;
    cmdContainer.initialize(pDevice, nullptr, HeapSize::defaultHeapSize, true, false);

    CrossThreadDataOffset offsets[3] = {0, sizeof(uint32_t), 2 * sizeof(uint32_t)};
    uint32_t crossThreadAddress[3] = {};
    uint32_t lws[3] = {2, 1, 1};

    EncodeIndirectParams<FamilyType>::setGlobalWorkSizeIndirect(cmdContainer, offsets, reinterpret_cast<uint64_t>(crossThreadAddress), lws);

    GenCmdList commands;
    CmdParse<FamilyType>::parseCommandBuffer(commands, ptrOffset(cmdContainer.getCommandStream()->getCpuBase(), 0), cmdContainer.getCommandStream()->getUsed());

    auto itor = commands.begin();

    itor = find<MI_MATH *>(itor, commands.end());
    ASSERT_NE(itor, commands.end());

    itor = find<MI_STORE_REGISTER_MEM *>(itor, commands.end());
    ASSERT_NE(itor, commands.end());
}

HWTEST_F(CommandEncoderMathTest, WhenSettingGroupCountIndirectThenCommandsAreCorrect) {
    using MI_STORE_REGISTER_MEM = typename FamilyType::MI_STORE_REGISTER_MEM;

    CommandContainer cmdContainer;
    cmdContainer.initialize(pDevice, nullptr, HeapSize::defaultHeapSize, true, false);

    CrossThreadDataOffset offsets[3] = {0, sizeof(uint32_t), 2 * sizeof(uint32_t)};
    uint32_t crossThreadAddress[3] = {};

    EncodeIndirectParams<FamilyType>::setGroupCountIndirect(cmdContainer, offsets, reinterpret_cast<uint64_t>(crossThreadAddress));

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

using CommandEncodeAluTests = ::testing::Test;

HWTEST_F(CommandEncodeAluTests, whenAskingForIncrementOrDecrementCmdsSizeThenReturnCorrectValue) {
    constexpr size_t expectedSize = (2 * sizeof(typename FamilyType::MI_LOAD_REGISTER_IMM)) + sizeof(typename FamilyType::MI_MATH) + (4 * sizeof(typename FamilyType::MI_MATH_ALU_INST_INLINE));

    EXPECT_EQ(EncodeMathMMIO<FamilyType>::getCmdSizeForIncrementOrDecrement(), expectedSize);
}

HWTEST_F(CommandEncodeAluTests, whenProgrammingIncrementOperationThenUseCorrectAluCommands) {
    using MI_LOAD_REGISTER_IMM = typename FamilyType::MI_LOAD_REGISTER_IMM;
    using MI_MATH_ALU_INST_INLINE = typename FamilyType::MI_MATH_ALU_INST_INLINE;
    using MI_MATH = typename FamilyType::MI_MATH;

    constexpr size_t bufferSize = EncodeMathMMIO<FamilyType>::getCmdSizeForIncrementOrDecrement();
    constexpr AluRegisters incRegister = AluRegisters::gpr1;

    uint8_t buffer[bufferSize] = {};
    LinearStream cmdStream(buffer, bufferSize);

    EncodeMathMMIO<FamilyType>::encodeIncrement(cmdStream, incRegister, false);

    EXPECT_EQ(bufferSize, cmdStream.getUsed());

    auto lriCmd = reinterpret_cast<MI_LOAD_REGISTER_IMM *>(buffer);
    EXPECT_EQ(lriCmd->getRegisterOffset(), RegisterOffsets::csGprR7);
    EXPECT_EQ(lriCmd->getDataDword(), 1u);

    lriCmd++;
    EXPECT_EQ(RegisterOffsets::csGprR7 + 4, lriCmd->getRegisterOffset());
    EXPECT_EQ(0u, lriCmd->getDataDword());

    auto miMathCmd = reinterpret_cast<MI_MATH *>(++lriCmd);
    EXPECT_EQ(3u, miMathCmd->DW0.BitField.DwordLength);

    auto miAluCmd = reinterpret_cast<MI_MATH_ALU_INST_INLINE *>(++miMathCmd);
    EXPECT_EQ(static_cast<uint32_t>(AluRegisters::opcodeLoad), miAluCmd->DW0.BitField.ALUOpcode);
    EXPECT_EQ(static_cast<uint32_t>(AluRegisters::srca), miAluCmd->DW0.BitField.Operand1);
    EXPECT_EQ(static_cast<uint32_t>(incRegister), miAluCmd->DW0.BitField.Operand2);

    miAluCmd++;
    EXPECT_EQ(static_cast<uint32_t>(AluRegisters::opcodeLoad), miAluCmd->DW0.BitField.ALUOpcode);
    EXPECT_EQ(static_cast<uint32_t>(AluRegisters::srcb), miAluCmd->DW0.BitField.Operand1);
    EXPECT_EQ(static_cast<uint32_t>(AluRegisters::gpr7), miAluCmd->DW0.BitField.Operand2);

    miAluCmd++;
    EXPECT_EQ(static_cast<uint32_t>(AluRegisters::opcodeAdd), miAluCmd->DW0.BitField.ALUOpcode);
    EXPECT_EQ(0u, miAluCmd->DW0.BitField.Operand1);
    EXPECT_EQ(0u, miAluCmd->DW0.BitField.Operand2);

    miAluCmd++;
    EXPECT_EQ(static_cast<uint32_t>(AluRegisters::opcodeStore), miAluCmd->DW0.BitField.ALUOpcode);
    EXPECT_EQ(static_cast<uint32_t>(incRegister), miAluCmd->DW0.BitField.Operand1);
    EXPECT_EQ(static_cast<uint32_t>(AluRegisters::accu), miAluCmd->DW0.BitField.Operand2);
}

HWTEST_F(CommandEncodeAluTests, whenProgrammingDecrementOperationThenUseCorrectAluCommands) {
    using MI_LOAD_REGISTER_IMM = typename FamilyType::MI_LOAD_REGISTER_IMM;
    using MI_MATH_ALU_INST_INLINE = typename FamilyType::MI_MATH_ALU_INST_INLINE;
    using MI_MATH = typename FamilyType::MI_MATH;

    constexpr size_t bufferSize = EncodeMathMMIO<FamilyType>::getCmdSizeForIncrementOrDecrement();
    constexpr AluRegisters decRegister = AluRegisters::gpr1;

    uint8_t buffer[bufferSize] = {};
    LinearStream cmdStream(buffer, bufferSize);

    EncodeMathMMIO<FamilyType>::encodeDecrement(cmdStream, decRegister, false);

    EXPECT_EQ(bufferSize, cmdStream.getUsed());

    auto lriCmd = reinterpret_cast<MI_LOAD_REGISTER_IMM *>(buffer);
    EXPECT_EQ(lriCmd->getRegisterOffset(), RegisterOffsets::csGprR7);
    EXPECT_EQ(lriCmd->getDataDword(), 1u);

    lriCmd++;
    EXPECT_EQ(RegisterOffsets::csGprR7 + 4, lriCmd->getRegisterOffset());
    EXPECT_EQ(0u, lriCmd->getDataDword());

    auto miMathCmd = reinterpret_cast<MI_MATH *>(++lriCmd);
    EXPECT_EQ(3u, miMathCmd->DW0.BitField.DwordLength);

    auto miAluCmd = reinterpret_cast<MI_MATH_ALU_INST_INLINE *>(++miMathCmd);
    EXPECT_EQ(static_cast<uint32_t>(AluRegisters::opcodeLoad), miAluCmd->DW0.BitField.ALUOpcode);
    EXPECT_EQ(static_cast<uint32_t>(AluRegisters::srca), miAluCmd->DW0.BitField.Operand1);
    EXPECT_EQ(static_cast<uint32_t>(decRegister), miAluCmd->DW0.BitField.Operand2);

    miAluCmd++;
    EXPECT_EQ(static_cast<uint32_t>(AluRegisters::opcodeLoad), miAluCmd->DW0.BitField.ALUOpcode);
    EXPECT_EQ(static_cast<uint32_t>(AluRegisters::srcb), miAluCmd->DW0.BitField.Operand1);
    EXPECT_EQ(static_cast<uint32_t>(AluRegisters::gpr7), miAluCmd->DW0.BitField.Operand2);

    miAluCmd++;
    EXPECT_EQ(static_cast<uint32_t>(AluRegisters::opcodeSub), miAluCmd->DW0.BitField.ALUOpcode);
    EXPECT_EQ(0u, miAluCmd->DW0.BitField.Operand1);
    EXPECT_EQ(0u, miAluCmd->DW0.BitField.Operand2);

    miAluCmd++;
    EXPECT_EQ(static_cast<uint32_t>(AluRegisters::opcodeStore), miAluCmd->DW0.BitField.ALUOpcode);
    EXPECT_EQ(static_cast<uint32_t>(decRegister), miAluCmd->DW0.BitField.Operand1);
    EXPECT_EQ(static_cast<uint32_t>(AluRegisters::accu), miAluCmd->DW0.BitField.Operand2);
}
