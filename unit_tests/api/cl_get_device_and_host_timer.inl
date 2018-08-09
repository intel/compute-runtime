/*
* Copyright (c) 2017 - 2018, Intel Corporation
*
* Permission is hereby granted, free of charge, to any person obtaining a
* copy of this software and associated documentation files (the "Software"),
* to deal in the Software without restriction, including without limitation
* the rights to use, copy, modify, merge, publish, distribute, sublicense,
* and/or sell copies of the Software, and to permit persons to whom the
* Software is furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included
* in all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
* OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
* THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
* OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
* ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
* OTHER DEALINGS IN THE SOFTWARE.
*/

#include "cl_api_tests.h"
#include "unit_tests/fixtures/device_fixture.h"
#include "unit_tests/mocks/mock_ostime.h"

using namespace OCLRT;

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

TEST_F(clGetDeviceAndHostTimerTest, NullDevice) {
    cl_ulong device_timestamp = 0;
    cl_ulong host_timestamp = 0;

    retVal = clGetDeviceAndHostTimer(
        nullptr,
        &device_timestamp,
        &host_timestamp);

    EXPECT_EQ(CL_INVALID_DEVICE, retVal);
}

TEST_F(clGetDeviceAndHostTimerTest, NullHostTimer) {
    cl_ulong device_timestamp = 0;

    retVal = clGetDeviceAndHostTimer(
        devices[0],
        &device_timestamp,
        nullptr);

    EXPECT_EQ(CL_INVALID_VALUE, retVal);
}

TEST_F(clGetDeviceAndHostTimerTest, NullDevicesTimer) {
    cl_ulong host_timestamp = 0;

    retVal = clGetDeviceAndHostTimer(
        devices[0],
        nullptr,
        &host_timestamp);

    EXPECT_EQ(CL_INVALID_VALUE, retVal);
}

TEST_F(clGetDeviceAndHostTimerTest, OsCallPass) {
    cl_ulong device_timestamp = 0;
    cl_ulong host_timestamp = 0;
    cl_ulong zero_timestamp = 0;

    auto mDev = MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr);
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

TEST_F(clGetDeviceAndHostTimerTest, OsCallFail) {
    cl_ulong device_timestamp = 0;
    cl_ulong host_timestamp = 0;
    cl_ulong zero_timestamp = 0;

    auto mDev = MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr);
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

TEST_F(clGetHostTimerTest, NullDevice) {
    cl_ulong host_timestamp = 0;

    retVal = clGetHostTimer(
        nullptr,
        &host_timestamp);

    EXPECT_EQ(CL_INVALID_DEVICE, retVal);
}

TEST_F(clGetHostTimerTest, NullHostTimer) {
    retVal = clGetHostTimer(
        devices[0],
        nullptr);

    EXPECT_EQ(CL_INVALID_VALUE, retVal);
}

TEST_F(clGetHostTimerTest, ActualHostTimer) {
    cl_ulong host_timestamp = 0;
    cl_ulong zero_timestamp = 0;

    retVal = clGetHostTimer(
        devices[0],
        &host_timestamp);

    EXPECT_GE(host_timestamp, zero_timestamp);
    EXPECT_EQ(retVal, CL_SUCCESS);
}

TEST_F(clGetHostTimerTest, OsCallPass) {
    cl_ulong host_timestamp = 0;
    cl_ulong zero_timestamp = 0;

    auto mDev = MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr);
    mDev->setOSTime(new MockOSTime());

    retVal = clGetHostTimer(
        mDev,
        &host_timestamp);

    EXPECT_GE(host_timestamp, zero_timestamp);
    EXPECT_EQ(retVal, CL_SUCCESS);

    delete mDev;
}

TEST_F(clGetHostTimerTest, OsCallFail) {
    cl_ulong host_timestamp = 0;
    cl_ulong zero_timestamp = 0;

    auto mDev = MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr);
    mDev->setOSTime(new FailOSTime());

    retVal = clGetHostTimer(
        mDev,
        &host_timestamp);

    EXPECT_EQ(host_timestamp, zero_timestamp);
    EXPECT_EQ(retVal, CL_OUT_OF_RESOURCES);

    delete mDev;
}

} // namespace ULT
