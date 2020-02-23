/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/test/unit_test/fixtures/device_instrumentation_fixture.h"
#include "opencl/test/unit_test/os_interface/mock_performance_counters.h"

#include "cl_api_tests.h"

using namespace NEO;

struct clSetPerformanceConfigurationINTELTests : public DeviceInstrumentationFixture,
                                                 public PerformanceCountersDeviceFixture,
                                                 ::testing::Test {
    void SetUp() override {
        PerformanceCountersDeviceFixture::SetUp();
        DeviceInstrumentationFixture::SetUp(true);
    }
    void TearDown() override {
        PerformanceCountersDeviceFixture::TearDown();
    }
};
namespace ULT {

TEST_F(clSetPerformanceConfigurationINTELTests, GivenAnyArgumentsWhenSettingPerformanceConfigurationThenInvalidOperationErrorIsReturned) {
    cl_int ret = CL_OUT_OF_RESOURCES;
    cl_uint offsets[2];
    cl_uint values[2];

    ret = clSetPerformanceConfigurationINTEL(device.get(), 2, offsets, values);
    EXPECT_EQ(CL_INVALID_OPERATION, ret);
}

} // namespace ULT
