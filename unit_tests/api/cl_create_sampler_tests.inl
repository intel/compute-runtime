/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/context/context.h"

#include "cl_api_tests.h"

using namespace OCLRT;

typedef api_tests clCreateSamplerTests;

namespace ULT {

TEST_F(clCreateSamplerTests, GivenCorrectParametersWhenCreatingSamplerThenSamplerIsCreatedAndSuccessReturned) {
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

TEST_F(clCreateSamplerTests, GivenCorrectParametersAndNullReturnPointerWhenCreatingSamplerThenSamplerIsCreated) {
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

TEST_F(clCreateSamplerTests, GivenInvalidContextWhenCreatingSamplerThenInvalidContextErrorIsReturned) {
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
