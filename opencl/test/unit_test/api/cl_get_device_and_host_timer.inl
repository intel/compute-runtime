/*
 * Copyright (C) 2018-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/mocks/mock_ostime.h"

#include "opencl/test/unit_test/fixtures/cl_device_fixture.h"

#include "cl_api_tests.h"

using namespace NEO;

struct FailDeviceTime : public MockDeviceTime {
    TimeQueryStatus getGpuCpuTimeImpl(TimeStampData *pGpuCpuTime, OSTime *) override {
        return TimeQueryStatus::deviceLost;
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

using ClGetDeviceAndHostTimerTest = ApiTests;
using ClGetHostTimerTest = ApiTests;

namespace ULT {

TEST_F(ClGetDeviceAndHostTimerTest, GivenNullDeviceWhenGettingDeviceAndHostTimerThenInvalidDeviceErrorIsReturned) {
    cl_ulong deviceTimestamp = 0;
    cl_ulong hostTimestamp = 0;

    retVal = clGetDeviceAndHostTimer(
        nullptr,
        &deviceTimestamp,
        &hostTimestamp);

    EXPECT_EQ(CL_INVALID_DEVICE, retVal);
}

TEST_F(ClGetDeviceAndHostTimerTest, GivenNullHostTimerWhenGettingDeviceAndHostTimerThenInvalidValueErrorIsReturned) {
    cl_ulong deviceTimestamp = 0;

    retVal = clGetDeviceAndHostTimer(
        testedClDevice,
        &deviceTimestamp,
        nullptr);

    EXPECT_EQ(CL_INVALID_VALUE, retVal);
}

TEST_F(ClGetDeviceAndHostTimerTest, GivenNullDevicesTimerWhenGettingDeviceAndHostTimerThenInvalidValueErrorIsReturned) {
    cl_ulong hostTimestamp = 0;

    retVal = clGetDeviceAndHostTimer(
        testedClDevice,
        nullptr,
        &hostTimestamp);

    EXPECT_EQ(CL_INVALID_VALUE, retVal);
}

TEST_F(ClGetDeviceAndHostTimerTest, GivenValidOSTimeWhenGettingDeviceAndHostTimerThenSuccessIsReturned) {
    cl_ulong deviceTimestamp = 0;
    cl_ulong hostTimestamp = 0;
    cl_ulong zeroTimestamp = 0;

    auto mDev = new MockClDevice{MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr)};
    mDev->setOSTime(new MockOSTime());

    retVal = clGetDeviceAndHostTimer(
        mDev,
        &deviceTimestamp,
        &hostTimestamp);

    EXPECT_GT(deviceTimestamp, zeroTimestamp);
    EXPECT_GT(hostTimestamp, zeroTimestamp);
    EXPECT_EQ(retVal, CL_SUCCESS);

    delete mDev;
}

TEST_F(ClGetDeviceAndHostTimerTest, GivenInvalidOSTimeWhenGettingDeviceAndHostTimerThenOutOfResourcesErrorIsReturned) {
    cl_ulong deviceTimestamp = 0;
    cl_ulong hostTimestamp = 0;
    cl_ulong zeroTimestamp = 0;

    auto mDev = new MockClDevice{MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr)};
    mDev->setOSTime(new FailOSTime());

    retVal = clGetDeviceAndHostTimer(
        mDev,
        &deviceTimestamp,
        &hostTimestamp);

    EXPECT_EQ(deviceTimestamp, zeroTimestamp);
    EXPECT_EQ(hostTimestamp, zeroTimestamp);
    EXPECT_EQ(retVal, CL_OUT_OF_RESOURCES);

    delete mDev;
}

TEST_F(ClGetHostTimerTest, GivenNullDeviceWhenGettingHostTimerThenInvalidDeviceErrorIsReturned) {
    cl_ulong hostTimestamp = 0;

    retVal = clGetHostTimer(
        nullptr,
        &hostTimestamp);

    EXPECT_EQ(CL_INVALID_DEVICE, retVal);
}

TEST_F(ClGetHostTimerTest, GivenNullHostTimerWhenGettingHostTimerThenInvalidValueErrorIsReturned) {
    retVal = clGetHostTimer(
        testedClDevice,
        nullptr);

    EXPECT_EQ(CL_INVALID_VALUE, retVal);
}

TEST_F(ClGetHostTimerTest, GivenCorrectParametersWhenGettingHostTimerThenSuccessIsReturned) {
    cl_ulong hostTimestamp = 0;
    cl_ulong zeroTimestamp = 0;

    retVal = clGetHostTimer(
        testedClDevice,
        &hostTimestamp);

    EXPECT_GE(hostTimestamp, zeroTimestamp);
    EXPECT_EQ(retVal, CL_SUCCESS);
}

TEST_F(ClGetHostTimerTest, GivenValidOSTimeWhenGettingHostTimerThenSuccessIsReturned) {
    cl_ulong hostTimestamp = 0;
    cl_ulong zeroTimestamp = 0;

    auto mDev = new MockClDevice{MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr)};
    mDev->setOSTime(new MockOSTime());

    retVal = clGetHostTimer(
        mDev,
        &hostTimestamp);

    EXPECT_GE(hostTimestamp, zeroTimestamp);
    EXPECT_EQ(retVal, CL_SUCCESS);

    delete mDev;
}

TEST_F(ClGetHostTimerTest, GivenInvalidOSTimeWhenGettingHostTimerThenOutOfResourcesErrorIsReturned) {
    cl_ulong hostTimestamp = 0;
    cl_ulong zeroTimestamp = 0;

    auto mDev = new MockClDevice{MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr)};
    mDev->setOSTime(new FailOSTime());

    retVal = clGetHostTimer(
        mDev,
        &hostTimestamp);

    EXPECT_EQ(hostTimestamp, zeroTimestamp);
    EXPECT_EQ(retVal, CL_OUT_OF_RESOURCES);

    delete mDev;
}

} // namespace ULT
