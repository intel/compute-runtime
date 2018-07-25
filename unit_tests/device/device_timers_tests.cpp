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
#include "gtest/gtest.h"
#include "unit_tests/mocks/mock_ostime.h"
#include "unit_tests/fixtures/device_fixture.h"

using namespace OCLRT;

namespace ULT {

TEST(MockOSTime, DeviceHostIncreaseCheck) {
    cl_ulong deviceTimestamp[2] = {0, 0};
    cl_ulong hostTimestamp[2] = {0, 0};

    auto mDev = MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr);
    mDev->setOSTime(new MockOSTime());

    mDev->getDeviceAndHostTimer(
        &deviceTimestamp[0],
        &hostTimestamp[0]);

    std::this_thread::sleep_for(std::chrono::nanoseconds(1000));

    mDev->getDeviceAndHostTimer(
        &deviceTimestamp[1],
        &hostTimestamp[1]);

    EXPECT_LT(deviceTimestamp[0], deviceTimestamp[1]);
    EXPECT_LT(hostTimestamp[0], hostTimestamp[1]);

    delete mDev;
}

TEST(MockOSTime, DeviceHostDeltaCheck) {
    cl_ulong deviceTimestamp[2] = {0, 0};
    cl_ulong hostTimestamp[2] = {0, 0};
    cl_ulong hostOnlyTimestamp[2] = {0, 0};
    cl_ulong hostDiff = 0;
    cl_ulong hostOnlyDiff = 0;
    cl_ulong observedDiff = 0;
    cl_ulong allowedDiff = 0;
    float allowedErr = 0.005f;

    auto mDev = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));

    mDev->getDeviceAndHostTimer(
        &deviceTimestamp[0],
        &hostTimestamp[0]);

    mDev->getHostTimer(
        &hostOnlyTimestamp[0]);

    mDev->getDeviceAndHostTimer(
        &deviceTimestamp[1],
        &hostTimestamp[1]);

    mDev->getHostTimer(
        &hostOnlyTimestamp[1]);

    hostDiff = hostTimestamp[1] - hostTimestamp[0];
    hostOnlyDiff = hostOnlyTimestamp[1] - hostOnlyTimestamp[0];

    EXPECT_LT(deviceTimestamp[0], deviceTimestamp[1]);
    EXPECT_LT(hostTimestamp[0], hostOnlyTimestamp[0]);
    EXPECT_LT(hostTimestamp[1], hostOnlyTimestamp[1]);

    if (hostOnlyDiff > hostDiff) {
        observedDiff = hostOnlyDiff - hostDiff;
        allowedDiff = (cl_ulong)(allowedErr * hostDiff);
    } else {
        observedDiff = hostDiff - hostOnlyDiff;
        allowedDiff = (cl_ulong)(allowedErr * hostOnlyDiff);
    }

    EXPECT_TRUE(observedDiff <= allowedDiff);
}

TEST(MockOSTime, HostIncreaseCheck) {
    cl_ulong hostTimestamp[2] = {0, 0};

    auto mDev = MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr);
    mDev->setOSTime(new MockOSTime());

    mDev->getHostTimer(
        &hostTimestamp[0]);

    std::this_thread::sleep_for(std::chrono::nanoseconds(1000));

    mDev->getHostTimer(
        &hostTimestamp[1]);

    EXPECT_LT(hostTimestamp[0], hostTimestamp[1]);

    delete mDev;
}

TEST(MockOSTime, NegativeTest) {
    auto mDev = MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr);
    mDev->setOSTime(nullptr);

    double zeroRes;
    zeroRes = mDev->getPlatformHostTimerResolution();

    EXPECT_EQ(zeroRes, 0.0);

    delete mDev;
}
} // namespace ULT
