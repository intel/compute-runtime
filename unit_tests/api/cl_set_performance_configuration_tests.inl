/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "cl_api_tests.h"
#include "unit_tests/os_interface/mock_performance_counters.h"

using namespace OCLRT;

struct clSetPerformanceConfigurationINTELTests : public api_fixture,
                                                 public PerformanceCountersDeviceFixture,
                                                 ::testing::Test {
    void SetUp() override {
        PerformanceCountersDeviceFixture::SetUp();
        api_fixture::SetUp();
    }
    void TearDown() override {
        api_fixture::TearDown();
        PerformanceCountersDeviceFixture::TearDown();
    }
};
namespace ULT {

TEST_F(clSetPerformanceConfigurationINTELTests, positiveSetPerfConfig) {
    cl_int ret = CL_OUT_OF_RESOURCES;
    cl_uint offsets[2];
    cl_uint values[2];

    ret = clSetPerformanceConfigurationINTEL(devices[0], 2, offsets, values);
    EXPECT_EQ(CL_SUCCESS, ret);
}

TEST_F(clSetPerformanceConfigurationINTELTests, negativeInvalidDevice) {
    cl_int ret = CL_OUT_OF_RESOURCES;
    cl_uint offsets[2];
    cl_uint values[2];
    cl_device_id device = {0};

    ret = clSetPerformanceConfigurationINTEL(device, 2, offsets, values);
    EXPECT_EQ(CL_INVALID_DEVICE, ret);
}

TEST_F(clSetPerformanceConfigurationINTELTests, negativeInstrumentationDisabled) {
    cl_int ret = CL_OUT_OF_RESOURCES;
    cl_uint offsets[2];
    cl_uint values[2];

    const HardwareInfo &hwInfo = pPlatform->getDevice(0)->getHardwareInfo();
    HardwareInfo *pInHwInfo = const_cast<HardwareInfo *>(&hwInfo);
    bool instrumentationEnabled = pInHwInfo->capabilityTable.instrumentationEnabled;
    pInHwInfo->capabilityTable.instrumentationEnabled = false;

    ret = clSetPerformanceConfigurationINTEL(devices[0], 2, offsets, values);
    EXPECT_EQ(CL_PROFILING_INFO_NOT_AVAILABLE, ret);

    pInHwInfo->capabilityTable.instrumentationEnabled = instrumentationEnabled;
}

} // namespace ULT
