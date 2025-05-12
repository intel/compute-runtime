/*
 * Copyright (C) 2019-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/mocks/mock_csr.h"
#include "shared/test/common/mocks/mock_os_context.h"
#include "shared/test/common/test_macros/test.h"

#include "opencl/extensions/public/cl_ext_private.h"
#include "opencl/test/unit_test/api/cl_api_tests.h"

using namespace NEO;
#include "aub_services.h"

TEST(CheckVerifyMemoryRelatedApiConstants, givenVerifyMemoryRelatedApiConstantsWhenVerifyingTheirValueThenCorrectValuesAreReturned) {
    EXPECT_EQ(AubMemDump::CmdServicesMemTraceMemoryCompare::CompareOperationValues::CompareEqual, CL_MEM_COMPARE_EQUAL);
    EXPECT_EQ(AubMemDump::CmdServicesMemTraceMemoryCompare::CompareOperationValues::CompareNotEqual, CL_MEM_COMPARE_NOT_EQUAL);
}

struct ClEnqueueVerifyMemoryINTELSettings {
    const cl_uint comparisonMode = CL_MEM_COMPARE_EQUAL;
    const size_t bufferSize = 1;
    static constexpr size_t expectedSize = 1;
    int expected[expectedSize]{};
    void *gpuAddress = expected;
};

class ClEnqueueVerifyMemoryIntelTests : public ApiTests,
                                        public ClEnqueueVerifyMemoryINTELSettings {
};

TEST_F(ClEnqueueVerifyMemoryIntelTests, givenSizeOfComparisonEqualZeroWhenCallingVerifyMemoryThenErrorIsReturned) {
    cl_int retval = clEnqueueVerifyMemoryINTEL(nullptr, nullptr, nullptr, 0, comparisonMode);
    EXPECT_EQ(CL_INVALID_VALUE, retval);
}

TEST_F(ClEnqueueVerifyMemoryIntelTests, givenNullExpectedDataWhenCallingVerifyMemoryThenErrorIsReturned) {
    cl_int retval = clEnqueueVerifyMemoryINTEL(nullptr, nullptr, nullptr, expectedSize, comparisonMode);
    EXPECT_EQ(CL_INVALID_VALUE, retval);
}

TEST_F(ClEnqueueVerifyMemoryIntelTests, givenInvalidAllocationPointerWhenCallingVerifyMemoryThenErrorIsReturned) {
    cl_int retval = clEnqueueVerifyMemoryINTEL(nullptr, nullptr, expected, expectedSize, comparisonMode);
    EXPECT_EQ(CL_INVALID_VALUE, retval);
}

TEST_F(ClEnqueueVerifyMemoryIntelTests, givenInvalidCommandQueueWhenCallingVerifyMemoryThenErrorIsReturned) {
    cl_int retval = clEnqueueVerifyMemoryINTEL(nullptr, gpuAddress, expected, expectedSize, comparisonMode);
    EXPECT_EQ(CL_INVALID_COMMAND_QUEUE, retval);
}

TEST_F(ClEnqueueVerifyMemoryIntelTests, givenEqualMemoryWhenCallingVerifyMemoryThenSuccessIsReturned) {
    cl_int retval = clEnqueueVerifyMemoryINTEL(pCommandQueue, gpuAddress, expected, expectedSize, comparisonMode);
    EXPECT_EQ(CL_SUCCESS, retval);
}

TEST_F(ClEnqueueVerifyMemoryIntelTests, givenNotEqualMemoryWhenCallingVerifyMemoryThenInvalidValueErrorIsReturned) {
    int differentMemory = expected[0] + 1;
    cl_int retval = clEnqueueVerifyMemoryINTEL(pCommandQueue, gpuAddress, &differentMemory, sizeof(differentMemory), comparisonMode);
    EXPECT_EQ(CL_INVALID_VALUE, retval);
}

HWTEST_F(ClEnqueueVerifyMemoryIntelTests, givenActiveBcsEngineWhenCallingExpectMemoryThenPollForCompletionOnAllEngines) {
    UltCommandStreamReceiver<FamilyType> ultCsrBcs0(*pDevice->getExecutionEnvironment(), pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    UltCommandStreamReceiver<FamilyType> ultCsrBcs1(*pDevice->getExecutionEnvironment(), pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());

    MockOsContext osContext0(0, {{aub_stream::ENGINE_BCS, EngineUsage::regular}, 0, PreemptionMode::Disabled, false});
    MockOsContext osContext1(1, {{aub_stream::ENGINE_BCS1, EngineUsage::regular}, 0, PreemptionMode::Disabled, false});

    EngineControl engineControl[2] = {{&ultCsrBcs0, &osContext0}, {&ultCsrBcs1, &osContext1}};

    pCommandQueue->bcsInitialized = true;
    pCommandQueue->bcsEngines[EngineHelpers::getBcsIndex(aub_stream::ENGINE_BCS)] = &engineControl[0];
    pCommandQueue->bcsEngines[EngineHelpers::getBcsIndex(aub_stream::ENGINE_BCS1)] = &engineControl[1];

    pCommandQueue->bcsStates[0].engineType = aub_stream::ENGINE_BCS;
    pCommandQueue->bcsStates[1].engineType = aub_stream::ENGINE_BCS1;

    EXPECT_EQ(0u, ultCsrBcs0.pollForCompletionCalled);
    EXPECT_EQ(0u, ultCsrBcs1.pollForCompletionCalled);

    cl_int retval = clEnqueueVerifyMemoryINTEL(pCommandQueue, gpuAddress, expected, expectedSize, comparisonMode);
    EXPECT_EQ(CL_SUCCESS, retval);

    EXPECT_EQ(1u, ultCsrBcs0.pollForCompletionCalled);
    EXPECT_EQ(1u, ultCsrBcs1.pollForCompletionCalled);

    pCommandQueue->bcsEngines[EngineHelpers::getBcsIndex(aub_stream::ENGINE_BCS)]->commandStreamReceiver = nullptr;
    pCommandQueue->bcsEngines[EngineHelpers::getBcsIndex(aub_stream::ENGINE_BCS1)]->commandStreamReceiver = nullptr;

    pCommandQueue->bcsStates[0].engineType = aub_stream::NUM_ENGINES;
    pCommandQueue->bcsStates[1].engineType = aub_stream::NUM_ENGINES;

    pCommandQueue->bcsEngines[EngineHelpers::getBcsIndex(aub_stream::ENGINE_BCS)] = nullptr;
    pCommandQueue->bcsEngines[EngineHelpers::getBcsIndex(aub_stream::ENGINE_BCS1)] = nullptr;
}
