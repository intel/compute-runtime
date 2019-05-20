/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "unit_tests/fixtures/device_instrumentation_fixture.h"
#include "unit_tests/os_interface/mock_performance_counters.h"

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

TEST_F(clSetPerformanceConfigurationINTELTests, negativeSetPerfConfig) {
    cl_int ret = CL_OUT_OF_RESOURCES;
    cl_uint offsets[2];
    cl_uint values[2];

    ret = clSetPerformanceConfigurationINTEL(device.get(), 2, offsets, values);
    EXPECT_EQ(CL_INVALID_OPERATION, ret);
}

TEST_F(clSetPerformanceConfigurationINTELTests, negativeInvalidDevice) {
    cl_int ret = CL_OUT_OF_RESOURCES;
    cl_uint offsets[2];
    cl_uint values[2];
    cl_device_id clDevice = {0};

    ret = clSetPerformanceConfigurationINTEL(clDevice, 2, offsets, values);
    EXPECT_EQ(CL_INVALID_OPERATION, ret);
}

TEST_F(clSetPerformanceConfigurationINTELTests, negativeInstrumentationDisabled) {
    cl_int ret = CL_OUT_OF_RESOURCES;
    cl_uint offsets[2];
    cl_uint values[2];

    bool instrumentationEnabled = hwInfo->capabilityTable.instrumentationEnabled;
    hwInfo->capabilityTable.instrumentationEnabled = false;

    ret = clSetPerformanceConfigurationINTEL(device.get(), 2, offsets, values);
    EXPECT_EQ(CL_INVALID_OPERATION, ret);

    hwInfo->capabilityTable.instrumentationEnabled = instrumentationEnabled;
}

} // namespace ULT
