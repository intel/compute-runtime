/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/command_encoder.h"
#include "shared/source/helpers/register_offsets.h"
#include "shared/source/indirect_heap/heap_size.h"
#include "shared/test/common/cmd_parse/gen_cmd_parse.h"
#include "shared/test/common/fixtures/device_fixture.h"
#include "shared/test/common/helpers/gtest_helpers.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/test_macros/hw_test.h"
#include "shared/test/unit_test/mocks/mock_dispatch_kernel_encoder_interface.h"

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

    EncodeIndirectParams<FamilyType>::setGlobalWorkSizeIndirect(cmdContainer, offsets, reinterpret_cast<uint64_t>(crossThreadAddress), lws, nullptr);

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

    EncodeIndirectParams<FamilyType>::setGroupCountIndirect(cmdContainer, offsets, reinterpret_cast<uint64_t>(crossThreadAddress), nullptr);

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

HWTEST_F(CommandEncoderMathTest, givenPayloadArgumentStoredInInlineDataWhenSettingGroupCountIndirectThenInlineDataRelatedCommandIsStoredInCommandsToPatch) {
    using MI_STORE_REGISTER_MEM = typename FamilyType::MI_STORE_REGISTER_MEM;

    CommandContainer cmdContainer;
    cmdContainer.initialize(pDevice, nullptr, HeapSize::defaultHeapSize, true, false);

    CrossThreadDataOffset offsets[3] = {0, sizeof(uint32_t), 2 * sizeof(uint32_t)};
    uint64_t crossThreadGpuVa = 0xBADF000;

    IndirectParamsInInlineDataArgs args{};
    args.storeGroupCountInInlineData[1] = true;

    EncodeIndirectParams<FamilyType>::setGroupCountIndirect(cmdContainer, offsets, crossThreadGpuVa, &args);

    EXPECT_EQ(1u, args.commandsToPatch.size());

    GenCmdList commands;
    CmdParse<FamilyType>::parseCommandBuffer(commands, ptrOffset(cmdContainer.getCommandStream()->getCpuBase(), 0), cmdContainer.getCommandStream()->getUsed());

    auto itor = commands.begin();

    itor = find<MI_STORE_REGISTER_MEM *>(itor, commands.end());
    ASSERT_NE(itor, commands.end());
    auto storeRegMem = reinterpret_cast<MI_STORE_REGISTER_MEM *>(*itor);
    EXPECT_EQ(crossThreadGpuVa + offsets[0], storeRegMem->getMemoryAddress());

    itor = find<MI_STORE_REGISTER_MEM *>(++itor, commands.end());
    ASSERT_NE(itor, commands.end());
    EXPECT_EQ(*itor, args.commandsToPatch[0].command);
    storeRegMem = reinterpret_cast<MI_STORE_REGISTER_MEM *>(*itor);
    EXPECT_EQ(crossThreadGpuVa + offsets[1], storeRegMem->getMemoryAddress());
    EXPECT_EQ(args.commandsToPatch[0].address, offsets[1]);
    ;
    EXPECT_EQ(args.commandsToPatch[0].offset, storeRegMem->getRegisterAddress());

    itor = find<MI_STORE_REGISTER_MEM *>(++itor, commands.end());
    ASSERT_NE(itor, commands.end());
    storeRegMem = reinterpret_cast<MI_STORE_REGISTER_MEM *>(*itor);
    EXPECT_EQ(crossThreadGpuVa + offsets[2], storeRegMem->getMemoryAddress());

    itor = find<MI_STORE_REGISTER_MEM *>(++itor, commands.end());
    ASSERT_EQ(itor, commands.end());
}

HWTEST_F(CommandEncoderMathTest, givenPayloadArgumentStoredInInlineDataWhenSettingGlobalGroupSizeIndirectThenInlineDataRelatedCommandIsStoredInCommandsToPatch) {
    using MI_STORE_REGISTER_MEM = typename FamilyType::MI_STORE_REGISTER_MEM;

    CommandContainer cmdContainer;
    cmdContainer.initialize(pDevice, nullptr, HeapSize::defaultHeapSize, true, false);

    CrossThreadDataOffset offsets[3] = {0, sizeof(uint32_t), 2 * sizeof(uint32_t)};
    uint64_t crossThreadGpuVa = 0xBADF000;

    IndirectParamsInInlineDataArgs args{};
    args.storeGlobalWorkSizeInInlineData[1] = true;

    uint32_t lws[3] = {1, 2, 3};

    EncodeIndirectParams<FamilyType>::setGlobalWorkSizeIndirect(cmdContainer, offsets, crossThreadGpuVa, lws, &args);

    EXPECT_EQ(1u, args.commandsToPatch.size());

    GenCmdList commands;
    CmdParse<FamilyType>::parseCommandBuffer(commands, ptrOffset(cmdContainer.getCommandStream()->getCpuBase(), 0), cmdContainer.getCommandStream()->getUsed());

    auto itor = commands.begin();

    itor = find<MI_STORE_REGISTER_MEM *>(itor, commands.end());
    ASSERT_NE(itor, commands.end());
    auto storeRegMem = reinterpret_cast<MI_STORE_REGISTER_MEM *>(*itor);
    EXPECT_EQ(crossThreadGpuVa + offsets[0], storeRegMem->getMemoryAddress());

    itor = find<MI_STORE_REGISTER_MEM *>(++itor, commands.end());
    ASSERT_NE(itor, commands.end());
    EXPECT_EQ(*itor, args.commandsToPatch[0].command);
    storeRegMem = reinterpret_cast<MI_STORE_REGISTER_MEM *>(*itor);
    EXPECT_EQ(offsets[1], storeRegMem->getMemoryAddress());
    EXPECT_EQ(args.commandsToPatch[0].address, offsets[1]);
    ;
    EXPECT_EQ(args.commandsToPatch[0].offset, storeRegMem->getRegisterAddress());

    itor = find<MI_STORE_REGISTER_MEM *>(++itor, commands.end());
    ASSERT_NE(itor, commands.end());
    storeRegMem = reinterpret_cast<MI_STORE_REGISTER_MEM *>(*itor);
    EXPECT_EQ(crossThreadGpuVa + offsets[2], storeRegMem->getMemoryAddress());

    itor = find<MI_STORE_REGISTER_MEM *>(++itor, commands.end());
    ASSERT_EQ(itor, commands.end());
}

HWTEST_F(CommandEncoderMathTest, givenPayloadArgumentStoredInInlineDataWhenSettingWorkDimIndirectThenInlineDataRelatedCommandIsStoredInCommandsToPatch) {
    using MI_STORE_REGISTER_MEM = typename FamilyType::MI_STORE_REGISTER_MEM;

    CommandContainer cmdContainer;
    cmdContainer.initialize(pDevice, nullptr, HeapSize::defaultHeapSize, true, false);

    CrossThreadDataOffset offset = sizeof(uint32_t);
    uint64_t crossThreadGpuVa = 0xBADF000;

    IndirectParamsInInlineDataArgs args{};
    args.storeWorkDimInInlineData = true;

    uint32_t groupSizes[3] = {1, 2, 3};

    EncodeIndirectParams<FamilyType>::setWorkDimIndirect(cmdContainer, offset, crossThreadGpuVa, groupSizes, &args);

    EXPECT_EQ(1u, args.commandsToPatch.size());

    GenCmdList commands;
    CmdParse<FamilyType>::parseCommandBuffer(commands, ptrOffset(cmdContainer.getCommandStream()->getCpuBase(), 0), cmdContainer.getCommandStream()->getUsed());

    auto itor = commands.begin();

    itor = find<MI_STORE_REGISTER_MEM *>(++itor, commands.end());
    ASSERT_NE(itor, commands.end());
    EXPECT_EQ(*itor, args.commandsToPatch[0].command);
    auto storeRegMem = reinterpret_cast<MI_STORE_REGISTER_MEM *>(*itor);
    EXPECT_EQ(args.commandsToPatch[0].address, offset);
    ;
    EXPECT_EQ(args.commandsToPatch[0].offset, storeRegMem->getRegisterAddress());

    itor = find<MI_STORE_REGISTER_MEM *>(++itor, commands.end());
    ASSERT_EQ(itor, commands.end());
}

HWTEST_F(CommandEncoderMathTest, givenPayloadArgumentStoredInInlineDataWhenEncodeIndirectParamsAndApplyingInlineGpuVaThenCorrectCommandsAreProgrammed) {
    using MI_STORE_REGISTER_MEM = typename FamilyType::MI_STORE_REGISTER_MEM;

    for (auto workDimInInlineData : ::testing::Bool()) {
        CommandContainer cmdContainer;
        cmdContainer.initialize(pDevice, nullptr, HeapSize::defaultHeapSize, true, false);

        uint64_t crossThreadGpuVa = 0xBADF000;

        IndirectParamsInInlineDataArgs args{};

        MockDispatchKernelEncoder dispatchInterface;

        auto &kernelDescriptor = dispatchInterface.kernelDescriptor;
        uint32_t groupSizes[3] = {1, 2, 3};
        dispatchInterface.getGroupSizeResult = groupSizes;

        kernelDescriptor.kernelAttributes.inlineDataPayloadSize = 0x100;
        kernelDescriptor.payloadMappings.dispatchTraits.numWorkGroups[0] = 0x8;
        kernelDescriptor.payloadMappings.dispatchTraits.numWorkGroups[1] = 0x120;
        kernelDescriptor.payloadMappings.dispatchTraits.numWorkGroups[2] = undefined<CrossThreadDataOffset>;

        kernelDescriptor.payloadMappings.dispatchTraits.globalWorkSize[0] = undefined<CrossThreadDataOffset>;
        kernelDescriptor.payloadMappings.dispatchTraits.globalWorkSize[1] = 0x20;
        kernelDescriptor.payloadMappings.dispatchTraits.globalWorkSize[2] = 0x100;

        kernelDescriptor.payloadMappings.dispatchTraits.workDim = workDimInInlineData ? 0x60 : 0x110;

        EncodeIndirectParams<FamilyType>::encode(cmdContainer, crossThreadGpuVa, &dispatchInterface, 0u, &args);

        if (workDimInInlineData) {
            EXPECT_EQ(3u, args.commandsToPatch.size());
        } else {
            EXPECT_EQ(2u, args.commandsToPatch.size());
        }
        EXPECT_TRUE(args.storeGroupCountInInlineData[0]);
        EXPECT_FALSE(args.storeGroupCountInInlineData[1]);
        EXPECT_FALSE(args.storeGroupCountInInlineData[2]);

        EXPECT_FALSE(args.storeGlobalWorkSizeInInlineData[0]);
        EXPECT_TRUE(args.storeGlobalWorkSizeInInlineData[1]);
        EXPECT_FALSE(args.storeGlobalWorkSizeInInlineData[2]);

        EXPECT_EQ(workDimInInlineData, args.storeWorkDimInInlineData);

        uint64_t inlineDataGpuVa = 0x12340000;
        EncodeIndirectParams<FamilyType>::applyInlineDataGpuVA(args, inlineDataGpuVa);

        GenCmdList commands;
        CmdParse<FamilyType>::parseCommandBuffer(commands, ptrOffset(cmdContainer.getCommandStream()->getCpuBase(), 0), cmdContainer.getCommandStream()->getUsed());

        auto itor = commands.begin();
        itor = find<MI_STORE_REGISTER_MEM *>(itor, commands.end());
        ASSERT_NE(itor, commands.end());
        auto storeRegMem = reinterpret_cast<MI_STORE_REGISTER_MEM *>(*itor);
        EXPECT_EQ(storeRegMem, args.commandsToPatch[0].command);
        EXPECT_EQ(inlineDataGpuVa + kernelDescriptor.payloadMappings.dispatchTraits.numWorkGroups[0], storeRegMem->getMemoryAddress());

        itor = find<MI_STORE_REGISTER_MEM *>(++itor, commands.end());
        ASSERT_NE(itor, commands.end());
        storeRegMem = reinterpret_cast<MI_STORE_REGISTER_MEM *>(*itor);
        EXPECT_EQ(crossThreadGpuVa + kernelDescriptor.payloadMappings.dispatchTraits.numWorkGroups[1], storeRegMem->getMemoryAddress());

        itor = find<MI_STORE_REGISTER_MEM *>(++itor, commands.end());
        ASSERT_NE(itor, commands.end());
        storeRegMem = reinterpret_cast<MI_STORE_REGISTER_MEM *>(*itor);
        EXPECT_EQ(storeRegMem, args.commandsToPatch[1].command);
        EXPECT_EQ(inlineDataGpuVa + kernelDescriptor.payloadMappings.dispatchTraits.globalWorkSize[1], storeRegMem->getMemoryAddress());

        itor = find<MI_STORE_REGISTER_MEM *>(++itor, commands.end());
        ASSERT_NE(itor, commands.end());
        storeRegMem = reinterpret_cast<MI_STORE_REGISTER_MEM *>(*itor);
        EXPECT_EQ(crossThreadGpuVa + kernelDescriptor.payloadMappings.dispatchTraits.globalWorkSize[2], storeRegMem->getMemoryAddress());

        itor = find<MI_STORE_REGISTER_MEM *>(++itor, commands.end());
        ASSERT_NE(itor, commands.end());
        storeRegMem = reinterpret_cast<MI_STORE_REGISTER_MEM *>(*itor);
        if (workDimInInlineData) {
            EXPECT_EQ(storeRegMem, args.commandsToPatch[2].command);
            EXPECT_EQ(inlineDataGpuVa + kernelDescriptor.payloadMappings.dispatchTraits.workDim, storeRegMem->getMemoryAddress());
        } else {
            EXPECT_EQ(crossThreadGpuVa + kernelDescriptor.payloadMappings.dispatchTraits.workDim, storeRegMem->getMemoryAddress());
        }

        itor = find<MI_STORE_REGISTER_MEM *>(++itor, commands.end());
        ASSERT_EQ(itor, commands.end());
    }
}

HWTEST_F(CommandEncoderMathTest, givenPayloadArgumentStoredInInlineDataWhenEncodeIndirectParamsThenPreparserMitigationIsProgrammed) {
    using MI_ARB_CHECK = typename FamilyType::MI_ARB_CHECK;
    using MI_BATCH_BUFFER_START = typename FamilyType::MI_BATCH_BUFFER_START;
    CommandContainer cmdContainer0;
    cmdContainer0.initialize(pDevice, nullptr, HeapSize::defaultHeapSize, true, false);

    CommandContainer cmdContainer1;
    cmdContainer1.initialize(pDevice, nullptr, HeapSize::defaultHeapSize, true, false);

    uint64_t crossThreadGpuVa = 0xBADF000;

    IndirectParamsInInlineDataArgs args{};

    MockDispatchKernelEncoder dispatchInterface;

    auto &kernelDescriptor = dispatchInterface.kernelDescriptor;
    uint32_t groupSizes[3] = {1, 2, 3};
    dispatchInterface.getGroupSizeResult = groupSizes;

    kernelDescriptor.kernelAttributes.inlineDataPayloadSize = 0x100;
    kernelDescriptor.payloadMappings.dispatchTraits.numWorkGroups[0] = 0x100;
    kernelDescriptor.payloadMappings.dispatchTraits.numWorkGroups[1] = 0x110;
    kernelDescriptor.payloadMappings.dispatchTraits.numWorkGroups[2] = undefined<CrossThreadDataOffset>;

    kernelDescriptor.payloadMappings.dispatchTraits.globalWorkSize[0] = undefined<CrossThreadDataOffset>;
    kernelDescriptor.payloadMappings.dispatchTraits.globalWorkSize[1] = 0x120;
    kernelDescriptor.payloadMappings.dispatchTraits.globalWorkSize[2] = 0x130;

    kernelDescriptor.payloadMappings.dispatchTraits.workDim = 0x140;

    EncodeIndirectParams<FamilyType>::encode(cmdContainer0, crossThreadGpuVa, &dispatchInterface, 0u, &args);

    kernelDescriptor.payloadMappings.dispatchTraits.workDim = 0x60;

    EncodeIndirectParams<FamilyType>::encode(cmdContainer1, crossThreadGpuVa, &dispatchInterface, 0u, &args);

    auto used0 = cmdContainer0.getCommandStream()->getUsed();
    auto used1 = cmdContainer1.getCommandStream()->getUsed();

    auto expectedDiff = sizeof(MI_ARB_CHECK) * 2 + sizeof(MI_BATCH_BUFFER_START);
    EXPECT_EQ(expectedDiff, used1 - used0);

    GenCmdList commands;
    CmdParse<FamilyType>::parseCommandBuffer(commands, ptrOffset(cmdContainer1.getCommandStream()->getCpuBase(), used0), used1 - used0);
    auto itor = commands.begin();
    itor = find<MI_ARB_CHECK *>(itor, commands.end());
    ASSERT_NE(itor, commands.end());
    itor = find<MI_BATCH_BUFFER_START *>(++itor, commands.end());

    ASSERT_NE(itor, commands.end());
    itor = find<MI_ARB_CHECK *>(++itor, commands.end());

    ASSERT_NE(itor, commands.end());
    itor = find<MI_ARB_CHECK *>(++itor, commands.end());

    EXPECT_EQ(itor, commands.end());
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
