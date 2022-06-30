/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/context/context.h"

#include "cl_api_tests.h"

using namespace NEO;

typedef api_tests clRetainReleaseSamplerTests;

namespace ULT {
TEST_F(clRetainReleaseSamplerTests, GivenValidSamplerWhenRetainingThenSamplerReferenceCountIsIncremented) {
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
