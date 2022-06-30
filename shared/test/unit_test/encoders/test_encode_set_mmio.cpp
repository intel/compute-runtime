/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/command_encoder.h"
#include "shared/test/common/cmd_parse/gen_cmd_parse.h"
#include "shared/test/common/fixtures/device_fixture.h"
#include "shared/test/common/test_macros/test.h"

using namespace NEO;

class CommandSetMMIOFixture : public DeviceFixture {
  public:
    void SetUp() {
        DeviceFixture::SetUp();
        cmdContainer = std::make_unique<CommandContainer>();
        cmdContainer->initialize(pDevice, nullptr, true);
    }
    void TearDown() {
        cmdContainer.reset();
        DeviceFixture::TearDown();
    }
    std::unique_ptr<CommandContainer> cmdContainer;
};

using CommandSetMMIOTest = Test<CommandSetMMIOFixture>;

HWTEST_F(CommandSetMMIOTest, WhenProgrammingThenLoadRegisterImmIsUsed) {
    EncodeSetMMIO<FamilyType>::encodeIMM(*cmdContainer.get(), 0x2000, 0xbaa, false);

    GenCmdList commands;
    CmdParse<FamilyType>::parseCommandBuffer(commands, ptrOffset(cmdContainer->getCommandStream()->getCpuBase(), 0), cmdContainer->getCommandStream()->getUsed());

    using MI_LOAD_REGISTER_IMM = typename FamilyType::MI_LOAD_REGISTER_IMM;
    auto itorLRI = find<MI_LOAD_REGISTER_IMM *>(commands.begin(), commands.end());
    ASSERT_NE(itorLRI, commands.end());
    {
        auto cmd = genCmdCast<MI_LOAD_REGISTER_IMM *>(*itorLRI);
        EXPECT_EQ(cmd->getRegisterOffset(), 0x2000u);
        EXPECT_EQ(cmd->getDataDword(), 0xbaau);
    }
}

HWTEST_F(CommandSetMMIOTest, WhenProgrammingThenLoadRegisterMemIsUsed) {
    EncodeSetMMIO<FamilyType>::encodeMEM(*cmdContainer.get(), 0x2000, 0xDEADBEEFCAF0);

    GenCmdList commands;
    CmdParse<FamilyType>::parseCommandBuffer(commands, ptrOffset(cmdContainer->getCommandStream()->getCpuBase(), 0), cmdContainer->getCommandStream()->getUsed());

    using MI_LOAD_REGISTER_MEM = typename FamilyType::MI_LOAD_REGISTER_MEM;
    auto itorLRI = find<MI_LOAD_REGISTER_MEM *>(commands.begin(), commands.end());
    ASSERT_NE(itorLRI, commands.end());
    {
        auto cmd = genCmdCast<MI_LOAD_REGISTER_MEM *>(*itorLRI);
        EXPECT_EQ(cmd->getRegisterAddress(), 0x2000u);
        EXPECT_EQ(cmd->getMemoryAddress(), 0xDEADBEEFCAF0u);
    }
}

HWTEST_F(CommandSetMMIOTest, WhenProgrammingThenLoadRegisterRegIsUsed) {
    EncodeSetMMIO<FamilyType>::encodeREG(*cmdContainer.get(), 0x2000, 0x2000);

    GenCmdList commands;
    CmdParse<FamilyType>::parseCommandBuffer(commands, ptrOffset(cmdContainer->getCommandStream()->getCpuBase(), 0), cmdContainer->getCommandStream()->getUsed());

    using MI_LOAD_REGISTER_REG = typename FamilyType::MI_LOAD_REGISTER_REG;
    auto itorLRI = find<MI_LOAD_REGISTER_REG *>(commands.begin(), commands.end());
    ASSERT_NE(itorLRI, commands.end());
    {
        auto cmd = genCmdCast<MI_LOAD_REGISTER_REG *>(*itorLRI);
        EXPECT_EQ(cmd->getDestinationRegisterAddress(), 0x2000u);
        EXPECT_EQ(cmd->getSourceRegisterAddress(), 0x2000u);
    }
}

using IsGen11OrBelow = IsAtMostGfxCore<IGFX_GEN11_CORE>;
HWTEST2_F(CommandSetMMIOTest, whenIsRemapApplicableCalledThenReturnFalse, IsGen11OrBelow) {
    EXPECT_FALSE(EncodeSetMMIO<FamilyType>::isRemapApplicable(0x1000));
    EXPECT_FALSE(EncodeSetMMIO<FamilyType>::isRemapApplicable(0x2000));
    EXPECT_FALSE(EncodeSetMMIO<FamilyType>::isRemapApplicable(0x2500));
    EXPECT_FALSE(EncodeSetMMIO<FamilyType>::isRemapApplicable(0x2600));
    EXPECT_FALSE(EncodeSetMMIO<FamilyType>::isRemapApplicable(0x4000));
}

using IsTgllpOrAbove = IsAtLeastProduct<IGFX_TIGERLAKE_LP>;
HWTEST2_F(CommandSetMMIOTest, givenRegisterWithinRemapRangeWhenEncodingLoadingMMIOThenRemapIsEnabled, IsTgllpOrAbove) {
    using MI_LOAD_REGISTER_MEM = typename FamilyType::MI_LOAD_REGISTER_MEM;

    uint32_t remapApplicableOffsets[] = {0x2000, 0x27ff,
                                         0x4200, 0x420f,
                                         0x4400, 0x441f};

    for (int i = 0; i < 3; i++) {
        for (uint32_t offset = remapApplicableOffsets[2 * i]; offset < remapApplicableOffsets[2 * i + 1]; offset += 32) {
            MI_LOAD_REGISTER_MEM *miLoadReg = reinterpret_cast<MI_LOAD_REGISTER_MEM *>(cmdContainer->getCommandStream()->getSpace(0));

            EncodeSetMMIO<FamilyType>::encodeMEM(*cmdContainer.get(), offset, 0xDEADBEEFCAF0);

            EXPECT_EQ(offset, miLoadReg->getRegisterAddress());
            EXPECT_EQ(0xDEADBEEFCAF0u, miLoadReg->getMemoryAddress());
            EXPECT_TRUE(miLoadReg->getMmioRemapEnable());
        }
    }

    {
        MI_LOAD_REGISTER_MEM *miLoadReg = reinterpret_cast<MI_LOAD_REGISTER_MEM *>(cmdContainer->getCommandStream()->getSpace(0));
        EncodeSetMMIO<FamilyType>::encodeMEM(*cmdContainer.get(), 0x3000, 0xDEADBEEFCAF0);

        EXPECT_EQ(0x3000u, miLoadReg->getRegisterAddress());
        EXPECT_EQ(0xDEADBEEFCAF0u, miLoadReg->getMemoryAddress());
        EXPECT_FALSE(miLoadReg->getMmioRemapEnable());
    }
    {
        MI_LOAD_REGISTER_MEM *miLoadReg = reinterpret_cast<MI_LOAD_REGISTER_MEM *>(cmdContainer->getCommandStream()->getSpace(0));
        EncodeSetMMIO<FamilyType>::encodeMEM(*cmdContainer.get(), 0x4300, 0xDEADBEEFCAF0);

        EXPECT_EQ(0x4300u, miLoadReg->getRegisterAddress());
        EXPECT_EQ(0xDEADBEEFCAF0u, miLoadReg->getMemoryAddress());
        EXPECT_FALSE(miLoadReg->getMmioRemapEnable());
    }
    {
        MI_LOAD_REGISTER_MEM *miLoadReg = reinterpret_cast<MI_LOAD_REGISTER_MEM *>(cmdContainer->getCommandStream()->getSpace(0));
        EncodeSetMMIO<FamilyType>::encodeMEM(*cmdContainer.get(), 0x5000, 0xDEADBEEFCAF0);

        EXPECT_EQ(0x5000u, miLoadReg->getRegisterAddress());
        EXPECT_EQ(0xDEADBEEFCAF0u, miLoadReg->getMemoryAddress());
        EXPECT_FALSE(miLoadReg->getMmioRemapEnable());
    }
}

HWTEST2_F(CommandSetMMIOTest, givenRegisterWithinRemapRangeWhenEncodingLoadingMMIOFromRegThenRemapIsEnabled, IsTgllpOrAbove) {
    using MI_LOAD_REGISTER_REG = typename FamilyType::MI_LOAD_REGISTER_REG;

    uint32_t remapApplicableOffsets[] = {0x2000, 0x27ff,
                                         0x4200, 0x420f,
                                         0x4400, 0x441f};

    for (int i = 0; i < 3; i++) {
        for (uint32_t offset = remapApplicableOffsets[2 * i]; offset < remapApplicableOffsets[2 * i + 1]; offset += 32) {
            MI_LOAD_REGISTER_REG *miLoadReg = reinterpret_cast<MI_LOAD_REGISTER_REG *>(cmdContainer->getCommandStream()->getSpace(0));
            EncodeSetMMIO<FamilyType>::encodeREG(*cmdContainer.get(), offset, offset);

            EXPECT_EQ(offset, miLoadReg->getSourceRegisterAddress());
            EXPECT_EQ(offset, miLoadReg->getDestinationRegisterAddress());
            EXPECT_TRUE(miLoadReg->getMmioRemapEnableSource());
            EXPECT_TRUE(miLoadReg->getMmioRemapEnableDestination());
        }
    }

    {
        MI_LOAD_REGISTER_REG *miLoadReg = reinterpret_cast<MI_LOAD_REGISTER_REG *>(cmdContainer->getCommandStream()->getSpace(0));
        EncodeSetMMIO<FamilyType>::encodeREG(*cmdContainer.get(), 0x1000, 0x2500);

        EXPECT_EQ(0x2500u, miLoadReg->getSourceRegisterAddress());
        EXPECT_EQ(0x1000u, miLoadReg->getDestinationRegisterAddress());
        EXPECT_TRUE(miLoadReg->getMmioRemapEnableSource());
        EXPECT_FALSE(miLoadReg->getMmioRemapEnableDestination());
    }
    {
        MI_LOAD_REGISTER_REG *miLoadReg = reinterpret_cast<MI_LOAD_REGISTER_REG *>(cmdContainer->getCommandStream()->getSpace(0));
        EncodeSetMMIO<FamilyType>::encodeREG(*cmdContainer.get(), 0x2200, 0x4000);

        EXPECT_EQ(0x4000u, miLoadReg->getSourceRegisterAddress());
        EXPECT_EQ(0x2200u, miLoadReg->getDestinationRegisterAddress());
        EXPECT_FALSE(miLoadReg->getMmioRemapEnableSource());
        EXPECT_TRUE(miLoadReg->getMmioRemapEnableDestination());
    }
}
