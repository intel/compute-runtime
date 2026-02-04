/*
 * Copyright (C) 2021-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/implicit_scaling.h"
#include "shared/source/command_stream/linear_stream.h"
#include "shared/test/common/cmd_parse/gen_cmd_parse.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdlist.h"

namespace L0 {
namespace ult {

using MultiPartitionPrologueTest = Test<DeviceFixture>;
HWTEST2_F(MultiPartitionPrologueTest, whenAppendMultiPartitionPrologueIsCalledThenCommandListIsUpdated, IsAtLeastXeCore) {
    using MI_LOAD_REGISTER_MEM = typename FamilyType::MI_LOAD_REGISTER_MEM;
    using MI_LOAD_REGISTER_IMM = typename FamilyType::MI_LOAD_REGISTER_IMM;

    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::compute, 0u, returnValue, false));
    auto &commandContainer = commandList->getCmdContainer();

    ASSERT_NE(nullptr, commandContainer.getCommandStream());
    auto usedSpaceBefore = commandContainer.getCommandStream()->getUsed();

    auto cmdList = commandList.get();
    uint32_t dataPartitionSize = 16;
    cmdList->appendMultiPartitionPrologue(dataPartitionSize);

    auto usedSpaceAfter = commandContainer.getCommandStream()->getUsed();
    ASSERT_GT(usedSpaceAfter, usedSpaceBefore);

    GenCmdList parsedCmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        parsedCmdList, ptrOffset(commandContainer.getCommandStream()->getCpuBase(), 0), usedSpaceAfter));

    auto itorLrm = find<MI_LOAD_REGISTER_MEM *>(parsedCmdList.begin(), parsedCmdList.end());
    EXPECT_EQ(parsedCmdList.end(), itorLrm);

    auto itorLri = find<MI_LOAD_REGISTER_IMM *>(parsedCmdList.begin(), parsedCmdList.end());
    ASSERT_NE(parsedCmdList.end(), itorLri);

    auto lriCmd = genCmdCast<MI_LOAD_REGISTER_IMM *>(*itorLri);
    EXPECT_EQ(NEO::PartitionRegisters<FamilyType>::addressOffsetCCSOffset, static_cast<uint64_t>(lriCmd->getRegisterOffset()));
    EXPECT_EQ(dataPartitionSize, static_cast<uint32_t>(lriCmd->getDataDword()));
    EXPECT_EQ(true, lriCmd->getMmioRemapEnable());

    auto result = commandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

HWTEST2_F(MultiPartitionPrologueTest, whenAppendMultiPartitionPrologueIsCalledThenCommandListIsNotUpdated, IsGen12LP) {
    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::compute, 0u, returnValue, false));
    auto &commandContainer = commandList->getCmdContainer();

    ASSERT_NE(nullptr, commandContainer.getCommandStream());
    auto usedSpaceBefore = commandContainer.getCommandStream()->getUsed();

    auto cmdList = commandList.get();
    uint32_t dataPartitionSize = 16;
    cmdList->appendMultiPartitionPrologue(dataPartitionSize);

    auto usedSpaceAfter = commandContainer.getCommandStream()->getUsed();
    ASSERT_EQ(usedSpaceAfter, usedSpaceBefore);

    auto result = commandList->close();
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);
}
using MultiPartitionEpilogueTest = Test<DeviceFixture>;
HWTEST2_F(MultiPartitionEpilogueTest, whenAppendMultiPartitionEpilogueIsCalledThenCommandListIsUpdated, IsAtLeastXeCore) {
    using MI_LOAD_REGISTER_IMM = typename FamilyType::MI_LOAD_REGISTER_IMM;

    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::compute, 0u, returnValue, false));
    auto &commandContainer = commandList->getCmdContainer();

    ASSERT_NE(nullptr, commandContainer.getCommandStream());
    auto usedSpaceBefore = commandContainer.getCommandStream()->getUsed();

    auto cmdList = commandList.get();
    cmdList->appendMultiPartitionEpilogue();

    auto usedSpaceAfter = commandContainer.getCommandStream()->getUsed();
    ASSERT_GT(usedSpaceAfter, usedSpaceBefore);

    GenCmdList parsedCmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        parsedCmdList, ptrOffset(commandContainer.getCommandStream()->getCpuBase(), 0), usedSpaceAfter));

    auto itorLri = find<MI_LOAD_REGISTER_IMM *>(parsedCmdList.begin(), parsedCmdList.end());
    ASSERT_NE(parsedCmdList.end(), itorLri);

    auto lriCmd = genCmdCast<MI_LOAD_REGISTER_IMM *>(*itorLri);
    EXPECT_EQ(NEO::PartitionRegisters<FamilyType>::addressOffsetCCSOffset, static_cast<uint64_t>(lriCmd->getRegisterOffset()));
    EXPECT_EQ(NEO::ImplicitScalingDispatch<FamilyType>::getImmediateWritePostSyncOffset(), static_cast<uint32_t>(lriCmd->getDataDword()));
    EXPECT_EQ(true, lriCmd->getMmioRemapEnable());

    auto result = commandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

HWTEST2_F(MultiPartitionEpilogueTest, whenAppendMultiPartitionPrologueIsCalledThenCommandListIsNotUpdated, IsGen12LP) {

    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::compute, 0u, returnValue, false));
    auto &commandContainer = commandList->getCmdContainer();

    ASSERT_NE(nullptr, commandContainer.getCommandStream());
    auto usedSpaceBefore = commandContainer.getCommandStream()->getUsed();

    auto cmdList = commandList.get();
    cmdList->appendMultiPartitionEpilogue();

    auto usedSpaceAfter = commandContainer.getCommandStream()->getUsed();
    ASSERT_EQ(usedSpaceAfter, usedSpaceBefore);

    auto result = commandList->close();
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);
}

} // namespace ult
} // namespace L0
