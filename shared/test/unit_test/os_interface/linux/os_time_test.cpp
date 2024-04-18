/*
 * Copyright (C) 2018-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/drm_neo.h"
#include "shared/source/os_interface/linux/ioctl_helper.h"
#include "shared/source/os_interface/linux/os_time_linux.h"
#include "shared/source/os_interface/os_interface.h"
#include "shared/test/common/mocks/linux/mock_os_time_linux.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/os_interface/linux/device_command_stream_fixture.h"
#include "shared/test/common/test_macros/test.h"

#include "gtest/gtest.h"

#include <dlfcn.h>

static int actualTime = 0;

int getTimeFuncFalse(clockid_t clkId, struct timespec *tp) throw() {
    return -1;
}
int getTimeFuncTrue(clockid_t clkId, struct timespec *tp) throw() {
    tp->tv_sec = 0;
    tp->tv_nsec = ++actualTime;
    return 0;
}

int resolutionFuncFalse(clockid_t clkId, struct timespec *res) throw() {
    return -1;
}
int resolutionFuncTrue(clockid_t clkId, struct timespec *res) throw() {
    res->tv_sec = 0;
    res->tv_nsec = 5;
    return 0;
}

using namespace NEO;
struct DrmTimeTest : public ::testing::Test {
  public:
    void SetUp() override {
        auto &rootDeviceEnvironment = *executionEnvironment.rootDeviceEnvironments[0];
        rootDeviceEnvironment.osInterface = std::make_unique<OSInterface>();
        rootDeviceEnvironment.osInterface->setDriverModel(std::make_unique<DrmMockTime>(mockFd, rootDeviceEnvironment));
        osTime = MockOSTimeLinux::create(*rootDeviceEnvironment.osInterface);
        osTime->setResolutionFunc(resolutionFuncTrue);
        osTime->setGetTimeFunc(getTimeFuncTrue);
        deviceTime = osTime->getDeviceTime();
    }

    void TearDown() override {
    }
    MockDeviceTimeDrm *deviceTime = nullptr;
    std::unique_ptr<MockOSTimeLinux> osTime;
    MockExecutionEnvironment executionEnvironment;
};

TEST_F(DrmTimeTest, WhenGettingCpuTimeThenSucceeds) {
    uint64_t time = 0;
    auto error = osTime->getCpuTime(&time);
    EXPECT_TRUE(error);
    EXPECT_NE(0ULL, time);
}

TEST_F(DrmTimeTest, GivenFalseTimeFuncWhenGettingCpuTimeThenFails) {
    uint64_t time = 0;
    osTime->setGetTimeFunc(getTimeFuncFalse);
    auto error = osTime->getCpuTime(&time);
    EXPECT_FALSE(error);
}

TEST_F(DrmTimeTest, WhenGettingGpuCpuTimeThenSucceeds) {
    TimeStampData gpuCpuTime01 = {0, 0};
    TimeStampData gpuCpuTime02 = {0, 0};
    auto pDrm = new DrmMockTime(mockFd, *executionEnvironment.rootDeviceEnvironments[0]);
    osTime->updateDrm(pDrm);
    auto error = osTime->getGpuCpuTime(&gpuCpuTime01);
    EXPECT_TRUE(error);
    EXPECT_NE(0ULL, gpuCpuTime01.cpuTimeinNS);
    EXPECT_NE(0ULL, gpuCpuTime01.gpuTimeStamp);
    error = osTime->getGpuCpuTime(&gpuCpuTime02);
    EXPECT_TRUE(error);
    EXPECT_NE(0ULL, gpuCpuTime02.cpuTimeinNS);
    EXPECT_NE(0ULL, gpuCpuTime02.gpuTimeStamp);
    EXPECT_GT(gpuCpuTime02.gpuTimeStamp, gpuCpuTime01.gpuTimeStamp);
    EXPECT_GT(gpuCpuTime02.cpuTimeinNS, gpuCpuTime01.cpuTimeinNS);
}

TEST_F(DrmTimeTest, GivenDrmWhenGettingGpuCpuTimeThenSucceeds) {
    TimeStampData gpuCpuTime01 = {0, 0};
    TimeStampData gpuCpuTime02 = {0, 0};
    auto pDrm = new DrmMockTime(mockFd, *executionEnvironment.rootDeviceEnvironments[0]);
    osTime->updateDrm(pDrm);
    auto error = osTime->getGpuCpuTime(&gpuCpuTime01);
    EXPECT_TRUE(error);
    EXPECT_NE(0ULL, gpuCpuTime01.cpuTimeinNS);
    EXPECT_NE(0ULL, gpuCpuTime01.gpuTimeStamp);
    error = osTime->getGpuCpuTime(&gpuCpuTime02);
    EXPECT_TRUE(error);
    EXPECT_NE(0ULL, gpuCpuTime02.cpuTimeinNS);
    EXPECT_NE(0ULL, gpuCpuTime02.gpuTimeStamp);
    EXPECT_GT(gpuCpuTime02.gpuTimeStamp, gpuCpuTime01.gpuTimeStamp);
    EXPECT_GT(gpuCpuTime02.cpuTimeinNS, gpuCpuTime01.cpuTimeinNS);
}

TEST_F(DrmTimeTest, givenGetGpuCpuTimeWhenItIsUnavailableThenReturnFalse) {
    TimeStampData gpuCpuTime = {0, 0};

    deviceTime->callBaseGetGpuCpuTimeImpl = false;
    deviceTime->getGpuCpuTimeImplResult = false;
    auto error = osTime->getGpuCpuTime(&gpuCpuTime);
    EXPECT_FALSE(error);
}

TEST_F(DrmTimeTest, given36BitGpuTimeStampWhenGpuTimeStampOverflowThenGpuTimeDoesNotDecrease) {
    TimeStampData gpuCpuTime = {0ull, 0ull};

    deviceTime->callBaseGetGpuCpuTimeImpl = false;
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

TEST_F(DrmTimeTest, given64BitGpuTimeStampWhenGpuTimeStampOverflowThenOverflowsAreNotDetected) {
    TimeStampData gpuCpuTime = {0ull, 0ull};

    deviceTime->callBaseGetGpuCpuTimeImpl = false;
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

TEST_F(DrmTimeTest, GivenInvalidDrmWhenGettingGpuCpuTimeThenFails) {
    TimeStampData gpuCpuTime01 = {0, 0};
    auto pDrm = new DrmMockFail(*executionEnvironment.rootDeviceEnvironments[0]);
    osTime->updateDrm(pDrm);
    auto error = osTime->getGpuCpuTime(&gpuCpuTime01);
    EXPECT_FALSE(error);
}

TEST_F(DrmTimeTest, GivenInvalidFuncTimeWhenGettingGpuCpuTimeCpuThenFails) {
    TimeStampData gpuCpuTime01 = {0, 0};
    auto pDrm = new DrmMockTime(mockFd, *executionEnvironment.rootDeviceEnvironments[0]);
    osTime->setGetTimeFunc(getTimeFuncFalse);
    osTime->updateDrm(pDrm);
    auto error = osTime->getGpuCpuTime(&gpuCpuTime01);
    EXPECT_FALSE(error);
}

TEST_F(DrmTimeTest, givenGpuTimestampResolutionQueryWhenIoctlFailsThenDefaultResolutionIsReturned) {
    auto defaultResolution = defaultHwInfo->capabilityTable.defaultProfilingTimerResolution;

    auto drm = new DrmMockCustom(*executionEnvironment.rootDeviceEnvironments[0]);
    osTime->updateDrm(drm);

    drm->getParamRetValue = 0;
    drm->ioctlRes = -1;

    auto result = osTime->getDynamicDeviceTimerResolution(*defaultHwInfo);
    EXPECT_DOUBLE_EQ(result, defaultResolution);
}

TEST_F(DrmTimeTest, givenGetDynamicDeviceTimerClockWhenIoctlFailsThenDefaultClockIsReturned) {
    auto defaultResolution = defaultHwInfo->capabilityTable.defaultProfilingTimerResolution;

    auto drm = new DrmMockCustom(*executionEnvironment.rootDeviceEnvironments[0]);
    osTime->updateDrm(drm);

    drm->getParamRetValue = 0;
    drm->ioctlRes = -1;

    auto result = osTime->getDynamicDeviceTimerClock(*defaultHwInfo);
    auto expectedResult = static_cast<uint64_t>(1000000000.0 / defaultResolution);
    EXPECT_EQ(result, expectedResult);
}

TEST_F(DrmTimeTest, givenGetDynamicDeviceTimerClockWhenIoctlSucceedsThenNonDefaultClockIsReturned) {
    auto drm = new DrmMockCustom(*executionEnvironment.rootDeviceEnvironments[0]);
    osTime->updateDrm(drm);

    uint64_t frequency = 1500;
    drm->getParamRetValue = static_cast<int>(frequency);

    auto result = osTime->getDynamicDeviceTimerClock(*defaultHwInfo);
    EXPECT_EQ(result, frequency);
}

TEST_F(DrmTimeTest, givenGpuTimestampResolutionQueryWhenIoctlSuccedsThenCorrectResolutionIsReturned) {
    auto drm = new DrmMockCustom(*executionEnvironment.rootDeviceEnvironments[0]);
    osTime->updateDrm(drm);

    // 19200000 is frequency yelding 52.083ns resolution
    drm->getParamRetValue = 19200000;
    drm->ioctlRes = 0;

    auto result = osTime->getDynamicDeviceTimerResolution(*defaultHwInfo);
    EXPECT_DOUBLE_EQ(result, 52.08333333333333);
}

TEST_F(DrmTimeTest, givenAlwaysFailingResolutionFuncWhenGetHostTimerResolutionIsCalledThenReturnsZero) {
    osTime->setResolutionFunc(resolutionFuncFalse);
    auto retVal = osTime->getHostTimerResolution();
    EXPECT_EQ(0, retVal);
}

TEST_F(DrmTimeTest, givenAlwaysPassingResolutionFuncWhenGetHostTimerResolutionIsCalledThenReturnsNonzero) {
    osTime->setResolutionFunc(resolutionFuncTrue);
    auto retVal = osTime->getHostTimerResolution();
    EXPECT_EQ(5, retVal);
}

TEST_F(DrmTimeTest, givenAlwaysFailingResolutionFuncWhenGetCpuRawTimestampIsCalledThenReturnsZero) {
    osTime->setResolutionFunc(resolutionFuncFalse);
    auto retVal = osTime->getCpuRawTimestamp();
    EXPECT_EQ(0ull, retVal);
}

TEST_F(DrmTimeTest, givenAlwaysFailingGetTimeFuncWhenGetCpuRawTimestampIsCalledThenReturnsZero) {
    osTime->setGetTimeFunc(getTimeFuncFalse);
    auto retVal = osTime->getCpuRawTimestamp();
    EXPECT_EQ(0ull, retVal);
}

TEST_F(DrmTimeTest, givenAlwaysPassingResolutionFuncWhenGetCpuRawTimestampIsCalledThenReturnsNonzero) {
    actualTime = 4;
    auto retVal = osTime->getCpuRawTimestamp();
    EXPECT_EQ(1ull, retVal);
}

TEST_F(DrmTimeTest, whenGettingMaxGpuTimeStampValueThenHwInfoBasedValueIsReturned) {
    if (defaultHwInfo->capabilityTable.timestampValidBits < 64) {
        auto expectedMaxGpuTimeStampValue = 1ull << defaultHwInfo->capabilityTable.timestampValidBits;
        EXPECT_EQ(expectedMaxGpuTimeStampValue, osTime->getMaxGpuTimeStamp());
    } else {
        EXPECT_EQ(0ull, osTime->getMaxGpuTimeStamp());
    }
}
