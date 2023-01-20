/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "opencl/test/unit_test/fixtures/cl_device_fixture.h"

#include <memory>

using namespace NEO;

using XeHPAndLaterSamplerTest = Test<ClDeviceFixture>;

HWCMDTEST_F(IGFX_XE_HP_CORE, XeHPAndLaterSamplerTest, GivenDefaultThenLowQualityFilterIsDisabled) {
    using SAMPLER_STATE = typename FamilyType::SAMPLER_STATE;
    auto state = FamilyType::cmdInitSamplerState;
    EXPECT_EQ(SAMPLER_STATE::LOW_QUALITY_FILTER_DISABLE, state.getLowQualityFilter());
}
