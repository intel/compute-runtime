/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
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

TEST_F(clCreatePerfCountersCommandQueueINTELTests, GivenCorrectParamatersWhenCreatingPerfCountersCmdQThenCmdQIsCreatedAndPerfCountersAreEnabled) {
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

TEST_F(clCreatePerfCountersCommandQueueINTELTests, GivenNullPropertiesWhenCreatingPerfCountersCmdQThenInvalidQueuePropertiesErrorIsReturned) {
    cl_command_queue cmdQ = nullptr;
    cl_queue_properties properties = 0;
    cl_uint configuration = 0;

    cmdQ = clCreatePerfCountersCommandQueueINTEL(pContext, devices[0], properties, configuration, &retVal);

    ASSERT_EQ(nullptr, cmdQ);
    ASSERT_EQ(CL_INVALID_QUEUE_PROPERTIES, retVal);
}

TEST_F(clCreatePerfCountersCommandQueueINTELTests, GivenClQueueOnDevicePropertyWhenCreatingPerfCountersCmdQThenInvalidQueuePropertiesErrorIsReturned) {
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

TEST_F(clCreatePerfCountersCommandQueueINTELTests, GivenNullContextWhenCreatingPerfCountersCmdQThenInvalidContextErrorIsReturned) {
    cl_command_queue cmdQ = nullptr;
    cl_queue_properties properties = CL_QUEUE_PROFILING_ENABLE;
    cl_uint configuration = 0;

    cmdQ = clCreatePerfCountersCommandQueueINTEL(nullptr, devices[0], properties, configuration, &retVal);

    ASSERT_EQ(nullptr, cmdQ);
    ASSERT_EQ(CL_INVALID_CONTEXT, retVal);
}

TEST_F(clCreatePerfCountersCommandQueueINTELTests, GivenMaximumGtdiConfigurationWhenCreatingPerfCountersCmdQThenOutOfResourcesErrorIsReturned) {
    cl_command_queue cmdQ = nullptr;
    cl_queue_properties properties = CL_QUEUE_PROFILING_ENABLE;
    cl_uint configuration = GTDI_CONFIGURATION_SET_MAX;

    cmdQ = clCreatePerfCountersCommandQueueINTEL(pContext, devices[0], properties, configuration, &retVal);

    ASSERT_EQ(nullptr, cmdQ);
    ASSERT_EQ(CL_OUT_OF_RESOURCES, retVal);
}

TEST_F(clCreatePerfCountersCommandQueueINTELTests, GivenCorrectCmdQWhenEventIsCreatedThenPerfCountersAreEnabled) {
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

TEST_F(clCreatePerfCountersCommandQueueINTELTests, GivenInstrumentationEnabledIsFalseWhenCreatingPerfCountersCmdQThenInvalidDeviceErrorIsReturned) {
    pInHwInfo->capabilityTable.instrumentationEnabled = false;
    cl_command_queue cmdQ = nullptr;
    cl_queue_properties properties = CL_QUEUE_PROFILING_ENABLE;
    cl_uint configuration = 1;

    cmdQ = clCreatePerfCountersCommandQueueINTEL(pContext, devices[0], properties, configuration, &retVal);
    ASSERT_EQ(nullptr, cmdQ);
    EXPECT_EQ(CL_INVALID_DEVICE, retVal);
}

TEST_F(clCreatePerfCountersCommandQueueINTELTests, GivenInvalidDeviceWhenCreatingPerfCountersCmdQThenInvalidDeviceErrorIsReturned) {
    cl_command_queue cmdQ = nullptr;
    cl_queue_properties properties = CL_QUEUE_PROFILING_ENABLE;
    cl_uint configuration = 0;

    cmdQ = clCreatePerfCountersCommandQueueINTEL(pContext, (cl_device_id)pContext, properties, configuration, &retVal);

    ASSERT_EQ(nullptr, cmdQ);
    ASSERT_EQ(CL_INVALID_DEVICE, retVal);
}

} // namespace ULT
