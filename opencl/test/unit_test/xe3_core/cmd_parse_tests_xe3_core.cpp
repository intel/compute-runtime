/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/cmd_parse/gen_cmd_parse.h"
#include "shared/test/common/cmd_parse/hw_parse.h"
#include "shared/test/common/test_macros/header/per_product_test_definitions.h"
#include "shared/test/common/test_macros/test.h"

#include "opencl/test/unit_test/fixtures/cl_device_fixture.h"

#include "hw_cmds_xe3_core.h"

using namespace NEO;

using CmdParseTestsXe3Core = ::testing::Test;

XE3_CORETEST_F(CmdParseTestsXe3Core, givenMiMemFenceCmdWhenParsingThenFindCommandAndsItsName) {
    using MI_MEM_FENCE = typename FamilyType::MI_MEM_FENCE;

    uint32_t buffer[64] = {};
    LinearStream cmdStream(buffer, sizeof(buffer));

    auto miMemFenceCmd = cmdStream.getSpaceForCmd<MI_MEM_FENCE>();
    miMemFenceCmd->init();

    EXPECT_NE(nullptr, genCmdCast<MI_MEM_FENCE *>(buffer));

    auto commandName = CmdParse<FamilyType>::getCommandName(buffer);
    EXPECT_EQ(0, strcmp(commandName, "MI_MEM_FENCE"));

    HardwareParse hwParser;

    hwParser.parseCommands<FamilyType>(cmdStream, 0);

    EXPECT_EQ(1u, hwParser.cmdList.size());
}

XE3_CORETEST_F(CmdParseTestsXe3Core, givenStateSystemMemFenceAddrCmdWhenParsingThenFindCommandAndsItsName) {
    using STATE_SYSTEM_MEM_FENCE_ADDRESS = typename FamilyType::STATE_SYSTEM_MEM_FENCE_ADDRESS;

    uint32_t buffer[64] = {};
    LinearStream cmdStream(buffer, sizeof(buffer));

    auto stateSystemMemFenceCmd = cmdStream.getSpaceForCmd<STATE_SYSTEM_MEM_FENCE_ADDRESS>();
    stateSystemMemFenceCmd->init();

    EXPECT_NE(nullptr, genCmdCast<STATE_SYSTEM_MEM_FENCE_ADDRESS *>(buffer));

    auto commandName = CmdParse<FamilyType>::getCommandName(buffer);
    EXPECT_EQ(0, strcmp(commandName, "STATE_SYSTEM_MEM_FENCE_ADDRESS"));

    HardwareParse hwParser;

    hwParser.parseCommands<FamilyType>(cmdStream, 0);

    EXPECT_EQ(1u, hwParser.cmdList.size());
}
