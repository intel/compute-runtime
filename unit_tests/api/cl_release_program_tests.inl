/*
 * Copyright (c) 2017 - 2018, Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#include "cl_api_tests.h"
#include "runtime/context/context.h"

using namespace OCLRT;

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
