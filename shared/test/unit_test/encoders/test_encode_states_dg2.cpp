/*
 * Copyright (C) 2022-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/gmm_helper/gmm_lib.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/test/common/cmd_parse/gen_cmd_parse.h"
#include "shared/test/common/mocks/mock_debugger.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/test_macros/hw_test.h"
#include "shared/test/unit_test/fixtures/command_container_fixture.h"

#include "test_traits_common.h"

using namespace NEO;

using CommandEncodeStatesDG2Test = Test<CommandEncodeStatesFixture>;

HWTEST2_F(CommandEncodeStatesDG2Test, givenCommandContainerWhenSetStateBaseAddressCalledThenCachePolicyIsWB, IsDG2) {
    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;
    cmdContainer->dirtyHeaps = 0;

    STATE_BASE_ADDRESS sba;
    auto gmmHelper = cmdContainer->getDevice()->getRootDeviceEnvironment().getGmmHelper();
    uint32_t statelessMocsIndex = (gmmHelper->getL3EnabledMOCS() >> 1);

    EncodeStateBaseAddressArgs<FamilyType> args = createDefaultEncodeStateBaseAddressArgs<FamilyType>(cmdContainer.get(), sba, statelessMocsIndex);

    EncodeStateBaseAddress<FamilyType>::encode(args);

    GenCmdList commands;
    CmdParse<FamilyType>::parseCommandBuffer(commands, ptrOffset(cmdContainer->getCommandStream()->getCpuBase(), 0), cmdContainer->getCommandStream()->getUsed());

    auto itorCmd = find<STATE_BASE_ADDRESS *>(commands.begin(), commands.end());
    ASSERT_NE(itorCmd, commands.end());

    auto cmd = genCmdCast<STATE_BASE_ADDRESS *>(*itorCmd);

    EXPECT_EQ(FamilyType::STATE_BASE_ADDRESS::L1_CACHE_CONTROL_WB, cmd->getL1CacheControlCachePolicy());
}

HWTEST2_F(CommandEncodeStatesDG2Test, givenCommandContainerAndDebuggerWhenSetStateBaseAddressCalledThenCachePolicyIsWBP, IsDG2) {
    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;
    cmdContainer->dirtyHeaps = 0;
    auto debugger = new MockDebugger();

    cmdContainer->getDevice()->getExecutionEnvironment()->rootDeviceEnvironments[0]->debugger.reset(debugger);
    STATE_BASE_ADDRESS sba;
    auto gmmHelper = cmdContainer->getDevice()->getRootDeviceEnvironment().getGmmHelper();
    uint32_t statelessMocsIndex = (gmmHelper->getL3EnabledMOCS() >> 1);

    EncodeStateBaseAddressArgs<FamilyType> args = createDefaultEncodeStateBaseAddressArgs<FamilyType>(cmdContainer.get(), sba, statelessMocsIndex);
    EncodeStateBaseAddress<FamilyType>::encode(args);

    GenCmdList commands;
    CmdParse<FamilyType>::parseCommandBuffer(commands, ptrOffset(cmdContainer->getCommandStream()->getCpuBase(), 0), cmdContainer->getCommandStream()->getUsed());

    auto itorCmd = find<STATE_BASE_ADDRESS *>(commands.begin(), commands.end());
    ASSERT_NE(itorCmd, commands.end());

    auto cmd = genCmdCast<STATE_BASE_ADDRESS *>(*itorCmd);

    EXPECT_EQ(FamilyType::STATE_BASE_ADDRESS::L1_CACHE_CONTROL_WBP, cmd->getL1CacheControlCachePolicy());
}
