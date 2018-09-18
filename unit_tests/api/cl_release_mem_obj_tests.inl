/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "cl_api_tests.h"
#include "runtime/context/context.h"

using namespace OCLRT;

typedef api_tests clReleaseMemObjectTests;

namespace ULT {

TEST_F(clReleaseMemObjectTests, returnsSuccess) {
    unsigned char *hostMem = nullptr;
    cl_mem_flags flags = CL_MEM_USE_HOST_PTR;
    static const unsigned int bufferSize = 16;
    cl_mem buffer = nullptr;

    hostMem = new unsigned char[bufferSize];
    memset(hostMem, 0xaa, bufferSize);

    buffer = clCreateBuffer(
        pContext,
        flags,
        bufferSize,
        hostMem,
        &retVal);

    ASSERT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, buffer);

    retVal = clReleaseMemObject(buffer);
    EXPECT_EQ(CL_SUCCESS, retVal);

    delete[] hostMem;
}
} // namespace ULT
