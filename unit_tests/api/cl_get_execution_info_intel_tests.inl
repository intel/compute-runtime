/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/command_queue/command_queue.h"
#include "unit_tests/mocks/mock_kernel.h"

#include "cl_api_tests.h"

using namespace NEO;

using clGetExecutionInfoTests = api_tests;

namespace ULT {

TEST_F(clGetExecutionInfoTests, GivenInvalidInputWhenCallingGetExecutionInfoThenErrorIsReturned) {
    retVal = clGetExecutionInfoINTEL(nullptr, pKernel, 0, nullptr, nullptr, 0, 0, nullptr, nullptr);
    EXPECT_NE(CL_SUCCESS, retVal);

    retVal = clGetExecutionInfoINTEL(pCommandQueue, nullptr, 0, nullptr, nullptr, 0, 0, nullptr, nullptr);
    EXPECT_NE(CL_SUCCESS, retVal);

    pKernel->isPatchedOverride = false;
    retVal = clGetExecutionInfoINTEL(pCommandQueue, pKernel, 0, nullptr, nullptr, 0, 0, nullptr, nullptr);
    EXPECT_NE(CL_SUCCESS, retVal);
    pKernel->isPatchedOverride = true;

    auto invalidParamName = 0xFFFF;
    retVal = clGetExecutionInfoINTEL(pCommandQueue, pKernel, 0, nullptr, nullptr, invalidParamName, 0, nullptr, nullptr);
    EXPECT_NE(CL_SUCCESS, retVal);

    uint32_t queryResult;
    retVal = clGetExecutionInfoINTEL(pCommandQueue, pKernel, 0, nullptr, nullptr, CL_EXECUTION_INFO_MAX_WORKGROUP_COUNT_INTEL,
                                     sizeof(queryResult), nullptr, nullptr);
    EXPECT_NE(CL_SUCCESS, retVal);

    retVal = clGetExecutionInfoINTEL(pCommandQueue, pKernel, 0, nullptr, nullptr, CL_EXECUTION_INFO_MAX_WORKGROUP_COUNT_INTEL,
                                     0, &queryResult, nullptr);
    EXPECT_NE(CL_SUCCESS, retVal);
}

TEST_F(clGetExecutionInfoTests, GivenVariousInputWhenGettingMaxWorkGroupCountThenCorrectValuesAreReturned) {
    uint32_t queryResult;
    retVal = clGetExecutionInfoINTEL(pCommandQueue, pKernel, 0, nullptr, nullptr, CL_EXECUTION_INFO_MAX_WORKGROUP_COUNT_INTEL,
                                     sizeof(queryResult), &queryResult, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(0u, queryResult);

    uint64_t queryResult64 = 0;
    size_t queryResultSize;
    retVal = clGetExecutionInfoINTEL(pCommandQueue, pKernel, 0, nullptr, nullptr, CL_EXECUTION_INFO_MAX_WORKGROUP_COUNT_INTEL,
                                     sizeof(queryResult64), &queryResult64, &queryResultSize);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(queryResult, queryResult64);
    EXPECT_EQ(sizeof(queryResult), queryResultSize);

    std::unique_ptr<MockKernel> pKernelWithExecutionEnvironmentPatch(MockKernel::create(pCommandQueue->getDevice(), pProgram));
    uint32_t queryResultWithExecutionEnvironment;
    retVal = clGetExecutionInfoINTEL(pCommandQueue, pKernelWithExecutionEnvironmentPatch.get(), 0, nullptr, nullptr,
                                     CL_EXECUTION_INFO_MAX_WORKGROUP_COUNT_INTEL,
                                     sizeof(queryResultWithExecutionEnvironment), &queryResultWithExecutionEnvironment, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(queryResult, queryResultWithExecutionEnvironment);
}

} // namespace ULT
