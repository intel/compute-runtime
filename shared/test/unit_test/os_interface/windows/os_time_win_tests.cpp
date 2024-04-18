/*
 * Copyright (C) 2018-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/os_interface/os_interface.h"
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
    bool getGpuCpuTimeImpl(TimeStampData *pGpuCpuTime, OSTime *osTime) override {
        *pGpuCpuTime = gpuCpuTimeValue;
        return true;
    }
    TimeStampData gpuCpuTimeValue{};
};

struct OSTimeWinTest : public ::testing::Test {
  public:
    void SetUp() override {
        auto &rootDeviceEnvironment = *executionEnvironment.rootDeviceEnvironments[0];
        auto wddm = new WddmMock(rootDeviceEnvironment);
        rootDeviceEnvironment.osInterface = std::make_unique<OSInterface>();
        rootDeviceEnvironment.osInterface->setDriverModel(std::unique_ptr<DriverModel>(wddm));
        osTime = std::unique_ptr<MockOSTimeWin>(new MockOSTimeWin(*rootDeviceEnvironment.osInterface));
    }

    void TearDown() override {
    }
    MockExecutionEnvironment executionEnvironment;
    std::unique_ptr<MockOSTimeWin> osTime;
};

TEST_F(OSTimeWinTest, given36BitGpuTimeStampWhenGpuTimeStampOverflowThenGpuTimeDoesNotDecrease) {
    auto deviceTime = new MockDeviceTimeWin();
    osTime->deviceTime.reset(deviceTime);

    TimeStampData gpuCpuTime = {0ull, 0ull};

    deviceTime->gpuCpuTimeValue = {100ull, 100ull};
    EXPECT_TRUE(osTime->getGpuCpuTime(&gpuCpuTime));
    EXPECT_EQ(100ull, gpuCpuTime.gpuTimeStamp);

    deviceTime->gpuCpuTimeValue.gpuTimeStamp = 200ll;
    EXPECT_TRUE(osTime->getGpuCpuTime(&gpuCpuTime));
    EXPECT_EQ(200ull, gpuCpuTime.gpuTimeStamp);

    osTime->maxGpuTimeStamp = 1ull << 36;

    deviceTime->gpuCpuTimeValue.gpuTimeStamp = 10ull; // read below initial value
    EXPECT_TRUE(osTime->getGpuCpuTime(&gpuCpuTime));
    EXPECT_EQ(osTime->maxGpuTimeStamp + 10ull, gpuCpuTime.gpuTimeStamp);

    deviceTime->gpuCpuTimeValue.gpuTimeStamp = 30ull; // second read below initial value
    EXPECT_TRUE(osTime->getGpuCpuTime(&gpuCpuTime));
    EXPECT_EQ(osTime->maxGpuTimeStamp + 30ull, gpuCpuTime.gpuTimeStamp);

    deviceTime->gpuCpuTimeValue.gpuTimeStamp = 110ull;
    EXPECT_TRUE(osTime->getGpuCpuTime(&gpuCpuTime));
    EXPECT_EQ(osTime->maxGpuTimeStamp + 110ull, gpuCpuTime.gpuTimeStamp);

    deviceTime->gpuCpuTimeValue.gpuTimeStamp = 70ull; // second overflow
    EXPECT_TRUE(osTime->getGpuCpuTime(&gpuCpuTime));
    EXPECT_EQ(2ull * osTime->maxGpuTimeStamp + 70ull, gpuCpuTime.gpuTimeStamp);
}

TEST_F(OSTimeWinTest, given64BitGpuTimeStampWhenGpuTimeStampOverflowThenOverflowsAreNotDetected) {
    auto deviceTime = new MockDeviceTimeWin();
    osTime->deviceTime.reset(deviceTime);

    TimeStampData gpuCpuTime = {0ull, 0ull};

    deviceTime->gpuCpuTimeValue = {100ull, 100ull};
    EXPECT_TRUE(osTime->getGpuCpuTime(&gpuCpuTime));
    EXPECT_EQ(100ull, gpuCpuTime.gpuTimeStamp);

    deviceTime->gpuCpuTimeValue.gpuTimeStamp = 200ull;
    EXPECT_TRUE(osTime->getGpuCpuTime(&gpuCpuTime));
    EXPECT_EQ(200ull, gpuCpuTime.gpuTimeStamp);

    osTime->maxGpuTimeStamp = 0ull;

    deviceTime->gpuCpuTimeValue.gpuTimeStamp = 10ull; // read below initial value
    EXPECT_TRUE(osTime->getGpuCpuTime(&gpuCpuTime));
    EXPECT_EQ(10ull, gpuCpuTime.gpuTimeStamp);

    deviceTime->gpuCpuTimeValue.gpuTimeStamp = 30ull;
    EXPECT_TRUE(osTime->getGpuCpuTime(&gpuCpuTime));
    EXPECT_EQ(30ull, gpuCpuTime.gpuTimeStamp);

    deviceTime->gpuCpuTimeValue.gpuTimeStamp = 110ull;
    EXPECT_TRUE(osTime->getGpuCpuTime(&gpuCpuTime));
    EXPECT_EQ(110ull, gpuCpuTime.gpuTimeStamp);

    deviceTime->gpuCpuTimeValue.gpuTimeStamp = 70ull;
    EXPECT_TRUE(osTime->getGpuCpuTime(&gpuCpuTime));
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
    EXPECT_TRUE(success);
    EXPECT_EQ(0u, gpuCpuTime.cpuTimeinNS);
    EXPECT_EQ(0u, gpuCpuTime.gpuTimeStamp);
}

TEST(OSTimeWinTests, givenOSInterfaceWhenGetGpuCpuTimeThenReturnsSuccess) {
    MockExecutionEnvironment executionEnvironment;
    auto &rootDeviceEnvironment = *executionEnvironment.rootDeviceEnvironments[0];
    auto wddm = new WddmMock(rootDeviceEnvironment);
    TimeStampData gpuCpuTime01 = {};
    TimeStampData gpuCpuTime02 = {};
    std::unique_ptr<OSInterface> osInterface(new OSInterface());
    osInterface->setDriverModel(std::unique_ptr<DriverModel>(wddm));
    auto osTime = OSTime::create(osInterface.get());
    auto success = osTime->getGpuCpuTime(&gpuCpuTime01);
    EXPECT_TRUE(success);
    EXPECT_NE(0u, gpuCpuTime01.cpuTimeinNS);
    EXPECT_NE(0u, gpuCpuTime01.gpuTimeStamp);
    success = osTime->getGpuCpuTime(&gpuCpuTime02);
    EXPECT_TRUE(success);
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
