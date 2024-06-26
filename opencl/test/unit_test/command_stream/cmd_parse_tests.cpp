/*
 * Copyright (C) 2018-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/cmd_parse/gen_cmd_parse.h"
#include "shared/test/common/fixtures/device_fixture.h"
#include "shared/test/common/test_macros/hw_test.h"

using namespace NEO;

using CommandParse = Test<DeviceFixture>;

HWTEST_F(CommandParse, WhenGeneratingCommandBufferThenIsNotNull) {
    typedef typename FamilyType::Parse Parse;
    GenCmdList cmds;
    EXPECT_FALSE(Parse::parseCommandBuffer(cmds, nullptr, sizeof(void *)));
}

HWTEST_F(CommandParse, WhenGeneratingCommandBufferThenDoesNotContainGarbage) {
    typedef typename FamilyType::Parse Parse;
    uint32_t buffer[30] = {0xbaadf00d};
    GenCmdList cmds;

    EXPECT_FALSE(Parse::parseCommandBuffer(cmds, buffer, sizeof(uint32_t)));
}

HWTEST_F(CommandParse, GivenGarbageWhenGeneratingCommandBufferThenLengthIsZero) {
    typedef typename FamilyType::Parse Parse;
    uint32_t buffer[30] = {0xbaadf00d};

    EXPECT_EQ(0u, Parse::getCommandLength(buffer));
}

HWTEST_F(CommandParse, WhenGeneratingCommandBufferThenBufferIsCorrect) {
    typedef typename FamilyType::Parse Parse;
    typedef typename FamilyType::DefaultWalkerType DefaultWalkerType;
    GenCmdList cmds;
    DefaultWalkerType buffer = FamilyType::template getInitGpuWalker<DefaultWalkerType>();

    EXPECT_TRUE(Parse::parseCommandBuffer(cmds, &buffer, 0));
    EXPECT_FALSE(Parse::parseCommandBuffer(cmds, &buffer, 1));
    EXPECT_FALSE(Parse::parseCommandBuffer(cmds, &buffer, 2));
    EXPECT_FALSE(Parse::parseCommandBuffer(cmds, &buffer, 3));
    EXPECT_FALSE(Parse::parseCommandBuffer(cmds, &buffer, 4));
    EXPECT_TRUE(Parse::parseCommandBuffer(cmds, &buffer, sizeof(buffer)));
}
