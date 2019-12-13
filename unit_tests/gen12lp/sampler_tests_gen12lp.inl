/*
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/debug_settings/debug_settings_manager.h"
#include "core/unit_tests/helpers/debug_manager_state_restore.h"
#include "core/unit_tests/utilities/base_object_utils.h"
#include "runtime/sampler/sampler.h"
#include "test.h"
#include "unit_tests/fixtures/device_fixture.h"
#include "unit_tests/mocks/mock_context.h"

#include <memory>

using namespace NEO;

typedef Test<DeviceFixture> Gen12LpSamplerTest;

TGLLPTEST_F(Gen12LpSamplerTest, givenTglLpSamplerWhenUsingDefaultFilteringAndAppendSamplerStateParamsThenDisableLowQualityFilter) {
    EXPECT_FALSE(DebugManager.flags.ForceSamplerLowFilteringPrecision.get());
    typedef typename FamilyType::SAMPLER_STATE SAMPLER_STATE;
    auto context = clUniquePtr(new MockContext());
    auto sampler = clUniquePtr(new SamplerHw<FamilyType>(context.get(), CL_FALSE, CL_ADDRESS_NONE, CL_FILTER_NEAREST));
    auto state = FamilyType::cmdInitSamplerState;
    EXPECT_EQ(SAMPLER_STATE::LOW_QUALITY_FILTER_DISABLE, state.getLowQualityFilter());
    sampler->appendSamplerStateParams(&state);
    EXPECT_EQ(SAMPLER_STATE::LOW_QUALITY_FILTER_DISABLE, state.getLowQualityFilter());
}

TGLLPTEST_F(Gen12LpSamplerTest, givenTglLpSamplerWhenForcingLowQualityFilteringAndAppendSamplerStateParamsThenEnableLowQualityFilter) {
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.ForceSamplerLowFilteringPrecision.set(true);
    EXPECT_TRUE(DebugManager.flags.ForceSamplerLowFilteringPrecision.get());
    typedef typename FamilyType::SAMPLER_STATE SAMPLER_STATE;
    auto context = clUniquePtr(new MockContext());
    auto sampler = clUniquePtr(new SamplerHw<FamilyType>(context.get(), CL_FALSE, CL_ADDRESS_NONE, CL_FILTER_NEAREST));
    auto state = FamilyType::cmdInitSamplerState;
    EXPECT_EQ(SAMPLER_STATE::LOW_QUALITY_FILTER_DISABLE, state.getLowQualityFilter());
    sampler->appendSamplerStateParams(&state);
    EXPECT_EQ(SAMPLER_STATE::LOW_QUALITY_FILTER_ENABLE, state.getLowQualityFilter());
}

GEN12LPTEST_F(Gen12LpSamplerTest, defaultLowQualityFilter) {
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
