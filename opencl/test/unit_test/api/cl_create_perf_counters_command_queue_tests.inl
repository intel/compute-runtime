/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/command_queue/command_queue.h"
#include "opencl/source/context/context.h"
#include "opencl/source/event/event.h"
#include "opencl/test/unit_test/fixtures/device_instrumentation_fixture.h"
#include "opencl/test/unit_test/mocks/mock_context.h"
#include "opencl/test/unit_test/os_interface/mock_performance_counters.h"

#include "cl_api_tests.h"

using namespace NEO;

struct clCreatePerfCountersCommandQueueINTELTests : public DeviceInstrumentationFixture,
                                                    public PerformanceCountersDeviceFixture,
                                                    ::testing::Test {
    void SetUp() override {
        PerformanceCountersDeviceFixture::setUp();
        DeviceInstrumentationFixture::setUp(true);

        deviceId = device.get();
        retVal = CL_SUCCESS;
        context = std::unique_ptr<Context>(Context::create<MockContext>(nullptr, ClDeviceVector(&deviceId, 1),
                                                                        nullptr, nullptr, retVal));
    }
    void TearDown() override {
        PerformanceCountersDeviceFixture::tearDown();
    }

    std::unique_ptr<Context> context;
    cl_device_id deviceId;
    cl_int retVal;
};

namespace ULT {

TEST_F(clCreatePerfCountersCommandQueueINTELTests, GivenCorrectParamatersWhenCreatingPerfCountersCmdQThenCmdQIsCreatedAndPerfCountersAreEnabled) {
    cl_command_queue cmdQ = nullptr;
    cl_queue_properties properties = CL_QUEUE_PROFILING_ENABLE;
    cl_uint configuration = 0;

    cmdQ = clCreatePerfCountersCommandQueueINTEL(context.get(), deviceId, properties, configuration, &retVal);
    ASSERT_NE(nullptr, cmdQ);
    ASSERT_EQ(CL_SUCCESS, retVal);

    auto commandQueueObject = castToObject<CommandQueue>(cmdQ);
    EXPECT_TRUE(commandQueueObject->isPerfCountersEnabled());

    retVal = clReleaseCommandQueue(cmdQ);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(clCreatePerfCountersCommandQueueINTELTests, GivenNullPropertiesWhenCreatingPerfCountersCmdQThenInvalidQueuePropertiesErrorIsReturned) {
    cl_command_queue cmdQ = nullptr;
    cl_queue_properties properties = 0;
    cl_uint configuration = 0;

    cmdQ = clCreatePerfCountersCommandQueueINTEL(context.get(), deviceId, properties, configuration, &retVal);

    ASSERT_EQ(nullptr, cmdQ);
    ASSERT_EQ(CL_INVALID_QUEUE_PROPERTIES, retVal);
}

TEST_F(clCreatePerfCountersCommandQueueINTELTests, GivenClQueueOnDevicePropertyWhenCreatingPerfCountersCmdQThenInvalidQueuePropertiesErrorIsReturned) {
    cl_command_queue cmdQ = nullptr;
    cl_queue_properties properties = CL_QUEUE_PROFILING_ENABLE | CL_QUEUE_ON_DEVICE;
    cl_uint configuration = 0;

    cmdQ = clCreatePerfCountersCommandQueueINTEL(context.get(), deviceId, properties, configuration, &retVal);
    ASSERT_EQ(nullptr, cmdQ);
    ASSERT_EQ(CL_INVALID_QUEUE_PROPERTIES, retVal);

    properties = CL_QUEUE_PROFILING_ENABLE | CL_QUEUE_ON_DEVICE_DEFAULT;
    cmdQ = clCreatePerfCountersCommandQueueINTEL(context.get(), deviceId, properties, configuration, &retVal);

    ASSERT_EQ(nullptr, cmdQ);
    ASSERT_EQ(CL_INVALID_QUEUE_PROPERTIES, retVal);
}

TEST_F(clCreatePerfCountersCommandQueueINTELTests, GivenNullContextWhenCreatingPerfCountersCmdQThenInvalidContextErrorIsReturned) {
    cl_command_queue cmdQ = nullptr;
    cl_queue_properties properties = CL_QUEUE_PROFILING_ENABLE;
    cl_uint configuration = 0;

    cmdQ = clCreatePerfCountersCommandQueueINTEL(nullptr, deviceId, properties, configuration, &retVal);

    ASSERT_EQ(nullptr, cmdQ);
    ASSERT_EQ(CL_INVALID_CONTEXT, retVal);
}

TEST_F(clCreatePerfCountersCommandQueueINTELTests, GivenMaximumGtdiConfigurationWhenCreatingPerfCountersCmdQThenOutOfResourcesErrorIsReturned) {
    cl_command_queue cmdQ = nullptr;
    cl_queue_properties properties = CL_QUEUE_PROFILING_ENABLE;
    cl_uint configuration = 4;

    cmdQ = clCreatePerfCountersCommandQueueINTEL(context.get(), deviceId, properties, configuration, &retVal);

    ASSERT_EQ(nullptr, cmdQ);
    ASSERT_EQ(CL_INVALID_OPERATION, retVal);
}

TEST_F(clCreatePerfCountersCommandQueueINTELTests, GivenCorrectCmdQWhenEventIsCreatedThenPerfCountersAreEnabled) {
    cl_command_queue cmdQ = nullptr;
    cl_queue_properties properties = CL_QUEUE_PROFILING_ENABLE;
    cl_uint configuration = 0;

    cmdQ = clCreatePerfCountersCommandQueueINTEL(context.get(), deviceId, properties, configuration, &retVal);
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
    hwInfo->capabilityTable.instrumentationEnabled = false;
    cl_command_queue cmdQ = nullptr;
    cl_queue_properties properties = CL_QUEUE_PROFILING_ENABLE;
    cl_uint configuration = 0;

    cmdQ = clCreatePerfCountersCommandQueueINTEL(context.get(), deviceId, properties, configuration, &retVal);
    ASSERT_EQ(nullptr, cmdQ);
    EXPECT_EQ(CL_INVALID_DEVICE, retVal);
}

TEST_F(clCreatePerfCountersCommandQueueINTELTests, GivenInvalidDeviceWhenCreatingPerfCountersCmdQThenInvalidDeviceErrorIsReturned) {
    cl_command_queue cmdQ = nullptr;
    cl_queue_properties properties = CL_QUEUE_PROFILING_ENABLE;
    cl_uint configuration = 0;

    cmdQ = clCreatePerfCountersCommandQueueINTEL(context.get(), (cl_device_id)context.get(), properties, configuration, &retVal);

    ASSERT_EQ(nullptr, cmdQ);
    ASSERT_EQ(CL_INVALID_DEVICE, retVal);
}

TEST_F(clCreatePerfCountersCommandQueueINTELTests, GivenInvalidMetricsLibraryWhenCreatingPerfCountersThenPerfCountersReturnError) {
    cl_command_queue cmdQ = nullptr;
    cl_queue_properties properties = CL_QUEUE_PROFILING_ENABLE;
    cl_uint configuration = 0;

    cmdQ = clCreatePerfCountersCommandQueueINTEL(context.get(), deviceId, properties, configuration, &retVal);
    auto commandQueueObject = castToObject<CommandQueue>(cmdQ);
    ASSERT_NE(nullptr, cmdQ);
    ASSERT_EQ(CL_SUCCESS, retVal);
    auto performanceCounters = commandQueueObject->getPerfCounters();
    auto metricsLibary = static_cast<MockMetricsLibrary *>(performanceCounters->getMetricsLibraryInterface());
    metricsLibary->validOpen = false;
    ASSERT_NE(nullptr, metricsLibary);
    EXPECT_TRUE(commandQueueObject->isPerfCountersEnabled());

    retVal = clReleaseCommandQueue(cmdQ);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(clCreatePerfCountersCommandQueueINTELTests, givenInvalidMetricsLibraryWhenCreatingCommandQueueThenReturnError) {
    cl_command_queue cmdQ = nullptr;
    cl_queue_properties properties = CL_QUEUE_PROFILING_ENABLE;
    cl_uint configuration = 0;

    auto performanceCounters = device->getPerformanceCounters();

    auto metricsLibary = static_cast<MockMetricsLibrary *>(performanceCounters->getMetricsLibraryInterface());
    metricsLibary->validOpen = false;

    cmdQ = clCreatePerfCountersCommandQueueINTEL(context.get(), deviceId, properties, configuration, &retVal);
    EXPECT_EQ(nullptr, cmdQ);
    EXPECT_EQ(CL_OUT_OF_RESOURCES, retVal);
}

struct clCreateCommandQueueWithPropertiesMdapiTests : public clCreatePerfCountersCommandQueueINTELTests {
    cl_queue_properties queueProperties[7] = {CL_QUEUE_PROPERTIES, CL_QUEUE_PROFILING_ENABLE,
                                              CL_QUEUE_MDAPI_PROPERTIES_INTEL, CL_QUEUE_MDAPI_ENABLE_INTEL,
                                              CL_QUEUE_MDAPI_CONFIGURATION_INTEL, 0,
                                              0};
};

TEST_F(clCreateCommandQueueWithPropertiesMdapiTests, givenCorrectParamsWhenCreatingQueueWithPropertiesThenEnablePerfCounters) {
    auto cmdQ = clCreateCommandQueueWithProperties(context.get(), deviceId, queueProperties, &retVal);

    ASSERT_NE(nullptr, cmdQ);
    EXPECT_EQ(CL_SUCCESS, retVal);

    auto commandQueueObject = castToObject<CommandQueue>(cmdQ);
    EXPECT_TRUE(commandQueueObject->isPerfCountersEnabled());

    clReleaseCommandQueue(cmdQ);
}

TEST_F(clCreateCommandQueueWithPropertiesMdapiTests, givenParamsWithDisabledPerfCounterWhenCreatingQueueWithPropertiesThenCreateRegularQueue) {
    queueProperties[3] = 0;
    auto cmdQ = clCreateCommandQueueWithProperties(context.get(), deviceId, queueProperties, &retVal);

    ASSERT_NE(nullptr, cmdQ);
    EXPECT_EQ(CL_SUCCESS, retVal);

    auto commandQueueObject = castToObject<CommandQueue>(cmdQ);
    EXPECT_FALSE(commandQueueObject->isPerfCountersEnabled());

    clReleaseCommandQueue(cmdQ);
}

TEST_F(clCreateCommandQueueWithPropertiesMdapiTests, givenIncorrectConfigurationWhenCreatingQueueWithPropertiesThenFail) {
    queueProperties[5] = 1;

    auto cmdQ = clCreateCommandQueueWithProperties(context.get(), deviceId, queueProperties, &retVal);

    EXPECT_EQ(nullptr, cmdQ);
    EXPECT_NE(CL_SUCCESS, retVal);
}

TEST_F(clCreateCommandQueueWithPropertiesMdapiTests, givenInvalidMdapiOpenWhenCreatingQueueWithPropertiesThenFail) {
    auto performanceCounters = device->getPerformanceCounters();

    auto metricsLibary = static_cast<MockMetricsLibrary *>(performanceCounters->getMetricsLibraryInterface());
    metricsLibary->validOpen = false;

    auto cmdQ = clCreateCommandQueueWithProperties(context.get(), deviceId, queueProperties, &retVal);

    EXPECT_EQ(nullptr, cmdQ);
    EXPECT_NE(CL_SUCCESS, retVal);
}

} // namespace ULT
