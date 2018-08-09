/*
* Copyright (c) 2017 - 2018, Intel Corporation
*
* Permission is hereby granted, free of charge, to any person obtaining a
* copy of this software and associated documentation files (the "Software"),
* to deal in the Software without restriction, including without limitation
* the rights to use, copy, modify, merge, publish, distribute, sublicense,
* and/or sell copies of the Software, and to permit persons to whom the
* Software is furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included
* in all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
* OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
* THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
* OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
* ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
* OTHER DEALINGS IN THE SOFTWARE.
*/

#include "cl_api_tests.h"
#include "runtime/context/context.h"
#include "runtime/command_queue/command_queue.h"
#include "unit_tests/os_interface/mock_performance_counters.h"

using namespace OCLRT;

struct clCreatePerfCountersCommandQueueINTELTests : public api_fixture,
                                                    public PerformanceCountersDeviceFixture,
                                                    ::testing::Test {
    void SetUp() override {
        PerformanceCountersDeviceFixture::SetUp();
        api_fixture::SetUp();

        const HardwareInfo &hwInfo = pPlatform->getDevice(0)->getHardwareInfo();
        pInHwInfo = const_cast<HardwareInfo *>(&hwInfo);
        instrumentationEnabled = pInHwInfo->capabilityTable.instrumentationEnabled;
        pInHwInfo->capabilityTable.instrumentationEnabled = true;
    }
    void TearDown() override {
        pInHwInfo->capabilityTable.instrumentationEnabled = instrumentationEnabled;

        api_fixture::TearDown();
        PerformanceCountersDeviceFixture::TearDown();
    }

    HardwareInfo *pInHwInfo;
    bool instrumentationEnabled;
};

namespace ULT {

TEST_F(clCreatePerfCountersCommandQueueINTELTests, returnsSuccess) {
    cl_command_queue cmdQ = nullptr;
    cl_queue_properties properties = CL_QUEUE_PROFILING_ENABLE;
    cl_uint configuration = 1;

    cmdQ = clCreatePerfCountersCommandQueueINTEL(pContext, devices[0], properties, configuration, &retVal);
    ASSERT_NE(nullptr, cmdQ);
    ASSERT_EQ(CL_SUCCESS, retVal);

    auto commandQueueObject = castToObject<CommandQueue>(cmdQ);
    EXPECT_TRUE(commandQueueObject->isPerfCountersEnabled());

    commandQueueObject->setPerfCountersEnabled(false, 0);
    EXPECT_FALSE(commandQueueObject->isPerfCountersEnabled());

    retVal = clReleaseCommandQueue(cmdQ);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(clCreatePerfCountersCommandQueueINTELTests, negativeNoProfiling) {
    cl_command_queue cmdQ = nullptr;
    cl_queue_properties properties = 0;
    cl_uint configuration = 0;

    cmdQ = clCreatePerfCountersCommandQueueINTEL(pContext, devices[0], properties, configuration, &retVal);

    ASSERT_EQ(nullptr, cmdQ);
    ASSERT_EQ(CL_INVALID_QUEUE_PROPERTIES, retVal);
}

TEST_F(clCreatePerfCountersCommandQueueINTELTests, negativeProfilingDeviceQueue) {
    cl_command_queue cmdQ = nullptr;
    cl_queue_properties properties = CL_QUEUE_PROFILING_ENABLE | CL_QUEUE_ON_DEVICE;
    cl_uint configuration = 0;

    cmdQ = clCreatePerfCountersCommandQueueINTEL(pContext, devices[0], properties, configuration, &retVal);
    ASSERT_EQ(nullptr, cmdQ);
    ASSERT_EQ(CL_INVALID_QUEUE_PROPERTIES, retVal);

    properties = CL_QUEUE_PROFILING_ENABLE | CL_QUEUE_ON_DEVICE_DEFAULT;
    cmdQ = clCreatePerfCountersCommandQueueINTEL(pContext, devices[0], properties, configuration, &retVal);

    ASSERT_EQ(nullptr, cmdQ);
    ASSERT_EQ(CL_INVALID_QUEUE_PROPERTIES, retVal);
}

TEST_F(clCreatePerfCountersCommandQueueINTELTests, negativeInvalidCtx) {
    cl_command_queue cmdQ = nullptr;
    cl_queue_properties properties = CL_QUEUE_PROFILING_ENABLE;
    cl_uint configuration = 0;

    cmdQ = clCreatePerfCountersCommandQueueINTEL(nullptr, devices[0], properties, configuration, &retVal);

    ASSERT_EQ(nullptr, cmdQ);
    ASSERT_EQ(CL_INVALID_CONTEXT, retVal);
}

TEST_F(clCreatePerfCountersCommandQueueINTELTests, negativeInvalidConfig) {
    cl_command_queue cmdQ = nullptr;
    cl_queue_properties properties = CL_QUEUE_PROFILING_ENABLE;
    cl_uint configuration = GTDI_CONFIGURATION_SET_MAX;

    cmdQ = clCreatePerfCountersCommandQueueINTEL(pContext, devices[0], properties, configuration, &retVal);

    ASSERT_EQ(nullptr, cmdQ);
    ASSERT_EQ(CL_OUT_OF_RESOURCES, retVal);
}

TEST_F(clCreatePerfCountersCommandQueueINTELTests, returnsSuccessEvent) {
    cl_command_queue cmdQ = nullptr;
    cl_queue_properties properties = CL_QUEUE_PROFILING_ENABLE;
    cl_uint configuration = 1;

    cmdQ = clCreatePerfCountersCommandQueueINTEL(pContext, devices[0], properties, configuration, &retVal);
    ASSERT_NE(nullptr, cmdQ);
    ASSERT_EQ(CL_SUCCESS, retVal);

    auto commandQueueObject = castToObject<CommandQueue>(cmdQ);
    EXPECT_TRUE(commandQueueObject->isPerfCountersEnabled());

    Event event(commandQueueObject, CL_COMMAND_NDRANGE_KERNEL, 1, 5);
    EXPECT_TRUE(event.isPerfCountersEnabled());

    retVal = clReleaseCommandQueue(cmdQ);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(clCreatePerfCountersCommandQueueINTELTests, negativeNoInstrumentation) {
    pInHwInfo->capabilityTable.instrumentationEnabled = false;
    cl_command_queue cmdQ = nullptr;
    cl_queue_properties properties = CL_QUEUE_PROFILING_ENABLE;
    cl_uint configuration = 1;

    cmdQ = clCreatePerfCountersCommandQueueINTEL(pContext, devices[0], properties, configuration, &retVal);
    ASSERT_EQ(nullptr, cmdQ);
    EXPECT_EQ(CL_INVALID_DEVICE, retVal);
}

TEST_F(clCreatePerfCountersCommandQueueINTELTests, negativeInvalidDevice) {
    cl_command_queue cmdQ = nullptr;
    cl_queue_properties properties = CL_QUEUE_PROFILING_ENABLE;
    cl_uint configuration = 0;

    cmdQ = clCreatePerfCountersCommandQueueINTEL(pContext, (cl_device_id)pContext, properties, configuration, &retVal);

    ASSERT_EQ(nullptr, cmdQ);
    ASSERT_EQ(CL_INVALID_DEVICE, retVal);
}

} // namespace ULT
