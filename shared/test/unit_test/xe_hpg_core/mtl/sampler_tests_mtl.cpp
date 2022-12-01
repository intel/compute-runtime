/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/os_interface/hw_info_config.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/test_macros/hw_test.h"

#include <memory>

using namespace NEO;

using SamplerTestMtl = ::testing::Test;

HWTEST2_F(SamplerTestMtl, givenMtlSamplerWhenUsingDefaultFilteringAndAppendSamplerStateParamsThenDisableLowQualityFilter, IsMTL) {
    EXPECT_FALSE(DebugManager.flags.ForceSamplerLowFilteringPrecision.get());
    typedef typename FamilyType::SAMPLER_STATE SAMPLER_STATE;
    auto state = FamilyType::cmdInitSamplerState;
    EXPECT_EQ(SAMPLER_STATE::LOW_QUALITY_FILTER_DISABLE, state.getLowQualityFilter());
    ProductHelper::get(defaultHwInfo->platform.eProductFamily)->adjustSamplerState(&state, *defaultHwInfo);
    EXPECT_EQ(SAMPLER_STATE::LOW_QUALITY_FILTER_DISABLE, state.getLowQualityFilter());
}

HWTEST2_F(SamplerTestMtl, giveMtlSamplerWhenForcingLowQualityFilteringAndAppendSamplerStateParamsThenEnableLowQualityFilter, IsMTL) {
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.ForceSamplerLowFilteringPrecision.set(true);
    EXPECT_TRUE(DebugManager.flags.ForceSamplerLowFilteringPrecision.get());
    typedef typename FamilyType::SAMPLER_STATE SAMPLER_STATE;
    auto state = FamilyType::cmdInitSamplerState;
    EXPECT_EQ(SAMPLER_STATE::LOW_QUALITY_FILTER_DISABLE, state.getLowQualityFilter());
    ProductHelper::get(defaultHwInfo->platform.eProductFamily)->adjustSamplerState(&state, *defaultHwInfo);
    EXPECT_EQ(SAMPLER_STATE::LOW_QUALITY_FILTER_ENABLE, state.getLowQualityFilter());
}
