/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/mocks/mock_ostime.h"

#include "opencl/test/unit_test/fixtures/cl_device_fixture.h"

#include "cl_api_tests.h"

using namespace NEO;

struct FailDeviceTime : public MockDeviceTime {
    bool getCpuGpuTime(TimeStampData *pGpuCpuTime, OSTime *) override {
        return false;
    }
};

struct FailOSTime : public MockOSTime {
  public:
    FailOSTime() {
        this->deviceTime = std::make_unique<FailDeviceTime>();
    }

    bool getCpuTime(uint64_t *timeStamp) override {
        return false;
    };
};

typedef api_tests clGetDeviceAndHostTimerTest;
typedef api_tests clGetHostTimerTest;

namespace ULT {

TEST_F(clGetDeviceAndHostTimerTest, GivenNullDeviceWhenGettingDeviceAndHostTimerThenInvalidDeviceErrorIsReturned) {
    cl_ulong device_timestamp = 0;
    cl_ulong host_timestamp = 0;

    retVal = clGetDeviceAndHostTimer(
        nullptr,
        &device_timestamp,
        &host_timestamp);

    EXPECT_EQ(CL_INVALID_DEVICE, retVal);
}

TEST_F(clGetDeviceAndHostTimerTest, GivenNullHostTimerWhenGettingDeviceAndHostTimerThenInvalidValueErrorIsReturned) {
    cl_ulong device_timestamp = 0;

    retVal = clGetDeviceAndHostTimer(
        testedClDevice,
        &device_timestamp,
        nullptr);

    EXPECT_EQ(CL_INVALID_VALUE, retVal);
}

TEST_F(clGetDeviceAndHostTimerTest, GivenNullDevicesTimerWhenGettingDeviceAndHostTimerThenInvalidValueErrorIsReturned) {
    cl_ulong host_timestamp = 0;

    retVal = clGetDeviceAndHostTimer(
        testedClDevice,
        nullptr,
        &host_timestamp);

    EXPECT_EQ(CL_INVALID_VALUE, retVal);
}

TEST_F(clGetDeviceAndHostTimerTest, GivenValidOSTimeWhenGettingDeviceAndHostTimerThenSuccessIsReturned) {
    cl_ulong device_timestamp = 0;
    cl_ulong host_timestamp = 0;
    cl_ulong zero_timestamp = 0;

    auto mDev = new MockClDevice{MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr)};
    mDev->setOSTime(new MockOSTime());

    retVal = clGetDeviceAndHostTimer(
        mDev,
        &device_timestamp,
        &host_timestamp);

    EXPECT_GT(device_timestamp, zero_timestamp);
    EXPECT_GT(host_timestamp, zero_timestamp);
    EXPECT_EQ(retVal, CL_SUCCESS);

    delete mDev;
}

TEST_F(clGetDeviceAndHostTimerTest, GivenInvalidOSTimeWhenGettingDeviceAndHostTimerThenOutOfResourcesErrorIsReturned) {
    cl_ulong device_timestamp = 0;
    cl_ulong host_timestamp = 0;
    cl_ulong zero_timestamp = 0;

    auto mDev = new MockClDevice{MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr)};
    mDev->setOSTime(new FailOSTime());

    retVal = clGetDeviceAndHostTimer(
        mDev,
        &device_timestamp,
        &host_timestamp);

    EXPECT_EQ(device_timestamp, zero_timestamp);
    EXPECT_EQ(host_timestamp, zero_timestamp);
    EXPECT_EQ(retVal, CL_OUT_OF_RESOURCES);

    delete mDev;
}

TEST_F(clGetHostTimerTest, GivenNullDeviceWhenGettingHostTimerThenInvalidDeviceErrorIsReturned) {
    cl_ulong host_timestamp = 0;

    retVal = clGetHostTimer(
        nullptr,
        &host_timestamp);

    EXPECT_EQ(CL_INVALID_DEVICE, retVal);
}

TEST_F(clGetHostTimerTest, GivenNullHostTimerWhenGettingHostTimerThenInvalidValueErrorIsReturned) {
    retVal = clGetHostTimer(
        testedClDevice,
        nullptr);

    EXPECT_EQ(CL_INVALID_VALUE, retVal);
}

TEST_F(clGetHostTimerTest, GivenCorrectParametersWhenGettingHostTimerThenSuccessIsReturned) {
    cl_ulong host_timestamp = 0;
    cl_ulong zero_timestamp = 0;

    retVal = clGetHostTimer(
        testedClDevice,
        &host_timestamp);

    EXPECT_GE(host_timestamp, zero_timestamp);
    EXPECT_EQ(retVal, CL_SUCCESS);
}

TEST_F(clGetHostTimerTest, GivenValidOSTimeWhenGettingHostTimerThenSuccessIsReturned) {
    cl_ulong host_timestamp = 0;
    cl_ulong zero_timestamp = 0;

    auto mDev = new MockClDevice{MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr)};
    mDev->setOSTime(new MockOSTime());

    retVal = clGetHostTimer(
        mDev,
        &host_timestamp);

    EXPECT_GE(host_timestamp, zero_timestamp);
    EXPECT_EQ(retVal, CL_SUCCESS);

    delete mDev;
}

TEST_F(clGetHostTimerTest, GivenInvalidOSTimeWhenGettingHostTimerThenOutOfResourcesErrorIsReturned) {
    cl_ulong host_timestamp = 0;
    cl_ulong zero_timestamp = 0;

    auto mDev = new MockClDevice{MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr)};
    mDev->setOSTime(new FailOSTime());

    retVal = clGetHostTimer(
        mDev,
        &host_timestamp);

    EXPECT_EQ(host_timestamp, zero_timestamp);
    EXPECT_EQ(retVal, CL_OUT_OF_RESOURCES);

    delete mDev;
}

} // namespace ULT
