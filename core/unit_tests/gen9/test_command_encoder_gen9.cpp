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

#include "reg_configs_common.h"

using namespace NEO;

using CommandEncoderTest = Test<DeviceFixture>;

GEN9TEST_F(CommandEncoderTest, appendsASetMMIO) {
    CommandContainer cmdContainer;
    cmdContainer.initialize(pDevice);
    EncodeL3State<FamilyType>::encode(cmdContainer, false);

    GenCmdList commands;
    CmdParse<FamilyType>::parseCommandBuffer(commands, ptrOffset(cmdContainer.getCommandStream()->getCpuBase(), 0), cmdContainer.getCommandStream()->getUsed());

    using MI_LOAD_REGISTER_IMM = typename FamilyType::MI_LOAD_REGISTER_IMM;
    auto itorLRI = find<MI_LOAD_REGISTER_IMM *>(commands.begin(), commands.end());
    ASSERT_NE(itorLRI, commands.end());
}

GEN9TEST_F(CommandEncoderTest, givenNoSLMSetCorrectMMIO) {
    CommandContainer cmdContainer;
    cmdContainer.initialize(pDevice);
    EncodeL3State<FamilyType>::encode(cmdContainer, false);

    GenCmdList commands;
    CmdParse<FamilyType>::parseCommandBuffer(commands, ptrOffset(cmdContainer.getCommandStream()->getCpuBase(), 0), cmdContainer.getCommandStream()->getUsed());

    using MI_LOAD_REGISTER_IMM = typename FamilyType::MI_LOAD_REGISTER_IMM;
    auto itorLRI = find<MI_LOAD_REGISTER_IMM *>(commands.begin(), commands.end());
    ASSERT_NE(itorLRI, commands.end());
    auto cmd = genCmdCast<MI_LOAD_REGISTER_IMM *>(*itorLRI);
    auto expectedData = PreambleHelper<FamilyType>::isL3Configurable(cmdContainer.getDevice()->getHardwareInfo()) ? 0x80000340u : 0x60000321u;
    EXPECT_EQ(cmd->getRegisterOffset(), 0x7034u);
    EXPECT_EQ(cmd->getDataDword(), expectedData);
}

GEN9TEST_F(CommandEncoderTest, givenSLMSetCorrectMMIO) {
    CommandContainer cmdContainer;
    cmdContainer.initialize(pDevice);
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
