/*
 * Copyright (C) 2018-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/os_interface/os_interface.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/mocks/mock_ostime.h"
#include "shared/test/common/mocks/windows/mock_os_time_win.h"
#include "shared/test/common/os_interface/windows/wddm_fixture.h"

#include "gtest/gtest.h"

#include <memory>

using namespace NEO;

LARGE_INTEGER valueToSet = {};

BOOL WINAPI queryPerformanceCounterMock(
    _Out_ LARGE_INTEGER *lpPerformanceCount) {
    *lpPerformanceCount = valueToSet;
    return true;
};

class MockDeviceTimeWin : public MockDeviceTime {
  public:
    TimeQueryStatus getGpuCpuTimeImpl(TimeStampData *pGpuCpuTime, OSTime *osTime) override {
        getGpuCpuTimeImplCalled++;
        *pGpuCpuTime = gpuCpuTimeValue;
        return getGpuCpuTimeImplResult;
    }

    double getDynamicDeviceTimerResolution() const override {
        return deviceTimerResolution;
    }

    TimeQueryStatus getGpuCpuTimeImplResult = TimeQueryStatus::success;
    TimeStampData gpuCpuTimeValue{};
    uint32_t getGpuCpuTimeImplCalled = 0;
    double deviceTimerResolution = 1;
};

struct OSTimeWinTest : public ::testing::Test {
  public:
    void SetUp() override {
        auto &rootDeviceEnvironment = *executionEnvironment.rootDeviceEnvironments[0];
        auto wddm = new WddmMock(rootDeviceEnvironment);
        rootDeviceEnvironment.osInterface = std::make_unique<OSInterface>();
        rootDeviceEnvironment.osInterface->setDriverModel(std::unique_ptr<DriverModel>(wddm));
        osTime = std::unique_ptr<MockOSTimeWin>(new MockOSTimeWin(*rootDeviceEnvironment.osInterface));
        osTime->setDeviceTimerResolution();
    }

    void TearDown() override {
    }
    MockExecutionEnvironment executionEnvironment;
    std::unique_ptr<MockOSTimeWin> osTime;
};

TEST_F(OSTimeWinTest, given36BitGpuTimeStampWhenGpuTimeStampOverflowThenGpuTimeDoesNotDecrease) {
    auto deviceTime = new MockDeviceTimeWin();
    osTime->deviceTime.reset(deviceTime);
    osTime->setDeviceTimerResolution();

    TimeStampData gpuCpuTime = {0ull, 0ull};

    deviceTime->gpuCpuTimeValue = {100ull, 100ull};
    TimeQueryStatus error = osTime->getGpuCpuTime(&gpuCpuTime);
    EXPECT_EQ(error, TimeQueryStatus::success);
    EXPECT_EQ(100ull, gpuCpuTime.gpuTimeStamp);

    deviceTime->gpuCpuTimeValue.gpuTimeStamp = 200ll;
    error = osTime->getGpuCpuTime(&gpuCpuTime);
    EXPECT_EQ(error, TimeQueryStatus::success);
    EXPECT_EQ(200ull, gpuCpuTime.gpuTimeStamp);

    osTime->maxGpuTimeStamp = 1ull << 36;

    deviceTime->gpuCpuTimeValue.gpuTimeStamp = 10ull; // read below initial value
    error = osTime->getGpuCpuTime(&gpuCpuTime);
    EXPECT_EQ(error, TimeQueryStatus::success);
    EXPECT_EQ(osTime->maxGpuTimeStamp + 10ull, gpuCpuTime.gpuTimeStamp);

    deviceTime->gpuCpuTimeValue.gpuTimeStamp = 30ull; // second read below initial value
    error = osTime->getGpuCpuTime(&gpuCpuTime);
    EXPECT_EQ(error, TimeQueryStatus::success);
    EXPECT_EQ(osTime->maxGpuTimeStamp + 30ull, gpuCpuTime.gpuTimeStamp);

    deviceTime->gpuCpuTimeValue.gpuTimeStamp = 110ull;
    error = osTime->getGpuCpuTime(&gpuCpuTime);
    EXPECT_EQ(error, TimeQueryStatus::success);
    EXPECT_EQ(osTime->maxGpuTimeStamp + 110ull, gpuCpuTime.gpuTimeStamp);

    deviceTime->gpuCpuTimeValue.gpuTimeStamp = 70ull; // second overflow
    error = osTime->getGpuCpuTime(&gpuCpuTime);
    EXPECT_EQ(error, TimeQueryStatus::success);
    EXPECT_EQ(2ull * osTime->maxGpuTimeStamp + 70ull, gpuCpuTime.gpuTimeStamp);
}

TEST_F(OSTimeWinTest, given64BitGpuTimeStampWhenGpuTimeStampOverflowThenOverflowsAreNotDetected) {
    auto deviceTime = new MockDeviceTimeWin();
    osTime->deviceTime.reset(deviceTime);
    osTime->setDeviceTimerResolution();

    TimeStampData gpuCpuTime = {0ull, 0ull};

    deviceTime->gpuCpuTimeValue = {100ull, 100ull};
    TimeQueryStatus error = osTime->getGpuCpuTime(&gpuCpuTime);
    EXPECT_EQ(error, TimeQueryStatus::success);
    EXPECT_EQ(100ull, gpuCpuTime.gpuTimeStamp);

    deviceTime->gpuCpuTimeValue.gpuTimeStamp = 200ull;
    error = osTime->getGpuCpuTime(&gpuCpuTime);
    EXPECT_EQ(error, TimeQueryStatus::success);
    EXPECT_EQ(200ull, gpuCpuTime.gpuTimeStamp);

    osTime->maxGpuTimeStamp = 0ull;

    deviceTime->gpuCpuTimeValue.gpuTimeStamp = 10ull; // read below initial value
    error = osTime->getGpuCpuTime(&gpuCpuTime);
    EXPECT_EQ(error, TimeQueryStatus::success);
    EXPECT_EQ(10ull, gpuCpuTime.gpuTimeStamp);

    deviceTime->gpuCpuTimeValue.gpuTimeStamp = 30ull;
    error = osTime->getGpuCpuTime(&gpuCpuTime);
    EXPECT_EQ(error, TimeQueryStatus::success);
    EXPECT_EQ(30ull, gpuCpuTime.gpuTimeStamp);

    deviceTime->gpuCpuTimeValue.gpuTimeStamp = 110ull;
    error = osTime->getGpuCpuTime(&gpuCpuTime);
    EXPECT_EQ(error, TimeQueryStatus::success);
    EXPECT_EQ(110ull, gpuCpuTime.gpuTimeStamp);

    deviceTime->gpuCpuTimeValue.gpuTimeStamp = 70ull;
    error = osTime->getGpuCpuTime(&gpuCpuTime);
    EXPECT_EQ(error, TimeQueryStatus::success);
    EXPECT_EQ(70ull, gpuCpuTime.gpuTimeStamp);
}

TEST_F(OSTimeWinTest, givenZeroFrequencyWhenGetHostTimerFuncIsCalledThenReturnsZero) {
    LARGE_INTEGER frequency;
    frequency.QuadPart = 0;
    osTime->setFrequency(frequency);
    auto retVal = osTime->getHostTimerResolution();
    EXPECT_EQ(0, retVal);
}

TEST_F(OSTimeWinTest, givenNonZeroFrequencyWhenGetHostTimerFuncIsCalledThenReturnsNonZero) {
    LARGE_INTEGER frequency;
    frequency.QuadPart = NSEC_PER_SEC;
    osTime->setFrequency(frequency);
    auto retVal = osTime->getHostTimerResolution();
    EXPECT_EQ(1.0, retVal);
}

TEST_F(OSTimeWinTest, givenOsTimeWinWhenGetCpuRawTimestampIsCalledThenReturnsNonZero) {
    auto retVal = osTime->getCpuRawTimestamp();
    EXPECT_NE(0ull, retVal);
}

TEST_F(OSTimeWinTest, givenHighValueOfCpuTimestampWhenItIsObtainedThenItHasProperValue) {
    osTime->overrideQueryPerformanceCounterFunction(queryPerformanceCounterMock);
    LARGE_INTEGER frequency = {};
    frequency.QuadPart = 190457;
    osTime->setFrequency(frequency);
    valueToSet.QuadPart = 700894514854;
    uint64_t timeStamp = 0;
    uint64_t expectedTimestamp = static_cast<uint64_t>((static_cast<double>(valueToSet.QuadPart) * static_cast<double>(NSEC_PER_SEC) / static_cast<double>(frequency.QuadPart)));
    osTime->getCpuTime(&timeStamp);
    EXPECT_EQ(expectedTimestamp, timeStamp);
}

TEST_F(OSTimeWinTest, WhenGettingCpuTimeHostThenSucceeds) {
    uint64_t time = 0;
    auto error = osTime->getCpuTimeHost(&time);
    EXPECT_FALSE(error);
    EXPECT_NE(0ULL, time);
}

TEST(OSTimeWinTests, givenNoOSInterfaceWhenGetCpuTimeThenReturnsSuccess) {
    uint64_t time = 0;
    auto osTime(OSTime::create(nullptr));
    auto error = osTime->getCpuTime(&time);
    EXPECT_TRUE(error);
    EXPECT_EQ(0u, time);
}

TEST(OSTimeWinTests, givenNoOSInterfaceWhenGetGpuCpuTimeThenReturnsSuccess) {
    TimeStampData gpuCpuTime = {};
    auto osTime(OSTime::create(nullptr));
    auto success = osTime->getGpuCpuTime(&gpuCpuTime);
    EXPECT_EQ(success, TimeQueryStatus::success);
    EXPECT_EQ(0u, gpuCpuTime.cpuTimeinNS);
    EXPECT_EQ(0u, gpuCpuTime.gpuTimeStamp);
}

TEST(OSTimeWinTests, givenOSInterfaceWhenGetGpuCpuTimeThenReturnsSuccess) {
    MockExecutionEnvironment executionEnvironment;
    auto &rootDeviceEnvironment = *executionEnvironment.rootDeviceEnvironments[0];
    auto wddm = new WddmMock(rootDeviceEnvironment);
    TimeStampData gpuCpuTime01 = {};
    TimeStampData gpuCpuTime02 = {};
    rootDeviceEnvironment.osInterface = std::make_unique<OSInterface>();
    rootDeviceEnvironment.osInterface->setDriverModel(std::unique_ptr<DriverModel>(wddm));
    wddm->init();
    auto osTime = OSTime::create(rootDeviceEnvironment.osInterface.get());
    osTime->setDeviceTimerResolution();
    TimeQueryStatus success = osTime->getGpuCpuTime(&gpuCpuTime01);
    EXPECT_EQ(success, TimeQueryStatus::success);
    EXPECT_NE(0u, gpuCpuTime01.cpuTimeinNS);
    EXPECT_NE(0u, gpuCpuTime01.gpuTimeStamp);
    success = osTime->getGpuCpuTime(&gpuCpuTime02);
    EXPECT_EQ(success, TimeQueryStatus::success);
    EXPECT_NE(0u, gpuCpuTime02.cpuTimeinNS);
    EXPECT_NE(0u, gpuCpuTime02.gpuTimeStamp);
    EXPECT_GT(gpuCpuTime02.gpuTimeStamp, gpuCpuTime01.gpuTimeStamp);
    EXPECT_GT(gpuCpuTime02.cpuTimeinNS, gpuCpuTime01.cpuTimeinNS);
}

TEST_F(OSTimeWinTest, whenGettingMaxGpuTimeStampValueThenHwInfoBasedValueIsReturned) {
    if (defaultHwInfo->capabilityTable.timestampValidBits < 64) {
        auto expectedMaxGpuTimeStampValue = 1ull << defaultHwInfo->capabilityTable.timestampValidBits;
        EXPECT_EQ(expectedMaxGpuTimeStampValue, osTime->getMaxGpuTimeStamp());
    } else {
        EXPECT_EQ(0ull, osTime->getMaxGpuTimeStamp());
    }
}

TEST_F(OSTimeWinTest, whenGettingMaxGpuTimeStampValueWithinIntervalThenReuseFromPreviousCall) {
    osTime->overrideQueryPerformanceCounterFunction(queryPerformanceCounterMock);
    LARGE_INTEGER frequency = {};
    frequency.QuadPart = NSEC_PER_SEC;
    osTime->setFrequency(frequency);

    auto deviceTime = new MockDeviceTimeWin();
    osTime->deviceTime.reset(deviceTime);
    osTime->setDeviceTimerResolution();

    EXPECT_EQ(deviceTime->getGpuCpuTimeImplCalled, 0u);
    TimeStampData gpuCpuTime;
    deviceTime->gpuCpuTimeValue = {1u, 1u};
    valueToSet.QuadPart = 1;
    osTime->getGpuCpuTime(&gpuCpuTime);
    EXPECT_EQ(deviceTime->getGpuCpuTimeImplCalled, 1u);

    auto gpuTimestampBefore = gpuCpuTime.gpuTimeStamp;
    auto cpuTimeBefore = gpuCpuTime.cpuTimeinNS;
    valueToSet.QuadPart = 5;
    osTime->getGpuCpuTime(&gpuCpuTime);
    EXPECT_EQ(deviceTime->getGpuCpuTimeImplCalled, 1u);

    auto gpuTimestampAfter = gpuCpuTime.gpuTimeStamp;
    auto cpuTimeAfter = gpuCpuTime.cpuTimeinNS;

    auto cpuTimeDiff = cpuTimeAfter - cpuTimeBefore;

    auto deviceTimerResolution = deviceTime->getDynamicDeviceTimerResolution();
    auto gpuTimestampDiff = static_cast<uint64_t>(cpuTimeDiff / deviceTimerResolution);
    EXPECT_EQ(gpuTimestampAfter, gpuTimestampBefore + gpuTimestampDiff);
}

TEST_F(OSTimeWinTest, whenGettingGpuTimeStampValueAfterIntervalThenCallToKmdAndAdaptTimeout) {
    osTime->overrideQueryPerformanceCounterFunction(queryPerformanceCounterMock);
    LARGE_INTEGER frequency = {};
    frequency.QuadPart = NSEC_PER_SEC;
    osTime->setFrequency(frequency);

    // Recreate mock to apply debug flag
    auto deviceTime = new MockDeviceTimeWin();
    osTime->deviceTime.reset(deviceTime);
    osTime->setDeviceTimerResolution();
    EXPECT_EQ(deviceTime->getGpuCpuTimeImplCalled, 0u);

    const auto initialExpectedTimeoutNS = NSEC_PER_MSEC * 100;
    EXPECT_EQ(initialExpectedTimeoutNS, osTime->getTimestampRefreshTimeout());

    auto setTimestamps = [&](uint64_t cpuTimeNS, uint64_t cpuTimeFromKmdNS, uint64_t gpuTimestamp) {
        valueToSet.QuadPart = cpuTimeNS;
        deviceTime->gpuCpuTimeValue.cpuTimeinNS = cpuTimeFromKmdNS;
        deviceTime->gpuCpuTimeValue.gpuTimeStamp = gpuTimestamp;
    };
    setTimestamps(0, 0ull, 0ull);

    TimeStampData gpuCpuTime;
    osTime->getGpuCpuTime(&gpuCpuTime);
    EXPECT_EQ(deviceTime->getGpuCpuTimeImplCalled, 1u);

    // Error is smaller than 5%, timeout can be increased
    auto newTimeAfterInterval = valueToSet.QuadPart + osTime->getTimestampRefreshTimeout();
    setTimestamps(newTimeAfterInterval, newTimeAfterInterval + 10, newTimeAfterInterval + 10);

    osTime->getGpuCpuTime(&gpuCpuTime);
    EXPECT_EQ(deviceTime->getGpuCpuTimeImplCalled, 2u);

    auto diff = (gpuCpuTime.gpuTimeStamp - valueToSet.QuadPart);
    EXPECT_EQ(initialExpectedTimeoutNS + diff, osTime->getTimestampRefreshTimeout());
    EXPECT_GT(initialExpectedTimeoutNS + diff, initialExpectedTimeoutNS);

    // Error is larger than 5%, timeout should be decreased
    newTimeAfterInterval = valueToSet.QuadPart + osTime->getTimestampRefreshTimeout() + 10;
    setTimestamps(newTimeAfterInterval, newTimeAfterInterval * 2, newTimeAfterInterval * 2);

    osTime->getGpuCpuTime(&gpuCpuTime);
    EXPECT_EQ(deviceTime->getGpuCpuTimeImplCalled, 3u);

    EXPECT_LT(osTime->getTimestampRefreshTimeout(), initialExpectedTimeoutNS);
}

TEST_F(OSTimeWinTest, whenGetGpuCpuTimeFailedThenReturnFalse) {
    TimeStampData gpuCpuTime;
    auto deviceTime = new MockDeviceTimeWin();
    osTime->deviceTime.reset(deviceTime);
    deviceTime->getGpuCpuTimeImplResult = TimeQueryStatus::deviceLost;
    EXPECT_EQ(osTime->getGpuCpuTime(&gpuCpuTime), TimeQueryStatus::deviceLost);
}

TEST_F(OSTimeWinTest, whenGettingMaxGpuTimeStampValueAfterFlagSetThenCallToKmd) {
    TimeStampData gpuCpuTime;
    auto deviceTime = new MockDeviceTimeWin();
    osTime->deviceTime.reset(deviceTime);
    osTime->setDeviceTimerResolution();

    EXPECT_EQ(deviceTime->getGpuCpuTimeImplCalled, 0u);
    deviceTime->gpuCpuTimeValue = {1u, 1u};
    osTime->getGpuCpuTime(&gpuCpuTime);
    EXPECT_EQ(deviceTime->getGpuCpuTimeImplCalled, 1u);

    osTime->setRefreshTimestampsFlag();
    osTime->getGpuCpuTime(&gpuCpuTime);
    EXPECT_EQ(deviceTime->getGpuCpuTimeImplCalled, 2u);
}

TEST_F(OSTimeWinTest, whenGettingMaxGpuTimeStampValueWhenForceFlagSetThenCallToKmd) {
    osTime->overrideQueryPerformanceCounterFunction(queryPerformanceCounterMock);
    LARGE_INTEGER frequency = {};
    frequency.QuadPart = NSEC_PER_SEC;
    osTime->setFrequency(frequency);

    auto deviceTime = new MockDeviceTimeWin();
    osTime->deviceTime.reset(deviceTime);
    osTime->setDeviceTimerResolution();

    EXPECT_EQ(deviceTime->getGpuCpuTimeImplCalled, 0u);
    TimeStampData gpuCpuTime;
    deviceTime->gpuCpuTimeValue = {1u, 1u};
    valueToSet.QuadPart = 1;
    osTime->getGpuCpuTime(&gpuCpuTime);
    EXPECT_EQ(deviceTime->getGpuCpuTimeImplCalled, 1u);

    valueToSet.QuadPart = 5;
    osTime->getGpuCpuTime(&gpuCpuTime, true);
    EXPECT_EQ(deviceTime->getGpuCpuTimeImplCalled, 2u);
}

TEST_F(OSTimeWinTest, givenReusingTimestampsDisabledWhenGetGpuCpuTimeThenAlwaysCallKmd) {
    DebugManagerStateRestore restore;
    debugManager.flags.EnableReusingGpuTimestamps.set(0);
    auto deviceTime = new MockDeviceTimeWin();
    osTime->deviceTime.reset(deviceTime);
    osTime->setDeviceTimerResolution();
    TimeStampData gpuCpuTime;
    osTime->getGpuCpuTime(&gpuCpuTime);
    EXPECT_EQ(deviceTime->getGpuCpuTimeImplCalled, 1u);

    osTime->getGpuCpuTime(&gpuCpuTime);
    EXPECT_EQ(deviceTime->getGpuCpuTimeImplCalled, 2u);
}
