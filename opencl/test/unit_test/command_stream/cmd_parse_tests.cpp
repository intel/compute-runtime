/*
 * Copyright (C) 2018-2022 Intel Corporation
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
    typedef typename FamilyType::PARSE PARSE;
    GenCmdList cmds;
    EXPECT_FALSE(PARSE::parseCommandBuffer(cmds, nullptr, sizeof(void *)));
}

HWTEST_F(CommandParse, WhenGeneratingCommandBufferThenDoesNotContainGarbage) {
    typedef typename FamilyType::PARSE PARSE;
    uint32_t buffer[30] = {0xbaadf00d};
    GenCmdList cmds;

    EXPECT_FALSE(PARSE::parseCommandBuffer(cmds, buffer, sizeof(uint32_t)));
}

HWTEST_F(CommandParse, GivenGarbageWhenGeneratingCommandBufferThenLengthIsZero) {
    typedef typename FamilyType::PARSE PARSE;
    uint32_t buffer[30] = {0xbaadf00d};

    EXPECT_EQ(0u, PARSE::getCommandLength(buffer));
}

HWTEST_F(CommandParse, WhenGeneratingCommandBufferThenBufferIsCorrect) {
    typedef typename FamilyType::PARSE PARSE;
    typedef typename FamilyType::WALKER_TYPE WALKER_TYPE;
    GenCmdList cmds;
    WALKER_TYPE buffer = FamilyType::cmdInitGpgpuWalker;

    EXPECT_TRUE(PARSE::parseCommandBuffer(cmds, &buffer, 0));
    EXPECT_FALSE(PARSE::parseCommandBuffer(cmds, &buffer, 1));
    EXPECT_FALSE(PARSE::parseCommandBuffer(cmds, &buffer, 2));
    EXPECT_FALSE(PARSE::parseCommandBuffer(cmds, &buffer, 3));
    EXPECT_FALSE(PARSE::parseCommandBuffer(cmds, &buffer, 4));
    EXPECT_TRUE(PARSE::parseCommandBuffer(cmds, &buffer, sizeof(buffer)));
}
