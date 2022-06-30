/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/hw_info_config.h"
#include "shared/test/common/test_macros/test.h"

#include "opencl/test/unit_test/fixtures/cl_device_fixture.h"

#include <memory>

using namespace NEO;

typedef Test<ClDeviceFixture> Gen9SamplerTest;

GEN9TEST_F(Gen9SamplerTest, WhenAppendingSamplerStateParamsThenStateIsNotChanged) {
    typedef typename FamilyType::SAMPLER_STATE SAMPLER_STATE;
    auto stateWithoutAppendedParams = FamilyType::cmdInitSamplerState;
    auto stateWithAppendedParams = FamilyType::cmdInitSamplerState;
    EXPECT_TRUE(memcmp(&stateWithoutAppendedParams, &stateWithAppendedParams, sizeof(SAMPLER_STATE)) == 0);
    HwInfoConfig::get(defaultHwInfo->platform.eProductFamily)->adjustSamplerState(&stateWithAppendedParams, *defaultHwInfo);
    EXPECT_TRUE(memcmp(&stateWithoutAppendedParams, &stateWithAppendedParams, sizeof(SAMPLER_STATE)) == 0);
}
