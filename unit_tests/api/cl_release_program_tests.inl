/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/context/context.h"

#include "cl_api_tests.h"

using namespace NEO;

typedef api_tests clReleaseProgramTests;

TEST_F(clReleaseProgramTests, releaseNullptr) {
    auto retVal = clReleaseProgram(nullptr);
    EXPECT_EQ(CL_INVALID_PROGRAM, retVal);
}

static const char fakeSrc[] = "__kernel void func(void) { }";

TEST_F(clReleaseProgramTests, validRelease) {
    size_t srcLen = sizeof(fakeSrc);
    const char *src = fakeSrc;
    cl_int retVal;
    cl_uint theRef;

    cl_program prog = clCreateProgramWithSource(pContext, 1, &src, &srcLen, &retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = clRetainProgram(prog);
    EXPECT_EQ(CL_SUCCESS, retVal);
    retVal = clGetProgramInfo(prog, CL_PROGRAM_REFERENCE_COUNT,
                              sizeof(cl_uint), &theRef, NULL);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(2u, theRef);

    retVal = clReleaseProgram(prog);
    EXPECT_EQ(CL_SUCCESS, retVal);
    retVal = clGetProgramInfo(prog, CL_PROGRAM_REFERENCE_COUNT,
                              sizeof(cl_uint), &theRef, NULL);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(1u, theRef);

    retVal = clReleaseProgram(prog);
    EXPECT_EQ(CL_SUCCESS, retVal);
}
