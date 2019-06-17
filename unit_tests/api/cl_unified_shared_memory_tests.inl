/*
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/api/api.h"

using namespace NEO;

TEST(clUnifiedSharedMemoryTests, whenClHostMemAllocINTELisCalledThenOutOfHostMemoryErrorIsReturned) {
    cl_int retVal = CL_SUCCESS;
    clHostMemAllocINTEL(0, nullptr, 0, 0, &retVal);
    EXPECT_EQ(CL_OUT_OF_HOST_MEMORY, retVal);
}

TEST(clUnifiedSharedMemoryTests, whenClDeviceMemAllocINTELisCalledThenOutOfHostMemoryErrorIsReturned) {
    cl_int retVal = CL_SUCCESS;
    clDeviceMemAllocINTEL(0, 0, nullptr, 0, 0, &retVal);
    EXPECT_EQ(CL_OUT_OF_HOST_MEMORY, retVal);
}

TEST(clUnifiedSharedMemoryTests, whenClSharedMemAllocINTELisCalledThenOutOfHostMemoryErrorIsReturned) {
    cl_int retVal = CL_SUCCESS;
    clSharedMemAllocINTEL(0, 0, nullptr, 0, 0, &retVal);
    EXPECT_EQ(CL_OUT_OF_HOST_MEMORY, retVal);
}

TEST(clUnifiedSharedMemoryTests, whenClMemFreeINTELisCalledThenOutOfHostMemoryErrorIsReturned) {
    auto retVal = clMemFreeINTEL(0, nullptr);
    EXPECT_EQ(CL_OUT_OF_HOST_MEMORY, retVal);
}

TEST(clUnifiedSharedMemoryTests, whenClGetMemAllocInfoINTELisCalledThenOutOfHostMemoryErrorIsReturned) {
    auto retVal = clGetMemAllocInfoINTEL(0, nullptr, 0, 0, nullptr, nullptr);
    EXPECT_EQ(CL_OUT_OF_HOST_MEMORY, retVal);
}

TEST(clUnifiedSharedMemoryTests, whenClSetKernelArgMemPointerINTELisCalledThenOutOfHostMemoryErrorIsReturned) {
    auto retVal = clSetKernelArgMemPointerINTEL(0, 0, nullptr);
    EXPECT_EQ(CL_OUT_OF_HOST_MEMORY, retVal);
}

TEST(clUnifiedSharedMemoryTests, whenclEnqueueMemsetINTELisCalledThenOutOfHostMemoryErrorIsReturned) {
    auto retVal = clEnqueueMemsetINTEL(0, nullptr, 0, 0, 0, nullptr, nullptr);
    EXPECT_EQ(CL_OUT_OF_HOST_MEMORY, retVal);
}

TEST(clUnifiedSharedMemoryTests, whenClEnqueueMemcpyINTELisCalledThenOutOfHostMemoryErrorIsReturned) {
    auto retVal = clEnqueueMemcpyINTEL(0, 0, nullptr, nullptr, 0, 0, nullptr, nullptr);
    EXPECT_EQ(CL_OUT_OF_HOST_MEMORY, retVal);
}

TEST(clUnifiedSharedMemoryTests, whenClEnqueueMigrateMemINTELisCalledThenOutOfHostMemoryErrorIsReturned) {
    auto retVal = clEnqueueMigrateMemINTEL(0, nullptr, 0, 0, 0, nullptr, nullptr);
    EXPECT_EQ(CL_OUT_OF_HOST_MEMORY, retVal);
}

TEST(clUnifiedSharedMemoryTests, whenClEnqueueMemAdviseINTELisCalledThenOutOfHostMemoryErrorIsReturned) {
    auto retVal = clEnqueueMemAdviseINTEL(0, nullptr, 0, 0, 0, nullptr, nullptr);
    EXPECT_EQ(CL_OUT_OF_HOST_MEMORY, retVal);
}
