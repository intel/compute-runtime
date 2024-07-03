/*
 * Copyright (C) 2020-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/cmdcontainer.h"
#include "shared/source/command_container/command_encoder.h"
#include "shared/source/gen9/hw_cmds.h"
#include "shared/source/gen_common/reg_configs_common.h"
#include "shared/source/helpers/blit_commands_helper.h"
#include "shared/source/indirect_heap/heap_size.h"
#include "shared/test/common/cmd_parse/gen_cmd_parse.h"
#include "shared/test/common/fixtures/device_fixture.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/test_macros/header/per_product_test_definitions.h"
#include "shared/test/common/test_macros/test.h"

using namespace NEO;

using CommandEncoderTest = Test<DeviceFixture>;

GEN9TEST_F(CommandEncoderTest, WhenProgrammingThenLoadRegisterImmIsUsed) {
    CommandContainer cmdContainer;
    cmdContainer.initialize(pDevice, nullptr, HeapSize::defaultHeapSize, true, false);
    EncodeL3State<FamilyType>::encode(cmdContainer, false);

    GenCmdList commands;
    CmdParse<FamilyType>::parseCommandBuffer(commands, ptrOffset(cmdContainer.getCommandStream()->getCpuBase(), 0), cmdContainer.getCommandStream()->getUsed());

    using MI_LOAD_REGISTER_IMM = typename FamilyType::MI_LOAD_REGISTER_IMM;
    auto itorLRI = find<MI_LOAD_REGISTER_IMM *>(commands.begin(), commands.end());
    ASSERT_NE(itorLRI, commands.end());
}

GEN9TEST_F(CommandEncoderTest, givenNoSlmThenCorrectMmioIsSet) {
    CommandContainer cmdContainer;
    cmdContainer.initialize(pDevice, nullptr, HeapSize::defaultHeapSize, true, false);
    EncodeL3State<FamilyType>::encode(cmdContainer, false);

    GenCmdList commands;
    CmdParse<FamilyType>::parseCommandBuffer(commands, ptrOffset(cmdContainer.getCommandStream()->getCpuBase(), 0), cmdContainer.getCommandStream()->getUsed());

    using MI_LOAD_REGISTER_IMM = typename FamilyType::MI_LOAD_REGISTER_IMM;
    auto itorLRI = find<MI_LOAD_REGISTER_IMM *>(commands.begin(), commands.end());
    ASSERT_NE(itorLRI, commands.end());
    auto cmd = genCmdCast<MI_LOAD_REGISTER_IMM *>(*itorLRI);
    auto isL3Configurable = (PreambleHelper<FamilyType>::getL3Config(pDevice->getHardwareInfo(), true) != PreambleHelper<FamilyType>::getL3Config(pDevice->getHardwareInfo(), false));
    auto expectedData = isL3Configurable ? 0x80000340u : 0x60000321u;
    EXPECT_EQ(cmd->getRegisterOffset(), 0x7034u);
    EXPECT_EQ(cmd->getDataDword(), expectedData);
}

GEN9TEST_F(CommandEncoderTest, givenSlmThenCorrectMmioIsSet) {
    CommandContainer cmdContainer;
    cmdContainer.initialize(pDevice, nullptr, HeapSize::defaultHeapSize, true, false);
    EncodeL3State<FamilyType>::encode(cmdContainer, true);

    GenCmdList commands;
    CmdParse<FamilyType>::parseCommandBuffer(commands, ptrOffset(cmdContainer.getCommandStream()->getCpuBase(), 0), cmdContainer.getCommandStream()->getUsed());

    using MI_LOAD_REGISTER_IMM = typename FamilyType::MI_LOAD_REGISTER_IMM;
    auto itorLRI = find<MI_LOAD_REGISTER_IMM *>(commands.begin(), commands.end());
    ASSERT_NE(itorLRI, commands.end());
    auto cmd = genCmdCast<MI_LOAD_REGISTER_IMM *>(*itorLRI);
    EXPECT_EQ(cmd->getRegisterOffset(), 0x7034u);
    EXPECT_EQ(cmd->getDataDword(), 0x60000321u);
}

using Gen9CommandEncodeTest = testing::Test;

GEN9TEST_F(Gen9CommandEncodeTest, givenBcsCommandsHelperWhenMiArbCheckWaRequiredThenReturnTrue) {
    EXPECT_FALSE(BlitCommandsHelper<FamilyType>::miArbCheckWaRequired());
}
