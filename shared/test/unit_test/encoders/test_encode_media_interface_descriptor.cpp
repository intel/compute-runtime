/*
 * Copyright (C) 2020-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/ptr_math.h"
#include "shared/test/common/cmd_parse/gen_cmd_parse.h"
#include "shared/test/common/test_macros/hw_test.h"
#include "shared/test/unit_test/fixtures/command_container_fixture.h"

using namespace NEO;

using CommandEncodeStatesTest = Test<CommandEncodeStatesFixture>;

HWCMDTEST_F(IGFX_GEN12LP_CORE, CommandEncodeStatesTest, WhenProgrammingThenMediaInterfaceDescriptorLoadIsUsed) {
    using MEDIA_STATE_FLUSH = typename FamilyType::MEDIA_STATE_FLUSH;
    using MEDIA_INTERFACE_DESCRIPTOR_LOAD = typename FamilyType::MEDIA_INTERFACE_DESCRIPTOR_LOAD;

    EncodeMediaInterfaceDescriptorLoad<FamilyType>::encode(*cmdContainer.get(), nullptr);
    GenCmdList commands;
    CmdParse<FamilyType>::parseCommandBuffer(commands, ptrOffset(cmdContainer->getCommandStream()->getCpuBase(), 0), cmdContainer->getCommandStream()->getUsed());

    auto itorCmd = find<MEDIA_STATE_FLUSH *>(commands.begin(), commands.end());
    ASSERT_NE(itorCmd, commands.end());

    itorCmd = find<MEDIA_INTERFACE_DESCRIPTOR_LOAD *>(++itorCmd, commands.end());
    ASSERT_NE(itorCmd, commands.end());
}