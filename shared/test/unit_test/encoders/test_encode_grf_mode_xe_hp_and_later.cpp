/*
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/stream_properties.h"
#include "shared/source/helpers/pipeline_select_helper.h"
#include "shared/source/helpers/ptr_math.h"
#include "shared/source/kernel/grf_config.h"
#include "shared/test/common/cmd_parse/gen_cmd_parse.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/libult/ult_command_stream_receiver.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/test_macros/hw_test.h"
#include "shared/test/unit_test/fixtures/command_container_fixture.h"

using namespace NEO;

using CommandEncodeStatesTest = Test<CommandEncodeStatesFixture>;

HWCMDTEST_F(IGFX_XE_HP_CORE, CommandEncodeStatesTest, givenCommandContainerWhenNumGrfRequiredIsDefaultThenLargeGrfModeDisabled) {
    using STATE_COMPUTE_MODE = typename FamilyType::STATE_COMPUTE_MODE;
    StreamProperties streamProperties{};
    auto &rootDeviceEnvironment = pDevice->getRootDeviceEnvironment();
    streamProperties.initSupport(rootDeviceEnvironment);
    streamProperties.stateComputeMode.setPropertiesAll(false, GrfConfig::defaultGrfNumber, 0u, PreemptionMode::Disabled, false);
    EncodeComputeMode<FamilyType>::programComputeModeCommand(*cmdContainer->getCommandStream(), streamProperties.stateComputeMode, rootDeviceEnvironment);

    GenCmdList commands;
    CmdParse<FamilyType>::parseCommandBuffer(commands, ptrOffset(cmdContainer->getCommandStream()->getCpuBase(), 0), cmdContainer->getCommandStream()->getUsed());

    auto itorCmd = find<STATE_COMPUTE_MODE *>(commands.begin(), commands.end());
    ASSERT_NE(itorCmd, commands.end());

    auto cmd = genCmdCast<STATE_COMPUTE_MODE *>(*itorCmd);

    EXPECT_FALSE(cmd->getLargeGrfMode());
}

HWTEST2_F(CommandEncodeStatesTest, givenCommandContainerWhenAdjustPipelineSelectCalledThenCommandHasGpgpuType, IsXeCore) {
    using PIPELINE_SELECT = typename FamilyType::PIPELINE_SELECT;
    EncodeComputeMode<FamilyType>::adjustPipelineSelect(*cmdContainer.get(), descriptor);
    GenCmdList commands;
    CmdParse<FamilyType>::parseCommandBuffer(commands, ptrOffset(cmdContainer->getCommandStream()->getCpuBase(), 0), cmdContainer->getCommandStream()->getUsed());

    auto itorCmd = find<PIPELINE_SELECT *>(commands.begin(), commands.end());
    ASSERT_NE(itorCmd, commands.end());

    auto cmd = genCmdCast<PIPELINE_SELECT *>(*itorCmd);
    EXPECT_EQ(cmd->getPipelineSelection(), PIPELINE_SELECT::PIPELINE_SELECTION_GPGPU);
}

HWTEST2_F(CommandEncodeStatesTest, givenCommandContainerWithKernelDpasThenSystolicModeEnabled, IsXeCore) {
    using PIPELINE_SELECT = typename FamilyType::PIPELINE_SELECT;
    descriptor.kernelAttributes.flags.usesSystolicPipelineSelectMode = true;
    EncodeComputeMode<FamilyType>::adjustPipelineSelect(*cmdContainer.get(), descriptor);
    GenCmdList commands;
    CmdParse<FamilyType>::parseCommandBuffer(commands, ptrOffset(cmdContainer->getCommandStream()->getCpuBase(), 0), cmdContainer->getCommandStream()->getUsed());

    auto itorCmd = find<PIPELINE_SELECT *>(commands.begin(), commands.end());

    ASSERT_NE(itorCmd, commands.end());
    auto cmd = genCmdCast<PIPELINE_SELECT *>(*itorCmd);

    EXPECT_EQ(pDevice->getUltCommandStreamReceiver<FamilyType>().pipelineSupportFlags.systolicMode, cmd->getSystolicModeEnable());
}

HWTEST2_F(CommandEncodeStatesTest, givenCommandContainerWithNoKernelDpasThenSystolicModeIsNotEnabled, IsXeCore) {
    using PIPELINE_SELECT = typename FamilyType::PIPELINE_SELECT;
    descriptor.kernelAttributes.flags.usesSystolicPipelineSelectMode = false;
    EncodeComputeMode<FamilyType>::adjustPipelineSelect(*cmdContainer.get(), descriptor);
    GenCmdList commands;
    CmdParse<FamilyType>::parseCommandBuffer(commands, ptrOffset(cmdContainer->getCommandStream()->getCpuBase(), 0), cmdContainer->getCommandStream()->getUsed());

    auto itorCmd = find<PIPELINE_SELECT *>(commands.begin(), commands.end());
    ASSERT_NE(itorCmd, commands.end());

    auto cmd = genCmdCast<PIPELINE_SELECT *>(*itorCmd);
    EXPECT_FALSE(cmd->getSystolicModeEnable());
}

HWTEST2_F(CommandEncodeStatesTest, givenDebugModeToOverrideSystolicModeToTrueWhenItIsSetThenPipelineSelectContainsProperBits, IsXeCore) {
    DebugManagerStateRestore restorer;
    debugManager.flags.OverrideSystolicPipelineSelect.set(1);

    using PIPELINE_SELECT = typename FamilyType::PIPELINE_SELECT;
    descriptor.kernelAttributes.flags.usesSystolicPipelineSelectMode = false;
    EncodeComputeMode<FamilyType>::adjustPipelineSelect(*cmdContainer.get(), descriptor);
    GenCmdList commands;
    CmdParse<FamilyType>::parseCommandBuffer(commands, ptrOffset(cmdContainer->getCommandStream()->getCpuBase(), 0), cmdContainer->getCommandStream()->getUsed());

    auto itorCmd = find<PIPELINE_SELECT *>(commands.begin(), commands.end());
    ASSERT_NE(itorCmd, commands.end());

    auto cmd = genCmdCast<PIPELINE_SELECT *>(*itorCmd);
    EXPECT_TRUE(cmd->getSystolicModeEnable());
}

HWTEST2_F(CommandEncodeStatesTest, givenDebugModeToOverrideSystolicModeToFalseWhenItIsSetThenPipelineSelectContainsProperBits, IsXeCore) {
    DebugManagerStateRestore restorer;
    debugManager.flags.OverrideSystolicPipelineSelect.set(0);

    using PIPELINE_SELECT = typename FamilyType::PIPELINE_SELECT;
    descriptor.kernelAttributes.flags.usesSystolicPipelineSelectMode = true;
    EncodeComputeMode<FamilyType>::adjustPipelineSelect(*cmdContainer.get(), descriptor);
    GenCmdList commands;
    CmdParse<FamilyType>::parseCommandBuffer(commands, ptrOffset(cmdContainer->getCommandStream()->getCpuBase(), 0), cmdContainer->getCommandStream()->getUsed());

    auto itorCmd = find<PIPELINE_SELECT *>(commands.begin(), commands.end());
    ASSERT_NE(itorCmd, commands.end());

    auto cmd = genCmdCast<PIPELINE_SELECT *>(*itorCmd);
    EXPECT_FALSE(cmd->getSystolicModeEnable());
}
