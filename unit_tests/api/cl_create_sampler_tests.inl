/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "cl_api_tests.h"
#include "runtime/context/context.h"

using namespace OCLRT;

typedef api_tests clCreateSamplerTests;

namespace ULT {

TEST_F(clCreateSamplerTests, returnsSuccess) {
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

TEST_F(clCreateSamplerTests, noRet) {
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

TEST_F(clCreateSamplerTests, nullContextReturnsError) {
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
