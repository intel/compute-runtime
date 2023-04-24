/*
 * Copyright (C) 2018-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/context/context.h"

#include "cl_api_tests.h"

using namespace NEO;

using ClRetainMemObjectTests = ApiTests;

namespace ULT {
TEST_F(ClRetainMemObjectTests, GivenValidParamsWhenRetainingMemObjectThenRefCountIsIncremented) {
    cl_mem_flags flags = CL_MEM_USE_HOST_PTR;
    static const unsigned int bufferSize = 16;
    cl_mem buffer = nullptr;
    cl_int retVal;
    cl_uint theRef;

    std::unique_ptr<char[]> hostMem(new char[bufferSize]);
    memset(hostMem.get(), 0xaa, bufferSize);

    buffer = clCreateBuffer(
        pContext,
        flags,
        bufferSize,
        hostMem.get(),
        &retVal);

    ASSERT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, buffer);

    retVal = clRetainMemObject(buffer);
    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = clGetMemObjectInfo(buffer, CL_MEM_REFERENCE_COUNT,
                                sizeof(cl_uint), &theRef, NULL);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(2u, theRef);

    retVal = clReleaseMemObject(buffer);
    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = clGetMemObjectInfo(buffer, CL_MEM_REFERENCE_COUNT,
                                sizeof(cl_uint), &theRef, NULL);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(1u, theRef);

    retVal = clReleaseMemObject(buffer);
    EXPECT_EQ(CL_SUCCESS, retVal);
}
} // namespace ULT
