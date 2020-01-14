/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "unit_tests/fixtures/device_fixture.h"
#include "unit_tests/mocks/mock_ostime.h"

#include "cl_api_tests.h"

using namespace NEO;

struct FailOSTime : public MockOSTime {
  public:
    bool getCpuGpuTime(TimeStampData *pGpuCpuTime) override {
        return false;
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
        devices[testedRootDeviceIndex],
        &device_timestamp,
        nullptr);

    EXPECT_EQ(CL_INVALID_VALUE, retVal);
}

TEST_F(clGetDeviceAndHostTimerTest, GivenNullDevicesTimerWhenGettingDeviceAndHostTimerThenInvalidValueErrorIsReturned) {
    cl_ulong host_timestamp = 0;

    retVal = clGetDeviceAndHostTimer(
        devices[testedRootDeviceIndex],
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
        devices[testedRootDeviceIndex],
        nullptr);

    EXPECT_EQ(CL_INVALID_VALUE, retVal);
}

TEST_F(clGetHostTimerTest, GivenCorrectParametersWhenGettingHostTimerThenSuccessIsReturned) {
    cl_ulong host_timestamp = 0;
    cl_ulong zero_timestamp = 0;

    retVal = clGetHostTimer(
        devices[testedRootDeviceIndex],
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
