/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "public/cl_ext_private.h"
#include "runtime/aub_mem_dump/aub_services.h"
#include "test.h"
#include "unit_tests/api/cl_api_tests.h"
#include "unit_tests/mocks/mock_csr.h"

using namespace OCLRT;

TEST(CheckVerifyMemoryRelatedApiConstants, givenVerifyMemoryRelatedApiConstantsWhenVerifyingTheirValueThenCorrectValuesAreReturned) {
    EXPECT_EQ(CmdServicesMemTraceMemoryCompare::CompareOperationValues::CompareEqual, CL_MEM_COMPARE_EQUAL);
    EXPECT_EQ(CmdServicesMemTraceMemoryCompare::CompareOperationValues::CompareNotEqual, CL_MEM_COMPARE_NOT_EQUAL);
}

struct clEnqueueVerifyMemorySettings {
    const cl_uint comparisonMode = CL_MEM_COMPARE_EQUAL;
    const size_t bufferSize = 1;
    static constexpr size_t expectedSize = 1;
    int expected[expectedSize];
    // Use any valid pointer as gpu address because non aub tests will not actually validate the memory
    void *gpuAddress = expected;
};

class clEnqueueVerifyMemoryTests : public api_tests,
                                   public clEnqueueVerifyMemorySettings {
};

TEST_F(clEnqueueVerifyMemoryTests, givenSizeOfComparisonEqualZeroWhenCallingVerifyMemoryThenErrorIsReturned) {
    cl_int retval = clEnqueueVerifyMemory(nullptr, nullptr, nullptr, 0, comparisonMode);
    EXPECT_EQ(CL_INVALID_VALUE, retval);
}

TEST_F(clEnqueueVerifyMemoryTests, givenNullExpectedDataWhenCallingVerifyMemoryThenErrorIsReturned) {
    cl_int retval = clEnqueueVerifyMemory(nullptr, nullptr, nullptr, expectedSize, comparisonMode);
    EXPECT_EQ(CL_INVALID_VALUE, retval);
}

TEST_F(clEnqueueVerifyMemoryTests, givenInvalidAllocationPointerWhenCallingVerifyMemoryThenErrorIsReturned) {
    cl_int retval = clEnqueueVerifyMemory(nullptr, nullptr, expected, expectedSize, comparisonMode);
    EXPECT_EQ(CL_INVALID_VALUE, retval);
}

TEST_F(clEnqueueVerifyMemoryTests, givenInvalidCommandQueueWhenCallingVerifyMemoryThenErrorIsReturned) {
    cl_int retval = clEnqueueVerifyMemory(nullptr, gpuAddress, expected, expectedSize, comparisonMode);
    EXPECT_EQ(CL_INVALID_COMMAND_QUEUE, retval);
}

TEST_F(clEnqueueVerifyMemoryTests, givenCommandQueueWithoutAubCsrWhenCallingVerifyMemoryThenSuccessIsReturned) {
    cl_int retval = clEnqueueVerifyMemory(pCommandQueue, gpuAddress, expected, expectedSize, comparisonMode);
    EXPECT_EQ(CL_SUCCESS, retval);
}
