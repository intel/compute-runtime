/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/command_container/cmdcontainer.h"
#include "core/command_container/command_encoder.h"
#include "core/unit_tests/mocks/mock_dispatch_kernel_encoder_interface.h"
#include "test.h"
#include "unit_tests/fixtures/device_fixture.h"
#include "unit_tests/gen_common/gen_cmd_parse.h"

using namespace NEO;

using EncodeFlushTest = Test<DeviceFixture>;

HWTEST_F(EncodeFlushTest, givenCommandContainerWhenEncodeFluchCalledThenCommandIsAdded) {
    CommandContainer cmdContainer;
    cmdContainer.initialize(pDevice);
    EncodeFlush<FamilyType>::encode(cmdContainer);

    GenCmdList commands;
    CmdParse<FamilyType>::parseCommandBuffer(commands, ptrOffset(cmdContainer.getCommandStream()->getCpuBase(), 0), cmdContainer.getCommandStream()->getUsed());

    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    auto itorPC = find<PIPE_CONTROL *>(commands.begin(), commands.end());
    ASSERT_NE(itorPC, commands.end());
    {
        auto cmd = genCmdCast<PIPE_CONTROL *>(*itorPC);
        EXPECT_TRUE(cmd->getCommandStreamerStallEnable());
        EXPECT_TRUE(cmd->getDcFlushEnable());
    }
}

HWTEST_F(EncodeFlushTest, givenCommandContainerAndDcFlushEnabledWhenEncodeWithQWordCalledThenCommandIsAdded) {
    CommandContainer cmdContainer;
    cmdContainer.initialize(pDevice);
    uint64_t gpuAddress = 0;
    uint64_t value = 1;
    EncodeFlush<FamilyType>::encodeWithQwordWrite(cmdContainer, gpuAddress, value, true);

    GenCmdList commands;
    CmdParse<FamilyType>::parseCommandBuffer(commands, ptrOffset(cmdContainer.getCommandStream()->getCpuBase(), 0), cmdContainer.getCommandStream()->getUsed());

    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    auto itorPC = find<PIPE_CONTROL *>(commands.begin(), commands.end());
    ASSERT_NE(itorPC, commands.end());
    {
        auto cmd = genCmdCast<PIPE_CONTROL *>(*itorPC);
        EXPECT_TRUE(cmd->getCommandStreamerStallEnable());
        EXPECT_TRUE(cmd->getDcFlushEnable());
        EXPECT_EQ(cmd->getImmediateData(), value);
    }
}

HWTEST_F(EncodeFlushTest, givenCommandContainerAndDcFlushDisabledWhenEncodeWithQWordCalledThenCommandIsAdded) {
    CommandContainer cmdContainer;
    cmdContainer.initialize(pDevice);
    uint64_t gpuAddress = 0;
    uint64_t value = 1;
    EncodeFlush<FamilyType>::encodeWithQwordWrite(cmdContainer, gpuAddress, value, false);

    GenCmdList commands;
    CmdParse<FamilyType>::parseCommandBuffer(commands, ptrOffset(cmdContainer.getCommandStream()->getCpuBase(), 0), cmdContainer.getCommandStream()->getUsed());

    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    auto itorPC = find<PIPE_CONTROL *>(commands.begin(), commands.end());
    ASSERT_NE(itorPC, commands.end());
    {
        auto cmd = genCmdCast<PIPE_CONTROL *>(*itorPC);
        EXPECT_TRUE(cmd->getCommandStreamerStallEnable());
        EXPECT_FALSE(cmd->getDcFlushEnable());
        EXPECT_EQ(cmd->getImmediateData(), value);
    }
}