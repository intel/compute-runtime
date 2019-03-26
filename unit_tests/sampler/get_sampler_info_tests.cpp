/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/sampler/sampler.h"
#include "unit_tests/fixtures/device_fixture.h"
#include "unit_tests/mocks/mock_context.h"

#include "gtest/gtest.h"

using namespace NEO;

TEST(GetSamplerInfo, InvalidFlags_returnsError) {
    MockContext context;
    auto retVal = CL_INVALID_VALUE;
    auto normalizedCoords = CL_TRUE;
    auto addressingMode = CL_ADDRESS_MIRRORED_REPEAT;
    auto filterMode = CL_FILTER_NEAREST;
    auto sampler = Sampler::create(&context, normalizedCoords,
                                   addressingMode, filterMode, retVal);

    retVal = sampler->getInfo(0, 0, nullptr, nullptr);
    EXPECT_EQ(CL_INVALID_VALUE, retVal);

    delete sampler;
}

struct GetSamplerInfo : public ::testing::TestWithParam<uint32_t /*cl_sampler_info*/> {
    void SetUp() override {
        param = GetParam();
    }

    cl_sampler_info param;
};

TEST_P(GetSamplerInfo, valid_returnsSuccess) {
    MockContext context;
    auto retVal = CL_INVALID_VALUE;
    auto normalizedCoords = CL_TRUE;
    auto addressingMode = CL_ADDRESS_MIRRORED_REPEAT;
    auto filterMode = CL_FILTER_NEAREST;
    auto sampler = Sampler::create(&context, normalizedCoords, addressingMode,
                                   filterMode, CL_FILTER_NEAREST, 2.0f, 3.0f, retVal);

    size_t sizeReturned = 0;
    retVal = sampler->getInfo(param, 0, nullptr, &sizeReturned);
    ASSERT_EQ(CL_SUCCESS, retVal) << " param = " << param;
    ASSERT_NE(0u, sizeReturned);

    auto *object = new char[sizeReturned];
    retVal = sampler->getInfo(param, sizeReturned, object, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);

    delete[] object;
    delete sampler;
}

// Define new command types to run the parameterized tests
cl_sampler_info samplerInfoParams[] = {
    CL_SAMPLER_REFERENCE_COUNT,
    CL_SAMPLER_CONTEXT,
    CL_SAMPLER_NORMALIZED_COORDS,
    CL_SAMPLER_ADDRESSING_MODE,
    CL_SAMPLER_FILTER_MODE,
    CL_SAMPLER_MIP_FILTER_MODE,
    CL_SAMPLER_LOD_MIN,
    CL_SAMPLER_LOD_MAX};

INSTANTIATE_TEST_CASE_P(
    Sampler_,
    GetSamplerInfo,
    testing::ValuesIn(samplerInfoParams));
