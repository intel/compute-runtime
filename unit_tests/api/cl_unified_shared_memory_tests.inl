/*
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/api/api.h"
#include "runtime/memory_manager/svm_memory_manager.h"
#include "unit_tests/mocks/mock_context.h"

using namespace NEO;

TEST(clUnifiedSharedMemoryTests, whenClHostMemAllocINTELisCalledWithoutContextThenInvalidContextIsReturned) {
    cl_int retVal = CL_SUCCESS;
    auto ptr = clHostMemAllocINTEL(0, nullptr, 0, 0, &retVal);
    EXPECT_EQ(nullptr, ptr);
    EXPECT_EQ(CL_INVALID_CONTEXT, retVal);
}

TEST(clUnifiedSharedMemoryTests, whenClHostMemAllocIntelIsCalledThenItAllocatesHostUnifiedMemoryAllocation) {
    MockContext mockContext;
    cl_int retVal = CL_SUCCESS;
    auto unifiedMemoryHostAllocation = clHostMemAllocINTEL(&mockContext, nullptr, 4, 0, &retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);
    ASSERT_NE(nullptr, unifiedMemoryHostAllocation);

    auto allocationsManager = mockContext.getSVMAllocsManager();
    EXPECT_EQ(1u, allocationsManager->getNumAllocs());
    auto graphicsAllocation = allocationsManager->getSVMAlloc(unifiedMemoryHostAllocation);
    EXPECT_EQ(graphicsAllocation->size, 4u);
    EXPECT_EQ(graphicsAllocation->memoryType, InternalMemoryType::HOST_UNIFIED_MEMORY);
    EXPECT_EQ(graphicsAllocation->gpuAllocation->getGpuAddress(), castToUint64(unifiedMemoryHostAllocation));

    retVal = clMemFreeINTEL(&mockContext, unifiedMemoryHostAllocation);
    EXPECT_EQ(CL_SUCCESS, retVal);
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

TEST(clUnifiedSharedMemoryTests, whenClMemFreeINTELisCalledWithIncorrectContextThenReturnError) {
    auto retVal = clMemFreeINTEL(0, nullptr);
    EXPECT_EQ(CL_INVALID_CONTEXT, retVal);
}

TEST(clUnifiedSharedMemoryTests, whenClMemFreeINTELisCalledWithValidUmPointerThenMemoryIsFreed) {
    MockContext mockContext;
    cl_int retVal = CL_SUCCESS;
    auto unifiedMemoryHostAllocation = clHostMemAllocINTEL(&mockContext, nullptr, 4, 0, &retVal);

    auto allocationsManager = mockContext.getSVMAllocsManager();
    EXPECT_EQ(1u, allocationsManager->getNumAllocs());

    retVal = clMemFreeINTEL(&mockContext, unifiedMemoryHostAllocation);
    EXPECT_EQ(CL_SUCCESS, retVal);

    EXPECT_EQ(0u, allocationsManager->getNumAllocs());
}

TEST(clUnifiedSharedMemoryTests, whenClMemFreeINTELisCalledWithInvalidUmPointerThenMemoryIsNotFreed) {
    MockContext mockContext;
    cl_int retVal = CL_SUCCESS;
    auto unifiedMemoryHostAllocation = clHostMemAllocINTEL(&mockContext, nullptr, 4, 0, &retVal);

    auto allocationsManager = mockContext.getSVMAllocsManager();
    EXPECT_EQ(1u, allocationsManager->getNumAllocs());

    retVal = clMemFreeINTEL(&mockContext, ptrOffset(unifiedMemoryHostAllocation, 4));
    EXPECT_EQ(CL_INVALID_VALUE, retVal);

    EXPECT_EQ(1u, allocationsManager->getNumAllocs());
    retVal = clMemFreeINTEL(&mockContext, unifiedMemoryHostAllocation);
    EXPECT_EQ(CL_SUCCESS, retVal);

    EXPECT_EQ(0u, allocationsManager->getNumAllocs());
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
