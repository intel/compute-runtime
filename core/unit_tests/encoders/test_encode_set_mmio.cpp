/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/command_container/command_encoder.h"
#include "test.h"
#include "unit_tests/fixtures/device_fixture.h"
#include "unit_tests/gen_common/gen_cmd_parse.h"

using namespace NEO;

class CommandSetMMIOFixture : public DeviceFixture {
  public:
    void SetUp() {
        DeviceFixture::SetUp();
        cmdContainer = std::make_unique<CommandContainer>();
        cmdContainer->initialize(pDevice);
    }
    void TearDown() {
        cmdContainer.reset();
        DeviceFixture::TearDown();
    }
    std::unique_ptr<CommandContainer> cmdContainer;
};

using CommandSetMMIOTest = Test<CommandSetMMIOFixture>;

HWTEST_F(CommandSetMMIOTest, appendsAMI_LOAD_REGISTER_IMM) {
    EncodeSetMMIO<FamilyType>::encodeIMM(*cmdContainer.get(), 0xf00, 0xbaa);

    GenCmdList commands;
    CmdParse<FamilyType>::parseCommandBuffer(commands, ptrOffset(cmdContainer->getCommandStream()->getCpuBase(), 0), cmdContainer->getCommandStream()->getUsed());

    using MI_LOAD_REGISTER_IMM = typename FamilyType::MI_LOAD_REGISTER_IMM;
    auto itorLRI = find<MI_LOAD_REGISTER_IMM *>(commands.begin(), commands.end());
    ASSERT_NE(itorLRI, commands.end());
    {
        auto cmd = genCmdCast<MI_LOAD_REGISTER_IMM *>(*itorLRI);
        EXPECT_EQ(cmd->getRegisterOffset(), 0xf00u);
        EXPECT_EQ(cmd->getDataDword(), 0xbaau);
    }
}

HWTEST_F(CommandSetMMIOTest, appendsAMI_LOAD_REGISTER_MEM) {
    EncodeSetMMIO<FamilyType>::encodeMEM(*cmdContainer.get(), 0xf00, 0xDEADBEEFCAF0);

    GenCmdList commands;
    CmdParse<FamilyType>::parseCommandBuffer(commands, ptrOffset(cmdContainer->getCommandStream()->getCpuBase(), 0), cmdContainer->getCommandStream()->getUsed());

    using MI_LOAD_REGISTER_MEM = typename FamilyType::MI_LOAD_REGISTER_MEM;
    auto itorLRI = find<MI_LOAD_REGISTER_MEM *>(commands.begin(), commands.end());
    ASSERT_NE(itorLRI, commands.end());
    {
        auto cmd = genCmdCast<MI_LOAD_REGISTER_MEM *>(*itorLRI);
        EXPECT_EQ(cmd->getRegisterAddress(), 0xf00u);
        EXPECT_EQ(cmd->getMemoryAddress(), 0xDEADBEEFCAF0u);
    }
}

HWTEST_F(CommandSetMMIOTest, appendsAMI_LOAD_REGISTER_REG) {
    EncodeSetMMIO<FamilyType>::encodeREG(*cmdContainer.get(), 0xf10, 0xaf0);

    GenCmdList commands;
    CmdParse<FamilyType>::parseCommandBuffer(commands, ptrOffset(cmdContainer->getCommandStream()->getCpuBase(), 0), cmdContainer->getCommandStream()->getUsed());

    using MI_LOAD_REGISTER_REG = typename FamilyType::MI_LOAD_REGISTER_REG;
    auto itorLRI = find<MI_LOAD_REGISTER_REG *>(commands.begin(), commands.end());
    ASSERT_NE(itorLRI, commands.end());
    {
        auto cmd = genCmdCast<MI_LOAD_REGISTER_REG *>(*itorLRI);
        EXPECT_EQ(cmd->getDestinationRegisterAddress(), 0xf10u);
        EXPECT_EQ(cmd->getSourceRegisterAddress(), 0xaf0u);
    }
}
