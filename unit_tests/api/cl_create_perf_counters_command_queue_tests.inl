/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/command_queue/command_queue.h"
#include "runtime/context/context.h"
#include "runtime/event/event.h"
#include "unit_tests/fixtures/device_instrumentation_fixture.h"
#include "unit_tests/mocks/mock_context.h"
#include "unit_tests/os_interface/mock_performance_counters.h"

#include "cl_api_tests.h"

using namespace OCLRT;

struct clCreatePerfCountersCommandQueueINTELTests : public DeviceInstrumentationFixture,
                                                    public PerformanceCountersDeviceFixture,
                                                    ::testing::Test {
    void SetUp() override {
        PerformanceCountersDeviceFixture::SetUp();
        DeviceInstrumentationFixture::SetUp(true);

        clDevice = device.get();
        retVal = CL_SUCCESS;
        context = std::unique_ptr<Context>(Context::create<MockContext>(nullptr, DeviceVector(&clDevice, 1),
                                                                        nullptr, nullptr, retVal));
    }
    void TearDown() override {
        PerformanceCountersDeviceFixture::TearDown();
    }

    std::unique_ptr<Context> context;
    cl_device_id clDevice;
    cl_int retVal;
};

namespace ULT {

TEST_F(clCreatePerfCountersCommandQueueINTELTests, GivenCorrectParamatersWhenCreatingPerfCountersCmdQThenCmdQIsCreatedAndPerfCountersAreEnabled) {
    cl_command_queue cmdQ = nullptr;
    cl_queue_properties properties = CL_QUEUE_PROFILING_ENABLE;
    cl_uint configuration = 1;

    cmdQ = clCreatePerfCountersCommandQueueINTEL(context.get(), clDevice, properties, configuration, &retVal);
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

    cmdQ = clCreatePerfCountersCommandQueueINTEL(context.get(), clDevice, properties, configuration, &retVal);

    ASSERT_EQ(nullptr, cmdQ);
    ASSERT_EQ(CL_INVALID_QUEUE_PROPERTIES, retVal);
}

TEST_F(clCreatePerfCountersCommandQueueINTELTests, GivenClQueueOnDevicePropertyWhenCreatingPerfCountersCmdQThenInvalidQueuePropertiesErrorIsReturned) {
    cl_command_queue cmdQ = nullptr;
    cl_queue_properties properties = CL_QUEUE_PROFILING_ENABLE | CL_QUEUE_ON_DEVICE;
    cl_uint configuration = 0;

    cmdQ = clCreatePerfCountersCommandQueueINTEL(context.get(), clDevice, properties, configuration, &retVal);
    ASSERT_EQ(nullptr, cmdQ);
    ASSERT_EQ(CL_INVALID_QUEUE_PROPERTIES, retVal);

    properties = CL_QUEUE_PROFILING_ENABLE | CL_QUEUE_ON_DEVICE_DEFAULT;
    cmdQ = clCreatePerfCountersCommandQueueINTEL(context.get(), clDevice, properties, configuration, &retVal);

    ASSERT_EQ(nullptr, cmdQ);
    ASSERT_EQ(CL_INVALID_QUEUE_PROPERTIES, retVal);
}

TEST_F(clCreatePerfCountersCommandQueueINTELTests, GivenNullContextWhenCreatingPerfCountersCmdQThenInvalidContextErrorIsReturned) {
    cl_command_queue cmdQ = nullptr;
    cl_queue_properties properties = CL_QUEUE_PROFILING_ENABLE;
    cl_uint configuration = 0;

    cmdQ = clCreatePerfCountersCommandQueueINTEL(nullptr, clDevice, properties, configuration, &retVal);

    ASSERT_EQ(nullptr, cmdQ);
    ASSERT_EQ(CL_INVALID_CONTEXT, retVal);
}

TEST_F(clCreatePerfCountersCommandQueueINTELTests, GivenMaximumGtdiConfigurationWhenCreatingPerfCountersCmdQThenOutOfResourcesErrorIsReturned) {
    cl_command_queue cmdQ = nullptr;
    cl_queue_properties properties = CL_QUEUE_PROFILING_ENABLE;
    cl_uint configuration = GTDI_CONFIGURATION_SET_MAX;

    cmdQ = clCreatePerfCountersCommandQueueINTEL(context.get(), clDevice, properties, configuration, &retVal);

    ASSERT_EQ(nullptr, cmdQ);
    ASSERT_EQ(CL_OUT_OF_RESOURCES, retVal);
}

TEST_F(clCreatePerfCountersCommandQueueINTELTests, GivenCorrectCmdQWhenEventIsCreatedThenPerfCountersAreEnabled) {
    cl_command_queue cmdQ = nullptr;
    cl_queue_properties properties = CL_QUEUE_PROFILING_ENABLE;
    cl_uint configuration = 1;

    cmdQ = clCreatePerfCountersCommandQueueINTEL(context.get(), clDevice, properties, configuration, &retVal);
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
    cl_uint configuration = 1;

    cmdQ = clCreatePerfCountersCommandQueueINTEL(context.get(), clDevice, properties, configuration, &retVal);
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

} // namespace ULT
