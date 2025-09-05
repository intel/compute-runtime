/*
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/stream_properties.h"
#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/os_interface/product_helper.h"
#include "shared/test/common/fixtures/preamble_fixture.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"

using namespace NEO;
#include "shared/test/common/test_macros/header/heapless_matchers.h"

using PreambleCfeStateDg2AndLater = PreambleFixture;
using IsDG2AndLater = IsAtLeastXeCore;

HWTEST2_F(PreambleCfeStateDg2AndLater, whenprogramVFEStateIsCalledWithProperAdditionalKernelExecInfoThenProperStateIsSet, IsHeapfulRequiredAndAtLeastXeCore) {
    using CFE_STATE = typename FamilyType::CFE_STATE;

    HardwareInfo hwInfo = *defaultHwInfo;
    const auto &productHelper = pDevice->getProductHelper();

    hwInfo.platform.usRevId = productHelper.getHwRevIdFromStepping(REVISION_B, hwInfo);

    if (!productHelper.isDisableOverdispatchAvailable(hwInfo)) {
        GTEST_SKIP();
    }

    auto pVfeCmd = PreambleHelper<FamilyType>::getSpaceForVfeState(&linearStream, hwInfo, EngineGroupType::renderCompute, nullptr);
    StreamProperties properties{};
    properties.frontEndState.disableOverdispatch.value = 1;
    PreambleHelper<FamilyType>::programVfeState(pVfeCmd, pDevice->getRootDeviceEnvironment(), 0u, 0, 0, properties);
    parseCommands<FamilyType>(linearStream);
    auto cfeStateIt = find<CFE_STATE *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), cfeStateIt);
    auto cfeState = reinterpret_cast<CFE_STATE *>(*cfeStateIt);
    EXPECT_TRUE(cfeState->getComputeOverdispatchDisable());

    cmdList.clear();
    pVfeCmd = PreambleHelper<FamilyType>::getSpaceForVfeState(&linearStream, hwInfo, EngineGroupType::renderCompute, nullptr);
    properties.frontEndState.disableOverdispatch.value = 0;
    PreambleHelper<FamilyType>::programVfeState(pVfeCmd, pDevice->getRootDeviceEnvironment(), 0u, 0, 0, properties);
    parseCommands<FamilyType>(linearStream);
    cfeStateIt = find<CFE_STATE *>(cmdList.begin(), cmdList.end());
    cfeStateIt++;
    ASSERT_NE(cmdList.end(), cfeStateIt);
    cfeState = reinterpret_cast<CFE_STATE *>(*cfeStateIt);
    EXPECT_FALSE(cfeState->getComputeOverdispatchDisable());
}

HWTEST2_F(PreambleCfeStateDg2AndLater, givenSetDebugFlagWhenPreambleCfeStateIsProgrammedThenCFEStateParamsHaveSetValue, IsHeapfulRequiredAndAtLeastXeCore) {
    using CFE_STATE = typename FamilyType::CFE_STATE;

    uint32_t expectedValue1 = 1u;

    DebugManagerStateRestore dbgRestore;

    debugManager.flags.ComputeOverdispatchDisable.set(expectedValue1);

    uint64_t expectedAddress = 1 << CFE_STATE::SCRATCHSPACEBUFFER_BIT_SHIFT;
    auto pVfeCmd = PreambleHelper<FamilyType>::getSpaceForVfeState(&linearStream, *defaultHwInfo, EngineGroupType::renderCompute, nullptr);
    StreamProperties emptyProperties{};
    PreambleHelper<FamilyType>::programVfeState(pVfeCmd, pDevice->getRootDeviceEnvironment(), 0u, expectedAddress, 16u, emptyProperties);

    parseCommands<FamilyType>(linearStream);
    auto cfeStateIt = find<CFE_STATE *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), cfeStateIt);

    auto cfeState = reinterpret_cast<CFE_STATE *>(*cfeStateIt);

    EXPECT_EQ(expectedValue1, cfeState->getComputeOverdispatchDisable());
}
