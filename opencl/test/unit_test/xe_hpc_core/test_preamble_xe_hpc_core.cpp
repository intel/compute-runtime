/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/stream_properties.h"
#include "shared/source/helpers/hw_helper.h"
#include "shared/source/os_interface/device_factory.h"
#include "shared/source/os_interface/hw_info_config.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/unit_test/preamble/preamble_fixture.h"

using namespace NEO;

using PreambleCfeState = PreambleFixture;
XE_HPC_CORETEST_F(PreambleCfeState, givenPvcAndKernelExecutionTypeAndRevisionWhenCallingProgramVFEStateThenCFEStateParamsAreCorrectlySet) {
    using CFE_STATE = typename FamilyType::CFE_STATE;
    auto hwInfo = pDevice->getRootDeviceEnvironment().getMutableHardwareInfo();
    const auto &hwInfoConfig = *NEO::HwInfoConfig::get(hwInfo->platform.eProductFamily);
    auto pVfeCmd = PreambleHelper<FamilyType>::getSpaceForVfeState(&linearStream, *hwInfo, EngineGroupType::RenderCompute);
    StreamProperties streamProperties{};
    std::array<std::pair<uint32_t, bool>, 4> revisions = {
        {{REVISION_A0, false},
         {REVISION_A0, true},
         {REVISION_B, false},
         {REVISION_B, true}}};

    for (const auto &[revision, kernelExecutionType] : revisions) {
        hwInfo->platform.usRevId = hwInfoConfig.getHwRevIdFromStepping(revision, *hwInfo);
        streamProperties.frontEndState.setProperties(kernelExecutionType, false, false, *hwInfo);

        PreambleHelper<FamilyType>::programVfeState(pVfeCmd, *hwInfo, 0u, 0, 0, streamProperties);
        parseCommands<FamilyType>(linearStream);
        auto cfeStateIt = find<CFE_STATE *>(cmdList.begin(), cmdList.end());
        ASSERT_NE(cmdList.end(), cfeStateIt);
        auto cfeState = reinterpret_cast<CFE_STATE *>(*cfeStateIt);

        auto expectedValue = (HwInfoConfig::get(hwInfo->platform.eProductFamily)->getSteppingFromHwRevId(*hwInfo) >= REVISION_B) &&
                             (!XE_HPC_CORE::isXtTemporary(*defaultHwInfo)) &&
                             kernelExecutionType;
        EXPECT_EQ(expectedValue, cfeState->getComputeDispatchAllWalkerEnable());
        EXPECT_EQ(expectedValue, cfeState->getSingleSliceDispatchCcsMode());
        EXPECT_FALSE(cfeState->getComputeOverdispatchDisable());
    }
}

XE_HPC_CORETEST_F(PreambleCfeState, givenPvcXtTemporaryAndKernelExecutionTypeConcurrentAndRevisionBWhenCallingProgramVFEStateThenAllWalkerIsDisabled) {
    using CFE_STATE = typename FamilyType::CFE_STATE;

    auto hwInfo = *defaultHwInfo;
    hwInfo.platform.usDeviceID = 0x0BE5;
    const auto &hwInfoConfig = *NEO::HwInfoConfig::get(hwInfo.platform.eProductFamily);
    hwInfo.platform.usRevId = hwInfoConfig.getHwRevIdFromStepping(REVISION_B, hwInfo);

    auto pVfeCmd = PreambleHelper<FamilyType>::getSpaceForVfeState(&linearStream, hwInfo, EngineGroupType::RenderCompute);
    StreamProperties streamProperties{};
    streamProperties.frontEndState.setProperties(true, false, false, hwInfo);

    PreambleHelper<FamilyType>::programVfeState(pVfeCmd, hwInfo, 0u, 0, 0, streamProperties);
    parseCommands<FamilyType>(linearStream);
    auto cfeStateIt = find<CFE_STATE *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), cfeStateIt);
    auto cfeState = reinterpret_cast<CFE_STATE *>(*cfeStateIt);

    EXPECT_FALSE(cfeState->getComputeDispatchAllWalkerEnable());
    EXPECT_FALSE(cfeState->getSingleSliceDispatchCcsMode());
}

XE_HPC_CORETEST_F(PreambleCfeState, givenXeHpcCoreAndSetDebugFlagWhenPreambleCfeStateIsProgrammedThenCFEStateParamsHaveSetValue) {
    using CFE_STATE = typename FamilyType::CFE_STATE;
    DebugManagerStateRestore dbgRestore;
    uint32_t expectedValue = 1u;

    DebugManager.flags.CFEComputeDispatchAllWalkerEnable.set(expectedValue);

    uint64_t expectedAddress = 1 << CFE_STATE::SCRATCHSPACEBUFFER_BIT_SHIFT;
    auto pVfeCmd = PreambleHelper<FamilyType>::getSpaceForVfeState(&linearStream, *defaultHwInfo, EngineGroupType::RenderCompute);
    StreamProperties emptyProperties{};
    PreambleHelper<FamilyType>::programVfeState(pVfeCmd, *defaultHwInfo, 0u, expectedAddress, 16u, emptyProperties);

    parseCommands<FamilyType>(linearStream);
    auto cfeStateIt = find<CFE_STATE *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), cfeStateIt);

    auto cfeState = reinterpret_cast<CFE_STATE *>(*cfeStateIt);

    EXPECT_EQ(expectedValue, cfeState->getComputeDispatchAllWalkerEnable());
    EXPECT_FALSE(cfeState->getSingleSliceDispatchCcsMode());
}

XE_HPC_CORETEST_F(PreambleCfeState, givenNotSetDebugFlagWhenPreambleCfeStateIsProgrammedThenCFEStateParamsHaveNotSetValue) {
    using CFE_STATE = typename FamilyType::CFE_STATE;

    auto cfeState = reinterpret_cast<CFE_STATE *>(linearStream.getSpace(sizeof(CFE_STATE)));
    *cfeState = FamilyType::cmdInitCfeState;

    uint32_t numberOfWalkers = cfeState->getNumberOfWalkers();
    uint32_t overDispatchControl = static_cast<uint32_t>(cfeState->getOverDispatchControl());
    uint32_t singleDispatchCcsMode = cfeState->getSingleSliceDispatchCcsMode();

    uint64_t expectedAddress = 1 << CFE_STATE::SCRATCHSPACEBUFFER_BIT_SHIFT;
    uint32_t expectedMaxThreads = HwHelper::getMaxThreadsForVfe(*defaultHwInfo);
    auto pVfeCmd = PreambleHelper<FamilyType>::getSpaceForVfeState(&linearStream, *defaultHwInfo, EngineGroupType::RenderCompute);
    StreamProperties emptyProperties{};
    PreambleHelper<FamilyType>::programVfeState(pVfeCmd, *defaultHwInfo, 0u, expectedAddress, expectedMaxThreads, emptyProperties);
    uint32_t maximumNumberOfThreads = cfeState->getMaximumNumberOfThreads();

    EXPECT_EQ(numberOfWalkers, cfeState->getNumberOfWalkers());
    EXPECT_NE(expectedMaxThreads, maximumNumberOfThreads);
    EXPECT_EQ(overDispatchControl, static_cast<uint32_t>(cfeState->getOverDispatchControl()));
    EXPECT_EQ(singleDispatchCcsMode, cfeState->getSingleSliceDispatchCcsMode());
}

XE_HPC_CORETEST_F(PreambleCfeState, givenSetDebugFlagWhenPreambleCfeStateIsProgrammedThenCFEStateParamsHaveSetValue) {
    using CFE_STATE = typename FamilyType::CFE_STATE;

    uint32_t expectedValue1 = 1u;
    uint32_t expectedValue2 = 2u;

    DebugManagerStateRestore dbgRestore;

    DebugManager.flags.CFEFusedEUDispatch.set(expectedValue1);
    DebugManager.flags.CFEOverDispatchControl.set(expectedValue1);
    DebugManager.flags.CFESingleSliceDispatchCCSMode.set(expectedValue1);
    DebugManager.flags.CFELargeGRFThreadAdjustDisable.set(expectedValue1);
    DebugManager.flags.CFENumberOfWalkers.set(expectedValue2);
    DebugManager.flags.CFEMaximumNumberOfThreads.set(expectedValue2);

    uint64_t expectedAddress = 1 << CFE_STATE::SCRATCHSPACEBUFFER_BIT_SHIFT;
    auto pVfeCmd = PreambleHelper<FamilyType>::getSpaceForVfeState(&linearStream, *defaultHwInfo, EngineGroupType::RenderCompute);
    StreamProperties emptyProperties{};
    PreambleHelper<FamilyType>::programVfeState(pVfeCmd, *defaultHwInfo, 0u, expectedAddress, 16u, emptyProperties);

    parseCommands<FamilyType>(linearStream);
    auto cfeStateIt = find<CFE_STATE *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), cfeStateIt);

    auto cfeState = reinterpret_cast<CFE_STATE *>(*cfeStateIt);

    EXPECT_EQ(expectedValue1, cfeState->getSingleSliceDispatchCcsMode());
    EXPECT_EQ(expectedValue1, static_cast<uint32_t>(cfeState->getOverDispatchControl()));
    EXPECT_EQ(expectedValue1, cfeState->getLargeGRFThreadAdjustDisable());
    EXPECT_EQ(expectedValue2, cfeState->getNumberOfWalkers());
    EXPECT_EQ(expectedValue2, cfeState->getMaximumNumberOfThreads());
}

using PreamblePipelineSelectState = PreambleFixture;
XE_HPC_CORETEST_F(PreamblePipelineSelectState, givenRevisionBAndAboveWhenCallingProgramPipelineSelectThenSystolicModeDisabled) {
    using PIPELINE_SELECT = typename FamilyType::PIPELINE_SELECT;
    auto hwInfo = pDevice->getRootDeviceEnvironment().getMutableHardwareInfo();

    PipelineSelectArgs pipelineArgs;
    pipelineArgs.specialPipelineSelectMode = true;

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
    for (auto &testInput : testInputs) {
        LinearStream linearStream(&gfxAllocation);
        hwInfo->platform.usRevId = testInput.revId;
        PreambleHelper<FamilyType>::programPipelineSelect(&linearStream, pipelineArgs, *hwInfo);
        parseCommands<FamilyType>(linearStream);

        auto itorCmd = find<PIPELINE_SELECT *>(cmdList.begin(), cmdList.end());
        ASSERT_NE(itorCmd, cmdList.end());

        auto cmd = genCmdCast<PIPELINE_SELECT *>(*itorCmd);
        EXPECT_EQ(testInput.expectedValue, cmd->getSystolicModeEnable());
    }
}
