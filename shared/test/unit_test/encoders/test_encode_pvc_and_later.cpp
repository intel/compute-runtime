/*
 * Copyright (C) 2022-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/command_encoder.h"
#include "shared/test/common/cmd_parse/gen_cmd_parse.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/test_macros/hw_test.h"
#include "shared/test/unit_test/fixtures/command_container_fixture.h"
#include "shared/test/unit_test/mocks/mock_dispatch_kernel_encoder_interface.h"

using namespace NEO;

using EncodeCommandsXeHpcAndLaterTests = ::testing::Test;

HWTEST2_F(EncodeCommandsXeHpcAndLaterTests, givenPredicateOrIndirectBitSetWhenProgrammingBbStartThenSetCorrectBits, IsAtLeastXeHpcCore) {
    using MI_BATCH_BUFFER_START = typename FamilyType::MI_BATCH_BUFFER_START;

    MI_BATCH_BUFFER_START cmd = {};
    LinearStream cmdStream(&cmd, sizeof(MI_BATCH_BUFFER_START));

    EncodeBatchBufferStartOrEnd<FamilyType>::programBatchBufferStart(&cmdStream, 0, true, true, true);
    EXPECT_EQ(1u, cmd.getIndirectAddressEnable());
    EXPECT_EQ(1u, cmd.getPredicationEnable());
}

struct EncodeConditionalBatchBufferStartTest : public ::testing::Test {
    template <typename FamilyType>
    void validateBaseProgramming(void *currentCmd, CompareOperation compareOperation, uint64_t startAddress, bool indirect, AluRegisters regA, AluRegisters regB) {
        using MI_BATCH_BUFFER_START = typename FamilyType::MI_BATCH_BUFFER_START;
        using MI_SET_PREDICATE = typename FamilyType::MI_SET_PREDICATE;
        using MI_LOAD_REGISTER_REG = typename FamilyType::MI_LOAD_REGISTER_REG;
        using MI_MATH_ALU_INST_INLINE = typename FamilyType::MI_MATH_ALU_INST_INLINE;
        using MI_MATH = typename FamilyType::MI_MATH;

        auto miMathCmd = reinterpret_cast<MI_MATH *>(currentCmd);
        EXPECT_EQ(3u, miMathCmd->DW0.BitField.DwordLength);

        auto miAluCmd = reinterpret_cast<MI_MATH_ALU_INST_INLINE *>(++miMathCmd);
        EXPECT_EQ(static_cast<uint32_t>(AluRegisters::OPCODE_LOAD), miAluCmd->DW0.BitField.ALUOpcode);
        EXPECT_EQ(static_cast<uint32_t>(AluRegisters::R_SRCA), miAluCmd->DW0.BitField.Operand1);
        EXPECT_EQ(static_cast<uint32_t>(regA), miAluCmd->DW0.BitField.Operand2);

        miAluCmd++;
        EXPECT_EQ(static_cast<uint32_t>(AluRegisters::OPCODE_LOAD), miAluCmd->DW0.BitField.ALUOpcode);
        EXPECT_EQ(static_cast<uint32_t>(AluRegisters::R_SRCB), miAluCmd->DW0.BitField.Operand1);
        EXPECT_EQ(static_cast<uint32_t>(regB), miAluCmd->DW0.BitField.Operand2);

        miAluCmd++;
        EXPECT_EQ(static_cast<uint32_t>(AluRegisters::OPCODE_SUB), miAluCmd->DW0.BitField.ALUOpcode);
        EXPECT_EQ(0u, miAluCmd->DW0.BitField.Operand1);
        EXPECT_EQ(0u, miAluCmd->DW0.BitField.Operand2);

        miAluCmd++;
        EXPECT_EQ(static_cast<uint32_t>(AluRegisters::OPCODE_STORE), miAluCmd->DW0.BitField.ALUOpcode);
        EXPECT_EQ(static_cast<uint32_t>(AluRegisters::R_7), miAluCmd->DW0.BitField.Operand1);
        if (compareOperation == CompareOperation::Equal || compareOperation == CompareOperation::NotEqual) {
            EXPECT_EQ(static_cast<uint32_t>(AluRegisters::R_ZF), miAluCmd->DW0.BitField.Operand2);
        } else {
            EXPECT_EQ(static_cast<uint32_t>(AluRegisters::R_CF), miAluCmd->DW0.BitField.Operand2);
        }

        auto lrrCmd = reinterpret_cast<MI_LOAD_REGISTER_REG *>(++miAluCmd);
        EXPECT_EQ(CS_PREDICATE_RESULT_2, lrrCmd->getDestinationRegisterAddress());
        EXPECT_EQ(CS_GPR_R7, lrrCmd->getSourceRegisterAddress());

        auto predicateCmd = reinterpret_cast<MI_SET_PREDICATE *>(++lrrCmd);
        if (compareOperation == CompareOperation::Equal) {
            EXPECT_EQ(static_cast<uint32_t>(MiPredicateType::NoopOnResult2Clear), predicateCmd->getPredicateEnable());
        } else {
            EXPECT_EQ(static_cast<uint32_t>(MiPredicateType::NoopOnResult2Set), predicateCmd->getPredicateEnable());
        }

        auto bbStartCmd = reinterpret_cast<MI_BATCH_BUFFER_START *>(++predicateCmd);
        EXPECT_EQ(1u, bbStartCmd->getPredicationEnable());
        EXPECT_EQ(indirect, bbStartCmd->getIndirectAddressEnable());
        if (!indirect) {
            EXPECT_EQ(startAddress, bbStartCmd->getBatchBufferStartAddress());
        }

        predicateCmd = reinterpret_cast<MI_SET_PREDICATE *>(++bbStartCmd);
        EXPECT_EQ(static_cast<uint32_t>(MiPredicateType::Disable), predicateCmd->getPredicateEnable());
    }
};

HWTEST2_F(EncodeConditionalBatchBufferStartTest, whenProgrammingConditionalDataMemBatchBufferStartThenProgramCorrectMathOperations, IsAtLeastXeHpcCore) {
    using MI_BATCH_BUFFER_START = typename FamilyType::MI_BATCH_BUFFER_START;
    using MI_LOAD_REGISTER_MEM = typename FamilyType::MI_LOAD_REGISTER_MEM;
    using MI_LOAD_REGISTER_IMM = typename FamilyType::MI_LOAD_REGISTER_IMM;

    constexpr size_t expectedSize = sizeof(MI_LOAD_REGISTER_MEM) + (3 * sizeof(MI_LOAD_REGISTER_IMM)) +
                                    EncodeAluHelper<FamilyType, 4>::getCmdsSize() + sizeof(typename FamilyType::MI_LOAD_REGISTER_REG) +
                                    (2 * EncodeMiPredicate<FamilyType>::getCmdSize()) + sizeof(MI_BATCH_BUFFER_START);

    EXPECT_EQ(expectedSize, EncodeBatchBufferStartOrEnd<FamilyType>::getCmdSizeConditionalDataMemBatchBufferStart());

    constexpr uint64_t startAddress = 0x12340000;
    constexpr uint64_t compareAddress = 0x56780000;
    constexpr uint32_t compareData = 9876;

    for (auto compareOperation : {CompareOperation::Equal, CompareOperation::NotEqual, CompareOperation::GreaterOrEqual}) {
        for (bool indirect : {false, true}) {
            uint8_t buffer[expectedSize] = {};
            LinearStream cmdStream(buffer, expectedSize);

            EncodeBatchBufferStartOrEnd<FamilyType>::programConditionalDataMemBatchBufferStart(cmdStream, indirect ? 0 : startAddress, compareAddress, compareData, compareOperation, indirect);

            EXPECT_EQ(expectedSize, cmdStream.getUsed());

            auto lrmCmd = reinterpret_cast<MI_LOAD_REGISTER_MEM *>(buffer);
            EXPECT_EQ(CS_GPR_R7, lrmCmd->getRegisterAddress());
            EXPECT_EQ(compareAddress, lrmCmd->getMemoryAddress());

            auto lriCmd = reinterpret_cast<MI_LOAD_REGISTER_IMM *>(++lrmCmd);
            EXPECT_EQ(CS_GPR_R7 + 4, lriCmd->getRegisterOffset());
            EXPECT_EQ(0u, lriCmd->getDataDword());

            lriCmd++;
            EXPECT_EQ(CS_GPR_R8, lriCmd->getRegisterOffset());
            EXPECT_EQ(compareData, lriCmd->getDataDword());

            lriCmd++;
            EXPECT_EQ(CS_GPR_R8 + 4, lriCmd->getRegisterOffset());
            EXPECT_EQ(0u, lriCmd->getDataDword());

            validateBaseProgramming<FamilyType>(++lriCmd, compareOperation, startAddress, indirect, AluRegisters::R_7, AluRegisters::R_8);
        }
    }
}

HWTEST2_F(EncodeConditionalBatchBufferStartTest, whenProgrammingConditionalDataRegBatchBufferStartThenProgramCorrectMathOperations, IsAtLeastXeHpcCore) {
    using MI_BATCH_BUFFER_START = typename FamilyType::MI_BATCH_BUFFER_START;
    using MI_LOAD_REGISTER_REG = typename FamilyType::MI_LOAD_REGISTER_REG;
    using MI_LOAD_REGISTER_IMM = typename FamilyType::MI_LOAD_REGISTER_IMM;

    constexpr size_t expectedSize = sizeof(MI_LOAD_REGISTER_REG) + (3 * sizeof(MI_LOAD_REGISTER_IMM)) +
                                    EncodeAluHelper<FamilyType, 4>::getCmdsSize() + sizeof(typename FamilyType::MI_LOAD_REGISTER_REG) +
                                    (2 * EncodeMiPredicate<FamilyType>::getCmdSize()) + sizeof(MI_BATCH_BUFFER_START);

    EXPECT_EQ(expectedSize, EncodeBatchBufferStartOrEnd<FamilyType>::getCmdSizeConditionalDataRegBatchBufferStart());

    constexpr uint64_t startAddress = 0x12340000;
    constexpr uint32_t compareReg = CS_GPR_R1;
    constexpr uint32_t compareData = 9876;

    for (auto compareOperation : {CompareOperation::Equal, CompareOperation::NotEqual, CompareOperation::GreaterOrEqual}) {
        for (bool indirect : {false, true}) {
            uint8_t buffer[expectedSize] = {};
            LinearStream cmdStream(buffer, expectedSize);

            EncodeBatchBufferStartOrEnd<FamilyType>::programConditionalDataRegBatchBufferStart(cmdStream, indirect ? 0 : startAddress, compareReg, compareData, compareOperation, indirect);

            EXPECT_EQ(expectedSize, cmdStream.getUsed());

            auto lrrCmd = reinterpret_cast<MI_LOAD_REGISTER_REG *>(buffer);
            EXPECT_EQ(CS_GPR_R7, lrrCmd->getDestinationRegisterAddress());
            EXPECT_EQ(compareReg, lrrCmd->getSourceRegisterAddress());

            auto lriCmd = reinterpret_cast<MI_LOAD_REGISTER_IMM *>(++lrrCmd);
            EXPECT_EQ(CS_GPR_R7 + 4, lriCmd->getRegisterOffset());
            EXPECT_EQ(0u, lriCmd->getDataDword());

            lriCmd++;
            EXPECT_EQ(CS_GPR_R8, lriCmd->getRegisterOffset());
            EXPECT_EQ(compareData, lriCmd->getDataDword());

            lriCmd++;
            EXPECT_EQ(CS_GPR_R8 + 4, lriCmd->getRegisterOffset());
            EXPECT_EQ(0u, lriCmd->getDataDword());

            validateBaseProgramming<FamilyType>(++lriCmd, compareOperation, startAddress, indirect, AluRegisters::R_7, AluRegisters::R_8);
        }
    }
}

HWTEST2_F(EncodeConditionalBatchBufferStartTest, whenProgrammingConditionalRegRegBatchBufferStartThenProgramCorrectMathOperations, IsAtLeastXeHpcCore) {
    using MI_BATCH_BUFFER_START = typename FamilyType::MI_BATCH_BUFFER_START;

    constexpr size_t expectedSize = EncodeAluHelper<FamilyType, 4>::getCmdsSize() + sizeof(typename FamilyType::MI_LOAD_REGISTER_REG) +
                                    (2 * EncodeMiPredicate<FamilyType>::getCmdSize()) + sizeof(MI_BATCH_BUFFER_START);

    EXPECT_EQ(expectedSize, EncodeBatchBufferStartOrEnd<FamilyType>::getCmdSizeConditionalRegRegBatchBufferStart());

    constexpr uint64_t startAddress = 0x12340000;
    constexpr AluRegisters compareReg1 = AluRegisters::R_1;
    constexpr AluRegisters compareReg2 = AluRegisters::R_2;

    for (auto compareOperation : {CompareOperation::Equal, CompareOperation::NotEqual, CompareOperation::GreaterOrEqual}) {
        for (bool indirect : {false, true}) {
            uint8_t buffer[expectedSize] = {};
            LinearStream cmdStream(buffer, expectedSize);

            EncodeBatchBufferStartOrEnd<FamilyType>::programConditionalRegRegBatchBufferStart(cmdStream, indirect ? 0 : startAddress, compareReg1, compareReg2, compareOperation, indirect);

            EXPECT_EQ(expectedSize, cmdStream.getUsed());

            validateBaseProgramming<FamilyType>(buffer, compareOperation, startAddress, indirect, compareReg1, compareReg2);
        }
    }
}

using CommandEncodeStatesXeHpcAndLaterTests = Test<CommandEncodeStatesFixture>;

HWTEST2_F(CommandEncodeStatesXeHpcAndLaterTests, givenDebugFlagSetWhenProgrammingWalkerThenSetFlushingBits, IsAtLeastXeHpcCore) {
    DebugManagerStateRestore restore;
    DebugManager.flags.ForceComputeWalkerPostSyncFlush.set(1);

    uint32_t dims[] = {2, 1, 1};
    std::unique_ptr<MockDispatchKernelEncoder> dispatchInterface(new MockDispatchKernelEncoder());
    bool requiresUncachedMocs = false;
    EncodeDispatchKernelArgs dispatchArgs = createDefaultDispatchKernelArgs(pDevice, dispatchInterface.get(), dims, requiresUncachedMocs);

    EncodeDispatchKernel<FamilyType>::encode(*cmdContainer.get(), dispatchArgs, nullptr);

    GenCmdList commands;
    CmdParse<FamilyType>::parseCommandBuffer(commands, ptrOffset(cmdContainer->getCommandStream()->getCpuBase(), 0), cmdContainer->getCommandStream()->getUsed());

    using WALKER_TYPE = typename FamilyType::WALKER_TYPE;
    auto itor = find<WALKER_TYPE *>(commands.begin(), commands.end());
    ASSERT_NE(itor, commands.end());

    auto walkerCmd = genCmdCast<WALKER_TYPE *>(*itor);
    EXPECT_TRUE(walkerCmd->getPostSync().getDataportPipelineFlush());
    EXPECT_TRUE(walkerCmd->getPostSync().getDataportSubsliceCacheFlush());
}