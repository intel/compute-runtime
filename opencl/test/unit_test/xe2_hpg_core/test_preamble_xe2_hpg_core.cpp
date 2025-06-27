/*
 * Copyright (C) 2024-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/stream_properties.h"
#include "shared/test/common/fixtures/preamble_fixture.h"

using namespace NEO;

using PreambleCfeState = PreambleFixture;
XE2_HPG_CORETEST_F(PreambleCfeState, givenXe2HpgCoreAndConcurrentKernelExecutionTypeWhenCallingProgramVFEStateThenSingleSpliceDispatchCcsModeIsEnabled) {
    using CFE_STATE = typename FamilyType::CFE_STATE;

    auto pVfeCmd = PreambleHelper<FamilyType>::getSpaceForVfeState(&linearStream, *defaultHwInfo, EngineGroupType::renderCompute);
    StreamProperties streamProperties{};
    streamProperties.initSupport(pDevice->getRootDeviceEnvironment());
    streamProperties.frontEndState.setPropertiesAll(true, false, false);
    PreambleHelper<FamilyType>::programVfeState(pVfeCmd, pDevice->getRootDeviceEnvironment(), 0u, 0, 0, streamProperties);
    parseCommands<FamilyType>(linearStream);
    auto cfeStateIt = find<CFE_STATE *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), cfeStateIt);
    auto cfeState = reinterpret_cast<CFE_STATE *>(*cfeStateIt);
    EXPECT_FALSE(cfeState->getComputeOverdispatchDisable());
    EXPECT_FALSE(cfeState->getSingleSliceDispatchCcsMode());
}

XE2_HPG_CORETEST_F(PreambleCfeState, givenXe2HpgCoreAndDefaultKernelExecutionTypeWhenCallingProgramVFEStateThenSingleSpliceDispatchCcsModeIsDisabled) {
    using CFE_STATE = typename FamilyType::CFE_STATE;

    auto pVfeCmd = PreambleHelper<FamilyType>::getSpaceForVfeState(&linearStream, *defaultHwInfo, EngineGroupType::renderCompute);
    StreamProperties streamProperties{};
    streamProperties.initSupport(pDevice->getRootDeviceEnvironment());
    streamProperties.frontEndState.setPropertiesAll(false, false, false);
    PreambleHelper<FamilyType>::programVfeState(pVfeCmd, pDevice->getRootDeviceEnvironment(), 0u, 0, 0, streamProperties);
    parseCommands<FamilyType>(linearStream);
    auto cfeStateIt = find<CFE_STATE *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), cfeStateIt);
    auto cfeState = reinterpret_cast<CFE_STATE *>(*cfeStateIt);
    EXPECT_FALSE(cfeState->getComputeOverdispatchDisable());
    EXPECT_FALSE(cfeState->getSingleSliceDispatchCcsMode());
}
