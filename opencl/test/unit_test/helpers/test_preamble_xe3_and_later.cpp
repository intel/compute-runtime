/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/stream_properties.h"
#include "shared/test/common/fixtures/preamble_fixture.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"

using namespace NEO;
#include "shared/test/common/test_macros/header/heapless_matchers.h"

using PreambleCfeStateXe3AndLater = PreambleFixture;

HWTEST2_F(PreambleCfeStateXe3AndLater, givenSetDebugFlagWhenPreambleCfeStateIsProgrammedThenCFEStateParamsHaveSetValue, IsHeapfulSupportedAndAtLeastXe3Core) {
    using CFE_STATE = typename FamilyType::CFE_STATE;

    uint32_t expectedValue1 = 1u;
    uint32_t expectedValue2 = 2u;

    DebugManagerStateRestore dbgRestore;

    debugManager.flags.CFEFusedEUDispatch.set(expectedValue1);
    debugManager.flags.OverDispatchControl.set(expectedValue1);
    debugManager.flags.CFESingleSliceDispatchCCSMode.set(expectedValue1);
    debugManager.flags.CFENumberOfWalkers.set(expectedValue2);
    debugManager.flags.MaximumNumberOfThreads.set(expectedValue2);

    uint64_t expectedAddress = 1 << CFE_STATE::SCRATCHSPACEBUFFER_BIT_SHIFT;
    auto pVfeCmd = PreambleHelper<FamilyType>::getSpaceForVfeState(&linearStream, *defaultHwInfo, EngineGroupType::renderCompute);
    StreamProperties emptyProperties{};
    PreambleHelper<FamilyType>::programVfeState(pVfeCmd, pDevice->getRootDeviceEnvironment(), 0u, expectedAddress, 16u, emptyProperties);

    parseCommands<FamilyType>(linearStream);
    auto cfeStateIt = find<CFE_STATE *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), cfeStateIt);

    auto cfeState = reinterpret_cast<CFE_STATE *>(*cfeStateIt);

    EXPECT_EQ(expectedValue1, static_cast<uint32_t>(cfeState->getOverDispatchControl()));
    if constexpr (TestTraits<FamilyType::gfxCoreFamily>::numberOfWalkersInCfeStateSupported) {
        EXPECT_EQ(expectedValue2, cfeState->getNumberOfWalkers());
    }
    EXPECT_EQ(expectedValue2, cfeState->getMaximumNumberOfThreads());
}
