/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gen9/reg_configs.h"
#include "shared/test/common/cmd_parse/gen_cmd_parse.h"
#include "shared/test/common/test_macros/test.h"

#include "level_zero/core/source/gen9/cmdlist_gen9.h"
#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdlist.h"

namespace L0 {
namespace ult {

using CommandListProgramL3 = Test<DeviceFixture>;

template <PRODUCT_FAMILY productFamily>
struct CommandListAdjustStateComputeMode : public WhiteBox<::L0::CommandListProductFamily<productFamily>> {
    CommandListAdjustStateComputeMode() : WhiteBox<::L0::CommandListProductFamily<productFamily>>(1) {}
};

HWTEST2_F(CommandListProgramL3, givenAllocationsWhenProgramL3ThenMmioIsAppended, IsGen9) {
    using GfxFamily = typename NEO::GfxFamilyMapper<gfxCoreFamily>::GfxFamily;
    using MI_LOAD_REGISTER_IMM = typename GfxFamily::MI_LOAD_REGISTER_IMM;
    const uint32_t registerOffset = NEO::L3CNTLRegisterOffset<GfxFamily>::registerOffset;

    auto commandList = new CommandListAdjustStateComputeMode<productFamily>();
    bool ret = commandList->initialize(device, NEO::EngineGroupType::RenderCompute, 0u);
    ASSERT_FALSE(ret);

    commandList->programL3(false);
    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList, ptrOffset(commandList->commandContainer.getCommandStream()->getCpuBase(), 0), commandList->commandContainer.getCommandStream()->getUsed()));
    bool found = false;
    auto miLoadImm = findAll<MI_LOAD_REGISTER_IMM *>(cmdList.begin(), cmdList.end());
    for (auto itor : miLoadImm) {
        auto cmd = genCmdCast<MI_LOAD_REGISTER_IMM *>(*itor);
        if (registerOffset == cmd->getRegisterOffset()) {
            found = true;
            break;
        }
    }
    EXPECT_TRUE(found);

    commandList->destroy();
}

HWTEST2_F(CommandListProgramL3, givenAllocationsWhenProgramL3WithSlmThenMmioIsAppendedWithSlm, IsGen9) {
    using GfxFamily = typename NEO::GfxFamilyMapper<gfxCoreFamily>::GfxFamily;
    using MI_LOAD_REGISTER_IMM = typename GfxFamily::MI_LOAD_REGISTER_IMM;
    const uint32_t registerOffset = NEO::L3CNTLRegisterOffset<GfxFamily>::registerOffset;
    auto hwInfo = device->getNEODevice()->getHardwareInfo();
    const uint32_t valueForSLM = NEO::PreambleHelper<GfxFamily>::getL3Config(hwInfo, true);

    auto commandList = new CommandListAdjustStateComputeMode<productFamily>();
    bool ret = commandList->initialize(device, NEO::EngineGroupType::RenderCompute, 0u);
    ASSERT_FALSE(ret);

    commandList->programL3(true);
    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList, ptrOffset(commandList->commandContainer.getCommandStream()->getCpuBase(), 0), commandList->commandContainer.getCommandStream()->getUsed()));
    bool found = false;
    auto miLoadImm = findAll<MI_LOAD_REGISTER_IMM *>(cmdList.begin(), cmdList.end());
    for (auto itor : miLoadImm) {
        auto cmd = genCmdCast<MI_LOAD_REGISTER_IMM *>(*itor);
        if (registerOffset == cmd->getRegisterOffset()) {
            EXPECT_EQ(cmd->getRegisterOffset(), registerOffset);
            EXPECT_EQ(cmd->getDataDword(), valueForSLM);
            found = true;
            break;
        }
    }
    EXPECT_TRUE(found);

    commandList->destroy();
}

HWTEST2_F(CommandListProgramL3, givenAllocationsWhenProgramL3WithoutSlmThenMmioIsAppendedWithoutSlm, IsGen9) {
    using GfxFamily = typename NEO::GfxFamilyMapper<gfxCoreFamily>::GfxFamily;
    using MI_LOAD_REGISTER_IMM = typename GfxFamily::MI_LOAD_REGISTER_IMM;
    const uint32_t registerOffset = NEO::L3CNTLRegisterOffset<GfxFamily>::registerOffset;
    const uint32_t valueForNoSLM = NEO::PreambleHelper<GfxFamily>::getL3Config(*defaultHwInfo, false);

    auto commandList = new CommandListAdjustStateComputeMode<productFamily>();
    bool ret = commandList->initialize(device, NEO::EngineGroupType::RenderCompute, 0u);
    ASSERT_FALSE(ret);

    commandList->programL3(false);
    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList, ptrOffset(commandList->commandContainer.getCommandStream()->getCpuBase(), 0), commandList->commandContainer.getCommandStream()->getUsed()));
    bool found = false;
    auto miLoadImm = findAll<MI_LOAD_REGISTER_IMM *>(cmdList.begin(), cmdList.end());
    for (auto itor : miLoadImm) {
        auto cmd = genCmdCast<MI_LOAD_REGISTER_IMM *>(*itor);
        if (registerOffset == cmd->getRegisterOffset()) {
            EXPECT_EQ(cmd->getRegisterOffset(), registerOffset);
            EXPECT_EQ(cmd->getDataDword(), valueForNoSLM);
            found = true;
            break;
        }
    }
    EXPECT_TRUE(found);

    commandList->destroy();
}

} // namespace ult
} // namespace L0
