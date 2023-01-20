/*
 * Copyright (C) 2019-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/os_interface/hw_info_config.h"
#include "shared/test/common/fixtures/device_fixture.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/test_macros/test.h"

#include <memory>

using namespace NEO;

using Gen12LpSamplerTest = Test<DeviceFixture>;

HWTEST2_F(Gen12LpSamplerTest, givenTglLpSamplerWhenUsingDefaultFilteringAndAppendSamplerStateParamsThenDisableLowQualityFilter, IsTGLLP) {
    EXPECT_FALSE(DebugManager.flags.ForceSamplerLowFilteringPrecision.get());
    typedef typename FamilyType::SAMPLER_STATE SAMPLER_STATE;
    auto state = FamilyType::cmdInitSamplerState;
    EXPECT_EQ(SAMPLER_STATE::LOW_QUALITY_FILTER_DISABLE, state.getLowQualityFilter());
    ProductHelper::get(defaultHwInfo->platform.eProductFamily)->adjustSamplerState(&state, *defaultHwInfo);
    EXPECT_EQ(SAMPLER_STATE::LOW_QUALITY_FILTER_DISABLE, state.getLowQualityFilter());
}

HWTEST2_F(Gen12LpSamplerTest, givenTglLpSamplerWhenForcingLowQualityFilteringAndAppendSamplerStateParamsThenEnableLowQualityFilter, IsTGLLP) {
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.ForceSamplerLowFilteringPrecision.set(true);
    EXPECT_TRUE(DebugManager.flags.ForceSamplerLowFilteringPrecision.get());
    typedef typename FamilyType::SAMPLER_STATE SAMPLER_STATE;
    auto state = FamilyType::cmdInitSamplerState;
    EXPECT_EQ(SAMPLER_STATE::LOW_QUALITY_FILTER_DISABLE, state.getLowQualityFilter());
    ProductHelper::get(defaultHwInfo->platform.eProductFamily)->adjustSamplerState(&state, *defaultHwInfo);
    EXPECT_EQ(SAMPLER_STATE::LOW_QUALITY_FILTER_ENABLE, state.getLowQualityFilter());
}

GEN12LPTEST_F(Gen12LpSamplerTest, GivenDefaultWhenGettingLowLowQualityFilterStateThenItIsDisabled) {
    typedef typename FamilyType::SAMPLER_STATE SAMPLER_STATE;
    auto state = FamilyType::cmdInitSamplerState;
    EXPECT_EQ(SAMPLER_STATE::LOW_QUALITY_FILTER_DISABLE, state.getLowQualityFilter());
}

GEN12LPTEST_F(Gen12LpSamplerTest, givenGen12LpSamplerWhenProgrammingLowQualityCubeCornerModeThenTheModeChangesAppropriately) {
    typedef typename FamilyType::SAMPLER_STATE SAMPLER_STATE;
    auto state = FamilyType::cmdInitSamplerState;
    EXPECT_EQ(SAMPLER_STATE::LOW_QUALITY_CUBE_CORNER_MODE_ENABLE, state.getLowQualityCubeCornerMode());
    state.setLowQualityCubeCornerMode(SAMPLER_STATE::LOW_QUALITY_CUBE_CORNER_MODE_DISABLE);
    EXPECT_EQ(SAMPLER_STATE::LOW_QUALITY_CUBE_CORNER_MODE_DISABLE, state.getLowQualityCubeCornerMode());
}
