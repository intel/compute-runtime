/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/cmdcontainer.h"
#include "shared/source/command_container/command_encoder.h"
#include "shared/test/common/cmd_parse/gen_cmd_parse.h"
#include "shared/test/common/fixtures/device_fixture.h"
#include "shared/test/common/test_macros/hw_test.h"

using namespace NEO;

using EncodeBatchBufferStartOrEndTest = Test<DeviceFixture>;

HWTEST_F(EncodeBatchBufferStartOrEndTest, givenCommandContainerWhenEncodeBBEndThenCommandIsAdded) {
    CommandContainer cmdContainer;
    cmdContainer.initialize(pDevice, nullptr, true);
    EncodeBatchBufferStartOrEnd<FamilyType>::programBatchBufferEnd(cmdContainer);

    GenCmdList commands;
    CmdParse<FamilyType>::parseCommandBuffer(commands, ptrOffset(cmdContainer.getCommandStream()->getCpuBase(), 0), cmdContainer.getCommandStream()->getUsed());

    using MI_BATCH_BUFFER_END = typename FamilyType::MI_BATCH_BUFFER_END;
    auto itor = find<MI_BATCH_BUFFER_END *>(commands.begin(), commands.end());
    ASSERT_NE(itor, commands.end());
}

HWTEST_F(EncodeBatchBufferStartOrEndTest, givenCommandContainerWhenEncodeBBStartThenCommandIsAdded) {
    CommandContainer cmdContainer;
    cmdContainer.initialize(pDevice, nullptr, true);
    EncodeBatchBufferStartOrEnd<FamilyType>::programBatchBufferStart(cmdContainer.getCommandStream(), 0, true);

    GenCmdList commands;
    CmdParse<FamilyType>::parseCommandBuffer(commands, ptrOffset(cmdContainer.getCommandStream()->getCpuBase(), 0), cmdContainer.getCommandStream()->getUsed());

    using MI_BATCH_BUFFER_START = typename FamilyType::MI_BATCH_BUFFER_START;
    auto itor = find<MI_BATCH_BUFFER_START *>(commands.begin(), commands.end());
    ASSERT_NE(itor, commands.end());
}

HWTEST_F(EncodeBatchBufferStartOrEndTest, givenCommandContainerWhenEncodeBBStartWithSecondLevelParameterThenCommandIsProgrammedCorrectly) {
    CommandContainer cmdContainer;
    cmdContainer.initialize(pDevice, nullptr, true);
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
    cmdContainer.initialize(pDevice, nullptr, true);
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
    cmdContainer.initialize(pDevice, nullptr, true);

    uint64_t gpuAddress = 12 * MemoryConstants::pageSize;
    EncodeBatchBufferStartOrEnd<FamilyType>::programBatchBufferStart(cmdContainer.getCommandStream(), gpuAddress, false);

    GenCmdList commands;
    CmdParse<FamilyType>::parseCommandBuffer(commands, ptrOffset(cmdContainer.getCommandStream()->getCpuBase(), 0), cmdContainer.getCommandStream()->getUsed());

    using MI_BATCH_BUFFER_START = typename FamilyType::MI_BATCH_BUFFER_START;
    auto itor = find<MI_BATCH_BUFFER_START *>(commands.begin(), commands.end());
    ASSERT_NE(itor, commands.end());
    {
        auto cmd = genCmdCast<MI_BATCH_BUFFER_START *>(*itor);
        EXPECT_EQ(gpuAddress, cmd->getBatchBufferStartAddress());
    }
}

using EncodeNoopTest = Test<DeviceFixture>;

HWTEST_F(EncodeNoopTest, WhenAligningLinearStreamToCacheLineSizeThenItIsAlignedCorrectly) {
    CommandContainer cmdContainer;
    cmdContainer.initialize(pDevice, nullptr, true);
    auto commandStream = cmdContainer.getCommandStream();

    EncodeNoop<FamilyType>::alignToCacheLine(*commandStream);
    EXPECT_EQ(0u, commandStream->getUsed() % MemoryConstants::cacheLineSize);

    commandStream->getSpace(4);
    EncodeNoop<FamilyType>::alignToCacheLine(*commandStream);
    EXPECT_EQ(0u, commandStream->getUsed() % MemoryConstants::cacheLineSize);
}

HWTEST_F(EncodeNoopTest, WhenEmittingNoopsThenExpectCorrectNumberOfBytesNooped) {
    CommandContainer cmdContainer;
    cmdContainer.initialize(pDevice, nullptr, true);
    auto commandStream = cmdContainer.getCommandStream();

    size_t usedBefore = commandStream->getUsed();

    EncodeNoop<FamilyType>::emitNoop(*commandStream, 0);
    size_t usedAfter = commandStream->getUsed();
    EXPECT_EQ(usedBefore, usedAfter);

    size_t noopingSize = 4;
    EncodeNoop<FamilyType>::emitNoop(*commandStream, noopingSize);
    usedAfter = commandStream->getUsed();
    EXPECT_EQ(noopingSize, (usedAfter - usedBefore));
}
