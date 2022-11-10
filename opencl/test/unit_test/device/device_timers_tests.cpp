/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/mocks/mock_ostime.h"

#include "opencl/test/unit_test/fixtures/cl_device_fixture.h"

#include "gtest/gtest.h"

using namespace NEO;

namespace ULT {

TEST(MockOSTime, WhenSleepingThenDeviceAndHostTimerAreIncreased) {
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

TEST(MockOSTime, WhenGettingTimersThenDiffBetweenQueriesWithinAllowedError) {
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

TEST(MockOSTime, WhenSleepingThenHostTimerIsIncreased) {
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

TEST(MockOSTime, GivenNullWhenSettingOsTimeThenResolutionIsZero) {
    auto mDev = MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr);
    mDev->setOSTime(nullptr);

    double zeroRes;
    zeroRes = mDev->getPlatformHostTimerResolution();

    EXPECT_EQ(zeroRes, 0.0);

    delete mDev;
}

TEST(MockOSTime, givenDeviceTimestampBaseNotEnabledWhenGetDeviceAndHostTimerThenCpuTimestampIsReturned) {
    auto mockDevice = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    mockDevice->setOSTime(new MockOSTimeWithConstTimestamp());

    uint64_t deviceTS = 0u, hostTS = 0u;
    mockDevice->getDeviceAndHostTimer(&deviceTS, &hostTS);

    EXPECT_EQ(deviceTS, MockDeviceTimeWithConstTimestamp::CPU_TIME_IN_NS);
    EXPECT_EQ(deviceTS, hostTS);
}

TEST(MockOSTime, givenDeviceTimestampBaseEnabledWhenGetDeviceAndHostTimerThenGpuTimestampIsReturned) {
    DebugManagerStateRestore dbgRestorer;
    DebugManager.flags.EnableDeviceBasedTimestamps.set(true);

    auto mockDevice = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    mockDevice->setOSTime(new MockOSTimeWithConstTimestamp());

    uint64_t deviceTS = 0u, hostTS = 0u;
    mockDevice->getDeviceAndHostTimer(&deviceTS, &hostTS);

    EXPECT_EQ(deviceTS, MockDeviceTimeWithConstTimestamp::GPU_TIMESTAMP);
    EXPECT_NE(deviceTS, hostTS);
}

class FailingMockOSTime : public OSTime {
  public:
    FailingMockOSTime() {
        this->deviceTime = std::make_unique<MockDeviceTime>();
    }

    bool getCpuTime(uint64_t *timeStamp) override {
        return false;
    }

    double getHostTimerResolution() const override {
        return 0;
    }

    uint64_t getCpuRawTimestamp() override {
        return 0;
    }
};

class FailingMockDeviceTime : public DeviceTime {
  public:
    bool getCpuGpuTime(TimeStampData *pGpuCpuTime, OSTime *osTime) override {
        return false;
    }

    double getDynamicDeviceTimerResolution(HardwareInfo const &hwInfo) const override {
        return 1.0;
    }

    uint64_t getDynamicDeviceTimerClock(HardwareInfo const &hwInfo) const override {
        return static_cast<uint64_t>(1000000000.0 / OSTime::getDeviceTimerResolution(hwInfo));
    }
};

class MockOSTimeWithFailingDeviceTime : public OSTime {
  public:
    MockOSTimeWithFailingDeviceTime() {
        this->deviceTime = std::make_unique<FailingMockDeviceTime>();
    }

    bool getCpuTime(uint64_t *timeStamp) override {
        return true;
    }

    double getHostTimerResolution() const override {
        return 0;
    }

    uint64_t getCpuRawTimestamp() override {
        return 0;
    }
};

TEST(MockOSTime, givenFailingDeviceTimeWhenGetDeviceAndHostTimerThenFalseIsReturned) {
    auto mockDevice = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    mockDevice->setOSTime(new MockOSTimeWithFailingDeviceTime());

    uint64_t deviceTS = 0u, hostTS = 0u;
    bool retVal = mockDevice->getDeviceAndHostTimer(&deviceTS, &hostTS);

    EXPECT_FALSE(retVal);
    EXPECT_EQ(deviceTS, 0u);
}

} // namespace ULT
