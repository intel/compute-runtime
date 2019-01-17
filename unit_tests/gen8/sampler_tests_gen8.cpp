/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/sampler/sampler.h"
#include "unit_tests/fixtures/device_fixture.h"
#include "unit_tests/mocks/mock_context.h"
#include "test.h"
#include <memory>

using namespace OCLRT;

typedef Test<DeviceFixture> Gen8SamplerTest;

GEN8TEST_F(Gen8SamplerTest, appendSamplerStateParamsDoesNothing) {
    typedef typename FamilyType::SAMPLER_STATE SAMPLER_STATE;
    std::unique_ptr<MockContext> context(new MockContext());
    std::unique_ptr<SamplerHw<FamilyType>> sampler(new SamplerHw<FamilyType>(context.get(), CL_FALSE, CL_ADDRESS_NONE, CL_FILTER_NEAREST));
    auto stateWithoutAppendedParams = FamilyType::cmdInitSamplerState;
    auto stateWithAppendedParams = FamilyType::cmdInitSamplerState;
    EXPECT_TRUE(memcmp(&stateWithoutAppendedParams, &stateWithAppendedParams, sizeof(SAMPLER_STATE)) == 0);
    sampler->appendSamplerStateParams(&stateWithAppendedParams);
    EXPECT_TRUE(memcmp(&stateWithoutAppendedParams, &stateWithAppendedParams, sizeof(SAMPLER_STATE)) == 0);
}
