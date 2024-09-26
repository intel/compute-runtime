/*
 * Copyright (C) 2018-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/context/context.h"

#include "cl_api_tests.h"

using namespace NEO;

using ClReleaseMemObjectTests = ApiTests;

namespace ULT {

TEST_F(ClReleaseMemObjectTests, GivenValidBufferWhenReleasingMemObjectThenSuccessIsReturned) {
    cl_mem_flags flags = CL_MEM_USE_HOST_PTR;
    static const unsigned int bufferSize = 16;
    cl_mem buffer = nullptr;

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

    retVal = clReleaseMemObject(buffer);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(ClReleaseMemObjectTests, GivenInvalidMemObjWhenReleasingThenErrorCodeReturn) {
    auto retVal = clReleaseMemObject(nullptr);
    EXPECT_NE(CL_SUCCESS, retVal);
}

TEST_F(ClReleaseMemObjectTests, GivenInvalidMemObjWhenTerdownWasCalledThenSuccessReturned) {
    wasPlatformTeardownCalled = true;
    auto retVal = clReleaseMemObject(nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    wasPlatformTeardownCalled = false;
}

} // namespace ULT
