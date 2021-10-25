/*
 * Copyright (C) 2019-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/mocks/mock_csr.h"

#include "opencl/extensions/public/cl_ext_private.h"
#include "opencl/test/unit_test/api/cl_api_tests.h"
#include "test.h"

using namespace NEO;

TEST(CheckVerifyMemoryRelatedApiConstants, givenVerifyMemoryRelatedApiConstantsWhenVerifyingTheirValueThenCorrectValuesAreReturned) {
    EXPECT_EQ(AubMemDump::CmdServicesMemTraceMemoryCompare::CompareOperationValues::CompareEqual, CL_MEM_COMPARE_EQUAL);
    EXPECT_EQ(AubMemDump::CmdServicesMemTraceMemoryCompare::CompareOperationValues::CompareNotEqual, CL_MEM_COMPARE_NOT_EQUAL);
}

struct clEnqueueVerifyMemoryINTELSettings {
    const cl_uint comparisonMode = CL_MEM_COMPARE_EQUAL;
    const size_t bufferSize = 1;
    static constexpr size_t expectedSize = 1;
    int expected[expectedSize]{};
    void *gpuAddress = expected;
};

class clEnqueueVerifyMemoryINTELTests : public api_tests,
                                        public clEnqueueVerifyMemoryINTELSettings {
};

TEST_F(clEnqueueVerifyMemoryINTELTests, givenSizeOfComparisonEqualZeroWhenCallingVerifyMemoryThenErrorIsReturned) {
    cl_int retval = clEnqueueVerifyMemoryINTEL(nullptr, nullptr, nullptr, 0, comparisonMode);
    EXPECT_EQ(CL_INVALID_VALUE, retval);
}

TEST_F(clEnqueueVerifyMemoryINTELTests, givenNullExpectedDataWhenCallingVerifyMemoryThenErrorIsReturned) {
    cl_int retval = clEnqueueVerifyMemoryINTEL(nullptr, nullptr, nullptr, expectedSize, comparisonMode);
    EXPECT_EQ(CL_INVALID_VALUE, retval);
}

TEST_F(clEnqueueVerifyMemoryINTELTests, givenInvalidAllocationPointerWhenCallingVerifyMemoryThenErrorIsReturned) {
    cl_int retval = clEnqueueVerifyMemoryINTEL(nullptr, nullptr, expected, expectedSize, comparisonMode);
    EXPECT_EQ(CL_INVALID_VALUE, retval);
}

TEST_F(clEnqueueVerifyMemoryINTELTests, givenInvalidCommandQueueWhenCallingVerifyMemoryThenErrorIsReturned) {
    cl_int retval = clEnqueueVerifyMemoryINTEL(nullptr, gpuAddress, expected, expectedSize, comparisonMode);
    EXPECT_EQ(CL_INVALID_COMMAND_QUEUE, retval);
}

TEST_F(clEnqueueVerifyMemoryINTELTests, givenEqualMemoryWhenCallingVerifyMemoryThenSuccessIsReturned) {
    cl_int retval = clEnqueueVerifyMemoryINTEL(pCommandQueue, gpuAddress, expected, expectedSize, comparisonMode);
    EXPECT_EQ(CL_SUCCESS, retval);
}

TEST_F(clEnqueueVerifyMemoryINTELTests, givenNotEqualMemoryWhenCallingVerifyMemoryThenInvalidValueErrorIsReturned) {
    int differentMemory = expected[0] + 1;
    cl_int retval = clEnqueueVerifyMemoryINTEL(pCommandQueue, gpuAddress, &differentMemory, sizeof(differentMemory), comparisonMode);
    EXPECT_EQ(CL_INVALID_VALUE, retval);
}
