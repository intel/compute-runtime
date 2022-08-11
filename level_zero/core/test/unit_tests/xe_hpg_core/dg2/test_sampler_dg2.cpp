/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/utilities/numeric.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/core/source/sampler/sampler_hw.h"
#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_sampler.h"

namespace L0 {
namespace ult {

using SamplerCreateTest = Test<DeviceFixture>;

HWTEST2_F(SamplerCreateTest, givenDg2WhenInitializeSamplerAndForceSamplerLowFilteringPrecisionIsFalseThenLowQualityFilterIsDisabled, IsDG2) {
    using SAMPLER_STATE = typename NEO::XeHpgCoreFamily::SAMPLER_STATE;
    EXPECT_FALSE(DebugManager.flags.ForceSamplerLowFilteringPrecision.get());
    ze_sampler_address_mode_t addressMode = ZE_SAMPLER_ADDRESS_MODE_REPEAT;
    ze_sampler_filter_mode_t filterMode = ZE_SAMPLER_FILTER_MODE_NEAREST;
    ze_bool_t isNormalized = true;

    ze_sampler_desc_t desc = {};
    desc.addressMode = addressMode;
    desc.filterMode = filterMode;
    desc.isNormalized = isNormalized;

    auto sampler = static_cast<MockSamplerHw<IGFX_XE_HPG_CORE> *>((*samplerFactory[IGFX_DG2])());
    sampler->initialize(device, &desc);

    EXPECT_EQ(SAMPLER_STATE::LOW_QUALITY_FILTER_DISABLE, sampler->samplerState.getLowQualityFilter());

    sampler->destroy();
}

HWTEST2_F(SamplerCreateTest, givenDg2WhenInitializeSamplerAndForceSamplerLowFilteringPrecisionIsTrueThenLowQualityFilterIsEnabled, IsDG2) {
    using SAMPLER_STATE = typename NEO::XeHpgCoreFamily::SAMPLER_STATE;
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.ForceSamplerLowFilteringPrecision.set(true);
    EXPECT_TRUE(DebugManager.flags.ForceSamplerLowFilteringPrecision.get());
    ze_sampler_address_mode_t addressMode = ZE_SAMPLER_ADDRESS_MODE_REPEAT;
    ze_sampler_filter_mode_t filterMode = ZE_SAMPLER_FILTER_MODE_NEAREST;
    ze_bool_t isNormalized = true;

    ze_sampler_desc_t desc = {};
    desc.addressMode = addressMode;
    desc.filterMode = filterMode;
    desc.isNormalized = isNormalized;

    auto sampler = static_cast<MockSamplerHw<IGFX_XE_HPG_CORE> *>((*samplerFactory[IGFX_DG2])());
    sampler->initialize(device, &desc);

    EXPECT_EQ(SAMPLER_STATE::LOW_QUALITY_FILTER_ENABLE, sampler->samplerState.getLowQualityFilter());

    sampler->destroy();
}

} // namespace ult
} // namespace L0
