/*
 * Copyright (C) 2021-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/test_macros/hw_test.h"

#include "opencl/test/unit_test/fixtures/cl_device_fixture.h"

using namespace NEO;

using XeHPAndLaterSamplerTest = Test<ClDeviceFixture>;

HWTEST2_F(XeHPAndLaterSamplerTest, GivenDefaultThenLowQualityFilterIsDisabled, IsAtMostXe3pCore) {
    using SAMPLER_STATE = typename FamilyType::SAMPLER_STATE;
    auto state = FamilyType::cmdInitSamplerState;
    EXPECT_EQ(SAMPLER_STATE::LOW_QUALITY_FILTER_DISABLE, state.getLowQualityFilter());
}
