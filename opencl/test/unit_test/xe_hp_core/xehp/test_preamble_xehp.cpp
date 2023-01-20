/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/stream_properties.h"
#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/test/common/fixtures/preamble_fixture.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/test_macros/test.h"

#include "opencl/source/helpers/hardware_commands_helper.h"

using namespace NEO;

using PreambleCfeState = PreambleFixture;
HWTEST2_F(PreambleCfeState, givenXehpAndFlagCFEWeightedDispatchModeDisableSetFalseWhenCallingProgramVFEStateThenFieldWeightedDispatchModeDisableAreNotSet, IsXEHP) {
    using CFE_STATE = typename FamilyType::CFE_STATE;
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.CFEWeightedDispatchModeDisable.set(false);

    auto pVfeCmd = PreambleHelper<FamilyType>::getSpaceForVfeState(&linearStream, *defaultHwInfo, EngineGroupType::RenderCompute);
    StreamProperties streamProperties{};
    streamProperties.frontEndState.setProperties(false, false, false, false, *defaultHwInfo);
    PreambleHelper<FamilyType>::programVfeState(pVfeCmd, *defaultHwInfo, 0u, 0, 0, streamProperties, nullptr);
    parseCommands<FamilyType>(linearStream);
    auto cfeStateIt = find<CFE_STATE *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), cfeStateIt);
    auto cfeState = reinterpret_cast<CFE_STATE *>(*cfeStateIt);

    EXPECT_FALSE(cfeState->getWeightedDispatchModeDisable());
}
HWTEST2_F(PreambleCfeState, givenXehpAndFlagCFEWeightedDispatchModeDisableSetTrueWhenCallingProgramVFEStateThenFieldWeightedDispatchModeDisableAreSet, IsXEHP) {
    using CFE_STATE = typename FamilyType::CFE_STATE;
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.CFEWeightedDispatchModeDisable.set(true);

    auto pVfeCmd = PreambleHelper<FamilyType>::getSpaceForVfeState(&linearStream, *defaultHwInfo, EngineGroupType::RenderCompute);
    StreamProperties streamProperties{};
    streamProperties.frontEndState.setProperties(false, false, false, false, *defaultHwInfo);
    PreambleHelper<FamilyType>::programVfeState(pVfeCmd, *defaultHwInfo, 0u, 0, 0, streamProperties, nullptr);
    parseCommands<FamilyType>(linearStream);
    auto cfeStateIt = find<CFE_STATE *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), cfeStateIt);
    auto cfeState = reinterpret_cast<CFE_STATE *>(*cfeStateIt);

    EXPECT_TRUE(cfeState->getWeightedDispatchModeDisable());
}

HWTEST2_F(PreambleCfeState, givenXehpAndFlagCFEComputeOverdispatchDisableSetFalseWhenCallingProgramVFEStateThenFieldComputeOverdispatchDisableAreNotSet, IsXEHP) {
    using CFE_STATE = typename FamilyType::CFE_STATE;
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.CFEComputeOverdispatchDisable.set(false);

    auto pVfeCmd = PreambleHelper<FamilyType>::getSpaceForVfeState(&linearStream, *defaultHwInfo, EngineGroupType::RenderCompute);
    StreamProperties streamProperties{};
    streamProperties.frontEndState.setProperties(false, false, false, false, *defaultHwInfo);
    PreambleHelper<FamilyType>::programVfeState(pVfeCmd, *defaultHwInfo, 0u, 0, 0, streamProperties, nullptr);
    parseCommands<FamilyType>(linearStream);
    auto cfeStateIt = find<CFE_STATE *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), cfeStateIt);
    auto cfeState = reinterpret_cast<CFE_STATE *>(*cfeStateIt);

    EXPECT_FALSE(cfeState->getComputeOverdispatchDisable());
}
HWTEST2_F(PreambleCfeState, givenXehpAndFlagCFEComputeOverdispatchDisableSetTrueWhenCallingProgramVFEStateThenFieldComputeOverdispatchDisableAreSet, IsXEHP) {
    using CFE_STATE = typename FamilyType::CFE_STATE;
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.CFEComputeOverdispatchDisable.set(true);

    auto pVfeCmd = PreambleHelper<FamilyType>::getSpaceForVfeState(&linearStream, *defaultHwInfo, EngineGroupType::RenderCompute);
    StreamProperties streamProperties{};
    streamProperties.frontEndState.setProperties(false, false, false, false, *defaultHwInfo);
    PreambleHelper<FamilyType>::programVfeState(pVfeCmd, *defaultHwInfo, 0u, 0, 0, streamProperties, nullptr);
    parseCommands<FamilyType>(linearStream);
    auto cfeStateIt = find<CFE_STATE *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), cfeStateIt);
    auto cfeState = reinterpret_cast<CFE_STATE *>(*cfeStateIt);

    EXPECT_TRUE(cfeState->getComputeOverdispatchDisable());
}

HWTEST2_F(PreambleCfeState, givenXehpAndDisabledFusedEuWhenCfeStateProgrammedThenFusedEuDispatchSetToTrue, IsXEHP) {
    using CFE_STATE = typename FamilyType::CFE_STATE;

    auto hwInfo = *defaultHwInfo;
    hwInfo.capabilityTable.fusedEuEnabled = false;

    auto pVfeCmd = PreambleHelper<FamilyType>::getSpaceForVfeState(&linearStream, hwInfo, EngineGroupType::RenderCompute);
    StreamProperties streamProperties{};
    streamProperties.frontEndState.setProperties(false, false, false, false, hwInfo);
    PreambleHelper<FamilyType>::programVfeState(pVfeCmd, hwInfo, 0u, 0, 0, streamProperties, nullptr);
    parseCommands<FamilyType>(linearStream);
    auto cfeStateIt = find<CFE_STATE *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), cfeStateIt);
    auto cfeState = reinterpret_cast<CFE_STATE *>(*cfeStateIt);

    EXPECT_TRUE(cfeState->getFusedEuDispatch());
}

HWTEST2_F(PreambleCfeState, givenXehpAndEnabledFusedEuWhenCfeStateProgrammedThenFusedEuDispatchSetToFalse, IsXEHP) {
    using CFE_STATE = typename FamilyType::CFE_STATE;

    auto hwInfo = *defaultHwInfo;
    hwInfo.capabilityTable.fusedEuEnabled = true;

    auto pVfeCmd = PreambleHelper<FamilyType>::getSpaceForVfeState(&linearStream, hwInfo, EngineGroupType::RenderCompute);
    StreamProperties streamProperties{};
    streamProperties.frontEndState.setProperties(false, false, false, false, hwInfo);
    PreambleHelper<FamilyType>::programVfeState(pVfeCmd, hwInfo, 0u, 0, 0, streamProperties, nullptr);
    parseCommands<FamilyType>(linearStream);
    auto cfeStateIt = find<CFE_STATE *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), cfeStateIt);
    auto cfeState = reinterpret_cast<CFE_STATE *>(*cfeStateIt);

    EXPECT_FALSE(cfeState->getFusedEuDispatch());
}