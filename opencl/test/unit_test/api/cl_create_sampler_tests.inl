/*
 * Copyright (C) 2018-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/context/context.h"

#include "cl_api_tests.h"

using namespace NEO;

using ClCreateSamplerTests = ApiTests;

namespace ULT {

TEST_F(ClCreateSamplerTests, GivenCorrectParametersWhenCreatingSamplerThenSamplerIsCreatedAndSuccessReturned) {
    auto sampler = clCreateSampler(
        pContext,
        CL_TRUE,
        CL_ADDRESS_CLAMP,
        CL_FILTER_NEAREST,
        &retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, sampler);

    retVal = clReleaseSampler(sampler);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(ClCreateSamplerTests, GivenCorrectParametersAndNullReturnPointerWhenCreatingSamplerThenSamplerIsCreated) {
    auto sampler = clCreateSampler(
        pContext,
        CL_TRUE,
        CL_ADDRESS_CLAMP,
        CL_FILTER_NEAREST,
        nullptr);
    EXPECT_NE(nullptr, sampler);

    retVal = clReleaseSampler(sampler);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(ClCreateSamplerTests, GivenInvalidContextWhenCreatingSamplerThenInvalidContextErrorIsReturned) {
    auto sampler = clCreateSampler(
        nullptr,
        CL_FALSE,
        CL_ADDRESS_CLAMP_TO_EDGE,
        CL_FILTER_LINEAR,
        &retVal);
    EXPECT_EQ(CL_INVALID_CONTEXT, retVal);
    EXPECT_EQ(nullptr, sampler);
    delete sampler;
}
} // namespace ULT
