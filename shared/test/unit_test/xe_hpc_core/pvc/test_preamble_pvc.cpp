/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/stream_properties.h"
#include "shared/source/helpers/hw_helper.h"
#include "shared/source/os_interface/device_factory.h"
#include "shared/source/os_interface/hw_info_config.h"
#include "shared/test/common/fixtures/preamble_fixture.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"

using namespace NEO;

using PreambleCfeState = PreambleFixture;

PVCTEST_F(PreambleCfeState, givenXeHpcAndKernelExecutionTypeAndRevisionWhenCallingProgramVFEStateThenCFEStateParamsAreCorrectlySet) {
    using CFE_STATE = typename FamilyType::CFE_STATE;
    auto hwInfo = pDevice->getRootDeviceEnvironment().getMutableHardwareInfo();

    const auto &productHelper = pDevice->getProductHelper();
    auto pVfeCmd = PreambleHelper<FamilyType>::getSpaceForVfeState(&linearStream, *hwInfo, EngineGroupType::RenderCompute);
    std::array<std::pair<uint32_t, bool>, 4> revisions = {
        {{REVISION_A0, false},
         {REVISION_A0, true},
         {REVISION_B, false},
         {REVISION_B, true}}};

    for (const auto &[revision, kernelExecutionType] : revisions) {
        StreamProperties streamProperties{};
        hwInfo->platform.usRevId = productHelper.getHwRevIdFromStepping(revision, *hwInfo);
        streamProperties.frontEndState.setProperties(kernelExecutionType, false, false, false, *hwInfo);

        PreambleHelper<FamilyType>::programVfeState(pVfeCmd, pDevice->getRootDeviceEnvironment(), 0u, 0, 0, streamProperties, nullptr);
        parseCommands<FamilyType>(linearStream);
        auto cfeStateIt = find<CFE_STATE *>(cmdList.begin(), cmdList.end());
        ASSERT_NE(cmdList.end(), cfeStateIt);
        auto cfeState = reinterpret_cast<CFE_STATE *>(*cfeStateIt);

        auto expectedValue = (revision >= REVISION_B) && kernelExecutionType;
        EXPECT_EQ(expectedValue, cfeState->getComputeDispatchAllWalkerEnable());
        EXPECT_FALSE(cfeState->getSingleSliceDispatchCcsMode());
        EXPECT_FALSE(cfeState->getComputeOverdispatchDisable());
    }
}

using PreamblePipelineSelectState = PreambleFixture;
PVCTEST_F(PreamblePipelineSelectState, givenRevisionBAndAboveWhenCallingProgramPipelineSelectThenSystolicModeDisabled) {
    using PIPELINE_SELECT = typename FamilyType::PIPELINE_SELECT;
    auto hwInfo = pDevice->getRootDeviceEnvironment().getMutableHardwareInfo();

    PipelineSelectArgs pipelineArgs;
    pipelineArgs.systolicPipelineSelectMode = true;

    struct {
        unsigned short revId;
        bool expectedValue;
    } testInputs[] = {
        {0x0, true},
        {0x1, true},
        {0x3, true},
        {0x5, false},
        {0x6, false},
        {0x7, false},
    };
    auto &productHelper = *ProductHelper::get(hwInfo->platform.eProductFamily);
    for (auto &testInput : testInputs) {
        LinearStream linearStream(&gfxAllocation);
        hwInfo->platform.usRevId = testInput.revId;
        pipelineArgs.systolicPipelineSelectSupport = productHelper.isSystolicModeConfigurable(*hwInfo);

        PreambleHelper<FamilyType>::programPipelineSelect(&linearStream, pipelineArgs, *hwInfo);
        parseCommands<FamilyType>(linearStream);

        auto itorCmd = find<PIPELINE_SELECT *>(cmdList.begin(), cmdList.end());
        ASSERT_NE(itorCmd, cmdList.end());

        auto cmd = genCmdCast<PIPELINE_SELECT *>(*itorCmd);
        EXPECT_EQ(testInput.expectedValue, cmd->getSystolicModeEnable());
    }
}
