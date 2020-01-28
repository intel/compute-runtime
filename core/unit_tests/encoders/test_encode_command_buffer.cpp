/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/command_container/cmdcontainer.h"
#include "core/command_container/command_encoder.h"
#include "test.h"
#include "unit_tests/fixtures/device_fixture.h"
#include "unit_tests/gen_common/gen_cmd_parse.h"

using namespace NEO;

using EncodeBatchBufferStartOrEndTest = Test<DeviceFixture>;

HWTEST_F(EncodeBatchBufferStartOrEndTest, givenCommandContainerWhenEncodeBBEndThenCommandIsAdded) {
    CommandContainer cmdContainer;
    cmdContainer.initialize(pDevice);
    EncodeBatchBufferStartOrEnd<FamilyType>::programBatchBufferEnd(cmdContainer);

    GenCmdList commands;
    CmdParse<FamilyType>::parseCommandBuffer(commands, ptrOffset(cmdContainer.getCommandStream()->getCpuBase(), 0), cmdContainer.getCommandStream()->getUsed());

    using MI_BATCH_BUFFER_END = typename FamilyType::MI_BATCH_BUFFER_END;
    auto itor = find<MI_BATCH_BUFFER_END *>(commands.begin(), commands.end());
    ASSERT_NE(itor, commands.end());
}

HWTEST_F(EncodeBatchBufferStartOrEndTest, givenCommandContainerWhenEncodeBBStartThenCommandIsAdded) {
    CommandContainer cmdContainer;
    cmdContainer.initialize(pDevice);
    EncodeBatchBufferStartOrEnd<FamilyType>::programBatchBufferStart(cmdContainer.getCommandStream(), 0, true);

    GenCmdList commands;
    CmdParse<FamilyType>::parseCommandBuffer(commands, ptrOffset(cmdContainer.getCommandStream()->getCpuBase(), 0), cmdContainer.getCommandStream()->getUsed());

    using MI_BATCH_BUFFER_START = typename FamilyType::MI_BATCH_BUFFER_START;
    auto itor = find<MI_BATCH_BUFFER_START *>(commands.begin(), commands.end());
    ASSERT_NE(itor, commands.end());
}

HWTEST_F(EncodeBatchBufferStartOrEndTest, givenCommandContainerWhenEncodeBBStartWithSecondLevelParameterThenCommandIsProgrammedCorrectly) {
    CommandContainer cmdContainer;
    cmdContainer.initialize(pDevice);
    EncodeBatchBufferStartOrEnd<FamilyType>::programBatchBufferStart(cmdContainer.getCommandStream(), 0, true);

    GenCmdList commands;
    CmdParse<FamilyType>::parseCommandBuffer(commands, ptrOffset(cmdContainer.getCommandStream()->getCpuBase(), 0), cmdContainer.getCommandStream()->getUsed());

    using MI_BATCH_BUFFER_START = typename FamilyType::MI_BATCH_BUFFER_START;
    auto itor = find<MI_BATCH_BUFFER_START *>(commands.begin(), commands.end());
    ASSERT_NE(itor, commands.end());
    {
        auto cmd = genCmdCast<MI_BATCH_BUFFER_START *>(*itor);
        EXPECT_EQ(MI_BATCH_BUFFER_START::SECOND_LEVEL_BATCH_BUFFER_SECOND_LEVEL_BATCH, cmd->getSecondLevelBatchBuffer());
    }
}

HWTEST_F(EncodeBatchBufferStartOrEndTest, givenCommandContainerWhenEncodeBBStartWithFirstLevelParameterThenCommandIsProgrammedCorrectly) {
    CommandContainer cmdContainer;
    cmdContainer.initialize(pDevice);
    EncodeBatchBufferStartOrEnd<FamilyType>::programBatchBufferStart(cmdContainer.getCommandStream(), 0, false);

    GenCmdList commands;
    CmdParse<FamilyType>::parseCommandBuffer(commands, ptrOffset(cmdContainer.getCommandStream()->getCpuBase(), 0), cmdContainer.getCommandStream()->getUsed());

    using MI_BATCH_BUFFER_START = typename FamilyType::MI_BATCH_BUFFER_START;
    auto itor = find<MI_BATCH_BUFFER_START *>(commands.begin(), commands.end());
    ASSERT_NE(itor, commands.end());
    {
        auto cmd = genCmdCast<MI_BATCH_BUFFER_START *>(*itor);
        EXPECT_EQ(MI_BATCH_BUFFER_START::SECOND_LEVEL_BATCH_BUFFER_FIRST_LEVEL_BATCH, cmd->getSecondLevelBatchBuffer());
    }
}

HWTEST_F(EncodeBatchBufferStartOrEndTest, givenGpuAddressWhenEncodeBBStartThenAddressIsProgrammedCorrectly) {
    CommandContainer cmdContainer;
    cmdContainer.initialize(pDevice);

    uint64_t gpuAddress = 12 * MemoryConstants::pageSize;
    EncodeBatchBufferStartOrEnd<FamilyType>::programBatchBufferStart(cmdContainer.getCommandStream(), gpuAddress, false);

    GenCmdList commands;
    CmdParse<FamilyType>::parseCommandBuffer(commands, ptrOffset(cmdContainer.getCommandStream()->getCpuBase(), 0), cmdContainer.getCommandStream()->getUsed());

    using MI_BATCH_BUFFER_START = typename FamilyType::MI_BATCH_BUFFER_START;
    auto itor = find<MI_BATCH_BUFFER_START *>(commands.begin(), commands.end());
    ASSERT_NE(itor, commands.end());
    {
        auto cmd = genCmdCast<MI_BATCH_BUFFER_START *>(*itor);
        EXPECT_EQ(gpuAddress, cmd->getBatchBufferStartAddressGraphicsaddress472());
    }
}