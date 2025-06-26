/*
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/implicit_scaling.h"
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

    CommandListImp *cmdListImp = static_cast<CommandListImp *>(commandList.get());
    uint32_t dataPartitionSize = 16;
    cmdListImp->appendMultiPartitionPrologue(dataPartitionSize);

    auto usedSpaceAfter = commandContainer.getCommandStream()->getUsed();
    ASSERT_GT(usedSpaceAfter, usedSpaceBefore);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList, ptrOffset(commandContainer.getCommandStream()->getCpuBase(), 0), usedSpaceAfter));

    auto itorLrm = find<MI_LOAD_REGISTER_MEM *>(cmdList.begin(), cmdList.end());
    EXPECT_EQ(cmdList.end(), itorLrm);

    auto itorLri = find<MI_LOAD_REGISTER_IMM *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), itorLri);

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

    CommandListImp *cmdListImp = static_cast<CommandListImp *>(commandList.get());
    uint32_t dataPartitionSize = 16;
    cmdListImp->appendMultiPartitionPrologue(dataPartitionSize);

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

    CommandListImp *cmdListImp = static_cast<CommandListImp *>(commandList.get());
    cmdListImp->appendMultiPartitionEpilogue();

    auto usedSpaceAfter = commandContainer.getCommandStream()->getUsed();
    ASSERT_GT(usedSpaceAfter, usedSpaceBefore);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList, ptrOffset(commandContainer.getCommandStream()->getCpuBase(), 0), usedSpaceAfter));

    auto itorLri = find<MI_LOAD_REGISTER_IMM *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), itorLri);

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

    CommandListImp *cmdListImp = static_cast<CommandListImp *>(commandList.get());
    cmdListImp->appendMultiPartitionEpilogue();

    auto usedSpaceAfter = commandContainer.getCommandStream()->getUsed();
    ASSERT_EQ(usedSpaceAfter, usedSpaceBefore);

    auto result = commandList->close();
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);
}

} // namespace ult
} // namespace L0
