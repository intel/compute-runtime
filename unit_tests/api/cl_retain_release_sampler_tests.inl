/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "cl_api_tests.h"
#include "runtime/context/context.h"

using namespace OCLRT;

typedef api_tests clRetainReleaseSamplerTests;

namespace ULT {
TEST_F(clRetainReleaseSamplerTests, RetainRelease) {
    cl_int retVal = CL_SUCCESS;
    auto sampler = clCreateSampler(pContext, CL_TRUE, CL_ADDRESS_CLAMP,
                                   CL_FILTER_NEAREST, &retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, sampler);

    cl_uint theRef;
    retVal = clGetSamplerInfo(sampler, CL_SAMPLER_REFERENCE_COUNT,
                              sizeof(cl_uint), &theRef, NULL);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(1u, theRef);

    retVal = clRetainSampler(sampler);
    EXPECT_EQ(CL_SUCCESS, retVal);
    retVal = clGetSamplerInfo(sampler, CL_SAMPLER_REFERENCE_COUNT,
                              sizeof(cl_uint), &theRef, NULL);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(2u, theRef);

    retVal = clReleaseSampler(sampler);
    EXPECT_EQ(CL_SUCCESS, retVal);
    retVal = clGetSamplerInfo(sampler, CL_SAMPLER_REFERENCE_COUNT,
                              sizeof(cl_uint), &theRef, NULL);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(1u, theRef);

    retVal = clReleaseSampler(sampler);
    EXPECT_EQ(CL_SUCCESS, retVal);
}
} // namespace ULT
