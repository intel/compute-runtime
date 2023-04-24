/*
 * Copyright (C) 2018-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/test/unit_test/fixtures/device_instrumentation_fixture.h"
#include "opencl/test/unit_test/os_interface/mock_performance_counters.h"

#include "cl_api_tests.h"

using namespace NEO;

struct ClSetPerformanceConfigurationINTELTests : public DeviceInstrumentationFixture,
                                                 public PerformanceCountersDeviceFixture,
                                                 ::testing::Test {
    void SetUp() override {
        PerformanceCountersDeviceFixture::setUp();
        DeviceInstrumentationFixture::setUp(true);
    }
    void TearDown() override {
        PerformanceCountersDeviceFixture::tearDown();
    }
};
namespace ULT {

TEST_F(ClSetPerformanceConfigurationINTELTests, GivenAnyArgumentsWhenSettingPerformanceConfigurationThenInvalidOperationErrorIsReturned) {
    cl_int ret = CL_OUT_OF_RESOURCES;
    cl_uint offsets[2];
    cl_uint values[2];

    ret = clSetPerformanceConfigurationINTEL(device.get(), 2, offsets, values);
    EXPECT_EQ(CL_INVALID_OPERATION, ret);
}

} // namespace ULT
