/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "unit_tests/fixtures/device_fixture.h"
#include "unit_tests/mocks/mock_ostime.h"

#include "gtest/gtest.h"

using namespace NEO;

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
