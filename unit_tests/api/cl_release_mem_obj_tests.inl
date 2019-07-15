/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/context/context.h"

#include "cl_api_tests.h"

using namespace NEO;

typedef api_tests clReleaseMemObjectTests;

namespace ULT {

TEST_F(clReleaseMemObjectTests, GivenValidBufferWhenReleasingMemObjectThenSuccessIsReturned) {
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
} // namespace ULT
