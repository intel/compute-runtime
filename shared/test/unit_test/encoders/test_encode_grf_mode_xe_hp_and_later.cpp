/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/stream_properties.h"
#include "shared/source/helpers/pipeline_select_helper.h"
#include "shared/source/helpers/ptr_math.h"
#include "shared/source/kernel/grf_config.h"
#include "shared/test/common/cmd_parse/gen_cmd_parse.h"
#include "shared/test/common/fixtures/command_container_fixture.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"

using namespace NEO;

using CommandEncodeStatesTest = Test<CommandEncodeStatesFixture>;

HWCMDTEST_F(IGFX_XE_HP_CORE, CommandEncodeStatesTest, givenCommandContainerWhenNumGrfRequiredIsDefaultThenLargeGrfModeDisabled) {
    using PIPELINE_SELECT = typename FamilyType::PIPELINE_SELECT;
    using STATE_COMPUTE_MODE = typename FamilyType::STATE_COMPUTE_MODE;
    StreamProperties streamProperties{};
    streamProperties.stateComputeMode.setProperties(false, GrfConfig::DefaultGrfNumber, 0u, PreemptionMode::Disabled, *defaultHwInfo);
    EncodeComputeMode<FamilyType>::programComputeModeCommand(*cmdContainer->getCommandStream(), streamProperties.stateComputeMode, *defaultHwInfo, nullptr);

    GenCmdList commands;
    CmdParse<FamilyType>::parseCommandBuffer(commands, ptrOffset(cmdContainer->getCommandStream()->getCpuBase(), 0), cmdContainer->getCommandStream()->getUsed());

    auto itorCmd = find<STATE_COMPUTE_MODE *>(commands.begin(), commands.end());
    ASSERT_NE(itorCmd, commands.end());

    auto cmd = genCmdCast<STATE_COMPUTE_MODE *>(*itorCmd);

    EXPECT_FALSE(cmd->getLargeGrfMode());
}

HWTEST2_F(CommandEncodeStatesTest, givenCommandContainerWhenAdjustPipelineSelectCalledThenCommandHasGpgpuType, IsWithinXeGfxFamily) {
    using PIPELINE_SELECT = typename FamilyType::PIPELINE_SELECT;
    EncodeComputeMode<FamilyType>::adjustPipelineSelect(*cmdContainer.get(), descriptor);
    GenCmdList commands;
    CmdParse<FamilyType>::parseCommandBuffer(commands, ptrOffset(cmdContainer->getCommandStream()->getCpuBase(), 0), cmdContainer->getCommandStream()->getUsed());

    auto itorCmd = find<PIPELINE_SELECT *>(commands.begin(), commands.end());
    ASSERT_NE(itorCmd, commands.end());

    auto cmd = genCmdCast<PIPELINE_SELECT *>(*itorCmd);
    EXPECT_EQ(cmd->getPipelineSelection(), PIPELINE_SELECT::PIPELINE_SELECTION_GPGPU);
}

HWTEST2_F(CommandEncodeStatesTest, givenLargeGrfModeProgrammedThenExpectedCommandSizeAdded, IsXeHpCore) {
    using GfxFamily = typename NEO::GfxFamilyMapper<gfxCoreFamily>::GfxFamily;
    using STATE_COMPUTE_MODE = typename FamilyType::STATE_COMPUTE_MODE;
    using PIPELINE_SELECT = typename FamilyType::PIPELINE_SELECT;

    auto usedSpaceBefore = cmdContainer->getCommandStream()->getUsed();

    NEO::EncodeComputeMode<GfxFamily>::adjustPipelineSelect(*cmdContainer, descriptor);
    StreamProperties streamProperties{};
    streamProperties.stateComputeMode.setProperties(false, 256u, 0u, PreemptionMode::Disabled, *defaultHwInfo);
    NEO::EncodeComputeMode<GfxFamily>::programComputeModeCommand(*cmdContainer->getCommandStream(), streamProperties.stateComputeMode, *defaultHwInfo, nullptr);

    auto usedSpaceAfter = cmdContainer->getCommandStream()->getUsed();
    ASSERT_GT(usedSpaceAfter, usedSpaceBefore);

    auto expectedCmdSize = sizeof(STATE_COMPUTE_MODE) + sizeof(PIPELINE_SELECT);
    auto cmdAddedSize = usedSpaceAfter - usedSpaceBefore;
    EXPECT_EQ(expectedCmdSize, cmdAddedSize);
}

HWTEST2_F(CommandEncodeStatesTest, givenLargeGrfModeDisabledThenExpectedCommandsAreAdded, IsXeHpCore) {
    using GfxFamily = typename NEO::GfxFamilyMapper<gfxCoreFamily>::GfxFamily;
    using STATE_COMPUTE_MODE = typename FamilyType::STATE_COMPUTE_MODE;
    using FORCE_NON_COHERENT = typename STATE_COMPUTE_MODE::FORCE_NON_COHERENT;
    using PIPELINE_SELECT = typename FamilyType::PIPELINE_SELECT;

    auto usedSpaceBefore = cmdContainer->getCommandStream()->getUsed();

    NEO::EncodeComputeMode<GfxFamily>::adjustPipelineSelect(*cmdContainer, descriptor);
    StreamProperties streamProperties{};
    streamProperties.stateComputeMode.largeGrfMode.set(0);
    NEO::EncodeComputeMode<GfxFamily>::programComputeModeCommand(*cmdContainer->getCommandStream(), streamProperties.stateComputeMode, *defaultHwInfo, nullptr);

    auto usedSpaceAfter = cmdContainer->getCommandStream()->getUsed();
    ASSERT_GT(usedSpaceAfter, usedSpaceBefore);

    auto expectedCmdSize = sizeof(STATE_COMPUTE_MODE) + sizeof(PIPELINE_SELECT);
    auto cmdAddedSize = usedSpaceAfter - usedSpaceBefore;
    EXPECT_EQ(expectedCmdSize, cmdAddedSize);

    auto expectedPsCmd = FamilyType::cmdInitPipelineSelect;
    expectedPsCmd.setSystolicModeEnable(false);
    expectedPsCmd.setMaskBits(NEO::pipelineSelectSystolicModeEnableMaskBits);
    expectedPsCmd.setPipelineSelection(PIPELINE_SELECT::PIPELINE_SELECTION_GPGPU);

    auto psCmd = reinterpret_cast<PIPELINE_SELECT *>(ptrOffset(cmdContainer->getCommandStream()->getCpuBase(), usedSpaceBefore));
    EXPECT_TRUE(memcmp(&expectedPsCmd, psCmd, sizeof(PIPELINE_SELECT)) == 0);

    auto expectedScmCmd = FamilyType::cmdInitStateComputeMode;
    expectedScmCmd.setLargeGrfMode(false);
    expectedScmCmd.setForceNonCoherent(FORCE_NON_COHERENT::FORCE_NON_COHERENT_FORCE_GPU_NON_COHERENT);
    expectedScmCmd.setMaskBits(FamilyType::stateComputeModeForceNonCoherentMask | FamilyType::stateComputeModeLargeGrfModeMask);

    auto scmCmd = reinterpret_cast<STATE_COMPUTE_MODE *>(ptrOffset(cmdContainer->getCommandStream()->getCpuBase(), (usedSpaceBefore + sizeof(PIPELINE_SELECT))));
    EXPECT_TRUE(memcmp(&expectedScmCmd, scmCmd, sizeof(STATE_COMPUTE_MODE)) == 0);
}

HWTEST2_F(CommandEncodeStatesTest, givenCommandContainerWithKernelDpasThenSystolicModeEnabled, IsWithinXeGfxFamily) {
    using PIPELINE_SELECT = typename FamilyType::PIPELINE_SELECT;
    descriptor.kernelAttributes.flags.usesSpecialPipelineSelectMode = true;
    EncodeComputeMode<FamilyType>::adjustPipelineSelect(*cmdContainer.get(), descriptor);
    GenCmdList commands;
    CmdParse<FamilyType>::parseCommandBuffer(commands, ptrOffset(cmdContainer->getCommandStream()->getCpuBase(), 0), cmdContainer->getCommandStream()->getUsed());

    auto itorCmd = find<PIPELINE_SELECT *>(commands.begin(), commands.end());
    ASSERT_NE(itorCmd, commands.end());

    auto cmd = genCmdCast<PIPELINE_SELECT *>(*itorCmd);
    EXPECT_TRUE(cmd->getSystolicModeEnable());
}

HWTEST2_F(CommandEncodeStatesTest, givenCommandContainerWithNoKernelDpasThenSystolicModeIsNotEnabled, IsWithinXeGfxFamily) {
    using PIPELINE_SELECT = typename FamilyType::PIPELINE_SELECT;
    descriptor.kernelAttributes.flags.usesSpecialPipelineSelectMode = false;
    EncodeComputeMode<FamilyType>::adjustPipelineSelect(*cmdContainer.get(), descriptor);
    GenCmdList commands;
    CmdParse<FamilyType>::parseCommandBuffer(commands, ptrOffset(cmdContainer->getCommandStream()->getCpuBase(), 0), cmdContainer->getCommandStream()->getUsed());

    auto itorCmd = find<PIPELINE_SELECT *>(commands.begin(), commands.end());
    ASSERT_NE(itorCmd, commands.end());

    auto cmd = genCmdCast<PIPELINE_SELECT *>(*itorCmd);
    EXPECT_FALSE(cmd->getSystolicModeEnable());
}

HWTEST2_F(CommandEncodeStatesTest, givenDebugModeToOverrideSystolicModeToTrueWhenItIsSetThenPipelineSelectContainsProperBits, IsWithinXeGfxFamily) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.OverrideSystolicPipelineSelect.set(1);

    using PIPELINE_SELECT = typename FamilyType::PIPELINE_SELECT;
    descriptor.kernelAttributes.flags.usesSpecialPipelineSelectMode = false;
    EncodeComputeMode<FamilyType>::adjustPipelineSelect(*cmdContainer.get(), descriptor);
    GenCmdList commands;
    CmdParse<FamilyType>::parseCommandBuffer(commands, ptrOffset(cmdContainer->getCommandStream()->getCpuBase(), 0), cmdContainer->getCommandStream()->getUsed());

    auto itorCmd = find<PIPELINE_SELECT *>(commands.begin(), commands.end());
    ASSERT_NE(itorCmd, commands.end());

    auto cmd = genCmdCast<PIPELINE_SELECT *>(*itorCmd);
    EXPECT_TRUE(cmd->getSystolicModeEnable());
}

HWTEST2_F(CommandEncodeStatesTest, givenDebugModeToOverrideSystolicModeToFalseWhenItIsSetThenPipelineSelectContainsProperBits, IsWithinXeGfxFamily) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.OverrideSystolicPipelineSelect.set(0);

    using PIPELINE_SELECT = typename FamilyType::PIPELINE_SELECT;
    descriptor.kernelAttributes.flags.usesSpecialPipelineSelectMode = true;
    EncodeComputeMode<FamilyType>::adjustPipelineSelect(*cmdContainer.get(), descriptor);
    GenCmdList commands;
    CmdParse<FamilyType>::parseCommandBuffer(commands, ptrOffset(cmdContainer->getCommandStream()->getCpuBase(), 0), cmdContainer->getCommandStream()->getUsed());

    auto itorCmd = find<PIPELINE_SELECT *>(commands.begin(), commands.end());
    ASSERT_NE(itorCmd, commands.end());

    auto cmd = genCmdCast<PIPELINE_SELECT *>(*itorCmd);
    EXPECT_FALSE(cmd->getSystolicModeEnable());
}

HWTEST2_F(CommandEncodeStatesTest, givenLargeGrfModeEnabledThenExpectedCommandsAreAdded, IsXeHpCore) {
    using GfxFamily = typename NEO::GfxFamilyMapper<gfxCoreFamily>::GfxFamily;
    using STATE_COMPUTE_MODE = typename FamilyType::STATE_COMPUTE_MODE;
    using FORCE_NON_COHERENT = typename STATE_COMPUTE_MODE::FORCE_NON_COHERENT;
    using PIPELINE_SELECT = typename FamilyType::PIPELINE_SELECT;

    auto usedSpaceBefore = cmdContainer->getCommandStream()->getUsed();

    descriptor.kernelAttributes.flags.usesSpecialPipelineSelectMode = true;
    NEO::EncodeComputeMode<GfxFamily>::adjustPipelineSelect(*cmdContainer, descriptor);
    StreamProperties streamProperties{};
    streamProperties.stateComputeMode.largeGrfMode.set(1);
    NEO::EncodeComputeMode<GfxFamily>::programComputeModeCommand(*cmdContainer->getCommandStream(), streamProperties.stateComputeMode, *defaultHwInfo, nullptr);

    auto usedSpaceAfter = cmdContainer->getCommandStream()->getUsed();
    ASSERT_GT(usedSpaceAfter, usedSpaceBefore);

    auto expectedCmdSize = sizeof(STATE_COMPUTE_MODE) + sizeof(PIPELINE_SELECT);
    auto cmdAddedSize = usedSpaceAfter - usedSpaceBefore;
    EXPECT_EQ(expectedCmdSize, cmdAddedSize);

    auto expectedPsCmd = FamilyType::cmdInitPipelineSelect;
    expectedPsCmd.setSystolicModeEnable(true);
    expectedPsCmd.setMaskBits(pipelineSelectSystolicModeEnableMaskBits);
    expectedPsCmd.setPipelineSelection(PIPELINE_SELECT::PIPELINE_SELECTION_GPGPU);

    auto psCmd = reinterpret_cast<PIPELINE_SELECT *>(ptrOffset(cmdContainer->getCommandStream()->getCpuBase(), usedSpaceBefore));
    EXPECT_TRUE(memcmp(&expectedPsCmd, psCmd, sizeof(PIPELINE_SELECT)) == 0);

    auto expectedScmCmd = FamilyType::cmdInitStateComputeMode;
    expectedScmCmd.setLargeGrfMode(true);
    expectedScmCmd.setForceNonCoherent(FORCE_NON_COHERENT::FORCE_NON_COHERENT_FORCE_GPU_NON_COHERENT);
    expectedScmCmd.setMaskBits(FamilyType::stateComputeModeForceNonCoherentMask | FamilyType::stateComputeModeLargeGrfModeMask);

    auto scmCmd = reinterpret_cast<STATE_COMPUTE_MODE *>(ptrOffset(cmdContainer->getCommandStream()->getCpuBase(), (usedSpaceBefore + sizeof(PIPELINE_SELECT))));
    EXPECT_TRUE(memcmp(&expectedScmCmd, scmCmd, sizeof(STATE_COMPUTE_MODE)) == 0);
}

HWTEST2_F(CommandEncodeStatesTest, givenLargeGrfModeEnabledAndDisabledThenExpectedCommandsAreAdded, IsXeHpCore) {
    using GfxFamily = typename NEO::GfxFamilyMapper<gfxCoreFamily>::GfxFamily;
    using STATE_COMPUTE_MODE = typename FamilyType::STATE_COMPUTE_MODE;
    using FORCE_NON_COHERENT = typename STATE_COMPUTE_MODE::FORCE_NON_COHERENT;
    using PIPELINE_SELECT = typename FamilyType::PIPELINE_SELECT;

    auto usedSpaceBefore = cmdContainer->getCommandStream()->getUsed();

    descriptor.kernelAttributes.flags.usesSpecialPipelineSelectMode = true;
    NEO::EncodeComputeMode<GfxFamily>::adjustPipelineSelect(*cmdContainer, descriptor);
    StreamProperties streamProperties{};
    streamProperties.stateComputeMode.largeGrfMode.set(1);
    NEO::EncodeComputeMode<GfxFamily>::programComputeModeCommand(*cmdContainer->getCommandStream(), streamProperties.stateComputeMode, *defaultHwInfo, nullptr);

    auto usedSpaceAfter = cmdContainer->getCommandStream()->getUsed();
    ASSERT_GT(usedSpaceAfter, usedSpaceBefore);

    auto expectedCmdSize = sizeof(STATE_COMPUTE_MODE) + sizeof(PIPELINE_SELECT);
    auto cmdAddedSize = usedSpaceAfter - usedSpaceBefore;
    EXPECT_EQ(expectedCmdSize, cmdAddedSize);

    auto expectedPsCmd = FamilyType::cmdInitPipelineSelect;
    expectedPsCmd.setSystolicModeEnable(true);
    expectedPsCmd.setMaskBits(NEO::pipelineSelectSystolicModeEnableMaskBits);
    expectedPsCmd.setPipelineSelection(PIPELINE_SELECT::PIPELINE_SELECTION_GPGPU);

    auto psCmd = reinterpret_cast<PIPELINE_SELECT *>(ptrOffset(cmdContainer->getCommandStream()->getCpuBase(), usedSpaceBefore));
    EXPECT_TRUE(memcmp(&expectedPsCmd, psCmd, sizeof(PIPELINE_SELECT)) == 0);

    auto expectedScmCmd = FamilyType::cmdInitStateComputeMode;
    expectedScmCmd.setLargeGrfMode(true);
    expectedScmCmd.setForceNonCoherent(FORCE_NON_COHERENT::FORCE_NON_COHERENT_FORCE_GPU_NON_COHERENT);
    expectedScmCmd.setMaskBits(FamilyType::stateComputeModeForceNonCoherentMask | FamilyType::stateComputeModeLargeGrfModeMask);

    auto scmCmd = reinterpret_cast<STATE_COMPUTE_MODE *>(ptrOffset(cmdContainer->getCommandStream()->getCpuBase(), (usedSpaceBefore + sizeof(PIPELINE_SELECT))));
    EXPECT_TRUE(memcmp(&expectedScmCmd, scmCmd, sizeof(STATE_COMPUTE_MODE)) == 0);

    // Disable Large GRF Mode
    usedSpaceBefore = cmdContainer->getCommandStream()->getUsed();
    NEO::EncodeComputeMode<GfxFamily>::adjustPipelineSelect(*cmdContainer, descriptor);
    streamProperties.stateComputeMode.largeGrfMode.set(0);
    NEO::EncodeComputeMode<GfxFamily>::programComputeModeCommand(*cmdContainer->getCommandStream(), streamProperties.stateComputeMode, *defaultHwInfo, nullptr);

    usedSpaceAfter = cmdContainer->getCommandStream()->getUsed();
    ASSERT_GT(usedSpaceAfter, usedSpaceBefore);

    expectedCmdSize = sizeof(STATE_COMPUTE_MODE) + sizeof(PIPELINE_SELECT);
    cmdAddedSize = usedSpaceAfter - usedSpaceBefore;
    EXPECT_EQ(expectedCmdSize, cmdAddedSize);

    expectedPsCmd = FamilyType::cmdInitPipelineSelect;
    expectedPsCmd.setSystolicModeEnable(true);
    expectedPsCmd.setMaskBits(NEO::pipelineSelectSystolicModeEnableMaskBits);
    expectedPsCmd.setPipelineSelection(PIPELINE_SELECT::PIPELINE_SELECTION_GPGPU);

    psCmd = reinterpret_cast<PIPELINE_SELECT *>(ptrOffset(cmdContainer->getCommandStream()->getCpuBase(), usedSpaceBefore));
    EXPECT_TRUE(memcmp(&expectedPsCmd, psCmd, sizeof(PIPELINE_SELECT)) == 0);

    expectedScmCmd = FamilyType::cmdInitStateComputeMode;
    expectedScmCmd.setLargeGrfMode(false);
    expectedScmCmd.setForceNonCoherent(FORCE_NON_COHERENT::FORCE_NON_COHERENT_FORCE_GPU_NON_COHERENT);
    expectedScmCmd.setMaskBits(FamilyType::stateComputeModeForceNonCoherentMask | FamilyType::stateComputeModeLargeGrfModeMask);

    scmCmd = reinterpret_cast<STATE_COMPUTE_MODE *>(ptrOffset(cmdContainer->getCommandStream()->getCpuBase(), (usedSpaceBefore + sizeof(PIPELINE_SELECT))));
    EXPECT_TRUE(memcmp(&expectedScmCmd, scmCmd, sizeof(STATE_COMPUTE_MODE)) == 0);
}
