/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/ptr_math.h"
#include "shared/test/unit_test/fixtures/command_container_fixture.h"

#include "opencl/test/unit_test/gen_common/gen_cmd_parse.h"

using namespace NEO;

using CommandEncodeStatesTest = Test<CommandEncodeStatesFixture>;

HWCMDTEST_F(IGFX_GEN8_CORE, CommandEncodeStatesTest, ecodeMediaInterfaceDescriptor) {
    using MEDIA_STATE_FLUSH = typename FamilyType::MEDIA_STATE_FLUSH;
    using MEDIA_INTERFACE_DESCRIPTOR_LOAD = typename FamilyType::MEDIA_INTERFACE_DESCRIPTOR_LOAD;

    EncodeMediaInterfaceDescriptorLoad<FamilyType>::encode(*cmdContainer.get());
    GenCmdList commands;
    CmdParse<FamilyType>::parseCommandBuffer(commands, ptrOffset(cmdContainer->getCommandStream()->getCpuBase(), 0), cmdContainer->getCommandStream()->getUsed());

    auto itorCmd = find<MEDIA_STATE_FLUSH *>(commands.begin(), commands.end());
    ASSERT_NE(itorCmd, commands.end());

    itorCmd = find<MEDIA_INTERFACE_DESCRIPTOR_LOAD *>(++itorCmd, commands.end());
    ASSERT_NE(itorCmd, commands.end());
}