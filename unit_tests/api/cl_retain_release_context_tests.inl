/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "cl_api_tests.h"
#include "runtime/platform/platform.h"

using namespace OCLRT;

typedef api_tests clRetainReleaseContextTests;

namespace ULT {

TEST_F(clRetainReleaseContextTests, retainAndrelease) {
    cl_context context = clCreateContext(nullptr, 1, devices, nullptr, nullptr, &retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, context);

    cl_uint theRef;
    retVal = clGetContextInfo(context, CL_CONTEXT_REFERENCE_COUNT,
                              sizeof(cl_uint), &theRef, NULL);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(1u, theRef);

    retVal = clRetainContext(context);
    EXPECT_EQ(CL_SUCCESS, retVal);
    retVal = clGetContextInfo(context, CL_CONTEXT_REFERENCE_COUNT,
                              sizeof(cl_uint), &theRef, NULL);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(2u, theRef);

    retVal = clReleaseContext(context);
    EXPECT_EQ(CL_SUCCESS, retVal);
    retVal = clGetContextInfo(context, CL_CONTEXT_REFERENCE_COUNT,
                              sizeof(cl_uint), &theRef, NULL);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(1u, theRef);

    retVal = clReleaseContext(context);
    EXPECT_EQ(CL_SUCCESS, retVal);
}
} // namespace ULT
