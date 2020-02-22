/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "cl_api_tests.h"
#include "fixtures/device_instrumentation_fixture.h"
#include "os_interface/mock_performance_counters.h"

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
