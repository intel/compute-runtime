/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "unit_tests/fixtures/device_instrumentation_fixture.h"
#include "unit_tests/os_interface/mock_performance_counters.h"

#include "cl_api_tests.h"

using namespace OCLRT;

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

TEST_F(clSetPerformanceConfigurationINTELTests, positiveSetPerfConfig) {
    cl_int ret = CL_OUT_OF_RESOURCES;
    cl_uint offsets[2];
    cl_uint values[2];

    ret = clSetPerformanceConfigurationINTEL(device.get(), 2, offsets, values);
    EXPECT_EQ(CL_SUCCESS, ret);
}

TEST_F(clSetPerformanceConfigurationINTELTests, negativeInvalidDevice) {
    cl_int ret = CL_OUT_OF_RESOURCES;
    cl_uint offsets[2];
    cl_uint values[2];
    cl_device_id clDevice = {0};

    ret = clSetPerformanceConfigurationINTEL(clDevice, 2, offsets, values);
    EXPECT_EQ(CL_INVALID_DEVICE, ret);
}

TEST_F(clSetPerformanceConfigurationINTELTests, negativeInstrumentationDisabled) {
    cl_int ret = CL_OUT_OF_RESOURCES;
    cl_uint offsets[2];
    cl_uint values[2];

    HardwareInfo *pInHwInfo = const_cast<HardwareInfo *>(hwInfo);
    bool instrumentationEnabled = pInHwInfo->capabilityTable.instrumentationEnabled;
    pInHwInfo->capabilityTable.instrumentationEnabled = false;

    ret = clSetPerformanceConfigurationINTEL(device.get(), 2, offsets, values);
    EXPECT_EQ(CL_PROFILING_INFO_NOT_AVAILABLE, ret);

    pInHwInfo->capabilityTable.instrumentationEnabled = instrumentationEnabled;
}

} // namespace ULT
