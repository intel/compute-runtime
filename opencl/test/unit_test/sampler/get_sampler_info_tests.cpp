/*
 * Copyright (C) 2018-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/sampler/sampler.h"
#include "opencl/test/unit_test/mocks/mock_context.h"

#include "gtest/gtest.h"

using namespace NEO;

TEST(GetSamplerInfo, GivenInvalidFlagsWhenGettingSamplerInfoThenInvalidValueErrorIsReturnedAndValueSizeRetIsNotUpdated) {
    MockContext context;
    auto retVal = CL_INVALID_VALUE;
    auto normalizedCoords = CL_TRUE;
    auto addressingMode = CL_ADDRESS_MIRRORED_REPEAT;
    auto filterMode = CL_FILTER_NEAREST;
    auto sampler = Sampler::create(&context, normalizedCoords,
                                   addressingMode, filterMode, retVal);

    size_t valueSizeRet = 0x1234;

    retVal = sampler->getInfo(0, 0, nullptr, &valueSizeRet);
    EXPECT_EQ(CL_INVALID_VALUE, retVal);
    EXPECT_EQ(0x1234u, valueSizeRet);

    delete sampler;
}

TEST(GetSamplerInfo, GivenValidParamWhenGettingInfoThenSuccessIsReturned) {
    MockContext context;
    auto retVal = CL_INVALID_VALUE;
    auto sampler = Sampler::create(&context, CL_TRUE, CL_ADDRESS_MIRRORED_REPEAT,
                                   CL_FILTER_NEAREST, CL_FILTER_NEAREST, 2.0f, 3.0f, retVal);

    const cl_sampler_info samplerInfoParams[] = {
        CL_SAMPLER_REFERENCE_COUNT,
        CL_SAMPLER_CONTEXT,
        CL_SAMPLER_NORMALIZED_COORDS,
        CL_SAMPLER_ADDRESSING_MODE,
        CL_SAMPLER_FILTER_MODE,
        CL_SAMPLER_MIP_FILTER_MODE,
        CL_SAMPLER_LOD_MIN,
        CL_SAMPLER_LOD_MAX};

    for (auto infoParam : samplerInfoParams) {
        size_t sizeReturned = 0;
        retVal = sampler->getInfo(infoParam, 0, nullptr, &sizeReturned);
        ASSERT_EQ(CL_SUCCESS, retVal) << " infoParam = " << infoParam;
        ASSERT_NE(0u, sizeReturned);

        auto *object = new char[sizeReturned];
        retVal = sampler->getInfo(infoParam, sizeReturned, object, nullptr);
        EXPECT_EQ(CL_SUCCESS, retVal);
        delete[] object;
    }

    delete sampler;
}
