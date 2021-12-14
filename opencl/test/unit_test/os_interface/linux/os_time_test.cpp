/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/drm_neo.h"
#include "shared/source/os_interface/linux/os_time_linux.h"
#include "shared/source/os_interface/os_interface.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/os_interface/linux/device_command_stream_fixture.h"
#include "shared/test/common/test_macros/test.h"

#include "opencl/test/unit_test/os_interface/linux/mock_os_time_linux.h"

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
        osInterface = std::unique_ptr<OSInterface>(new OSInterface());
        osTime = MockOSTimeLinux::create(osInterface.get());
        osTime->setResolutionFunc(resolutionFuncTrue);
        osTime->setGetTimeFunc(getTimeFuncTrue);
    }

    void TearDown() override {
    }
    std::unique_ptr<MockOSTimeLinux> osTime;
    std::unique_ptr<OSInterface> osInterface;
    MockExecutionEnvironment executionEnvironment;
};

TEST_F(DrmTimeTest, GivenMockOsTimeThenInitializes) {
}

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

TEST_F(DrmTimeTest, WhenGettingGpuTimeThenSuceeds) {
    uint64_t time = 0;
    auto pDrm = new DrmMockTime(mockFd, *executionEnvironment.rootDeviceEnvironments[0]);
    osTime->updateDrm(pDrm);
    auto error = osTime->getDeviceTime()->getGpuTime32(&time);
    EXPECT_TRUE(error);
    EXPECT_NE(0ULL, time);
    error = osTime->getDeviceTime()->getGpuTime36(&time);
    EXPECT_TRUE(error);
    EXPECT_NE(0ULL, time);
    error = osTime->getDeviceTime()->getGpuTimeSplitted(&time);
    EXPECT_TRUE(error);
    EXPECT_NE(0ULL, time);
}

TEST_F(DrmTimeTest, GivenInvalidDrmWhenGettingGpuTimeThenFails) {
    uint64_t time = 0;
    auto pDrm = new DrmMockFail(*executionEnvironment.rootDeviceEnvironments[0]);
    osTime->updateDrm(pDrm);
    auto error = osTime->getDeviceTime()->getGpuTime32(&time);
    EXPECT_FALSE(error);
    error = osTime->getDeviceTime()->getGpuTime36(&time);
    EXPECT_FALSE(error);
    error = osTime->getDeviceTime()->getGpuTimeSplitted(&time);
    EXPECT_FALSE(error);
}

TEST_F(DrmTimeTest, WhenGettingCpuGpuTimeThenSucceeds) {
    TimeStampData CPUGPUTime01 = {0, 0};
    TimeStampData CPUGPUTime02 = {0, 0};
    auto pDrm = new DrmMockTime(mockFd, *executionEnvironment.rootDeviceEnvironments[0]);
    osTime->updateDrm(pDrm);
    auto error = osTime->getCpuGpuTime(&CPUGPUTime01);
    EXPECT_TRUE(error);
    EXPECT_NE(0ULL, CPUGPUTime01.CPUTimeinNS);
    EXPECT_NE(0ULL, CPUGPUTime01.GPUTimeStamp);
    error = osTime->getCpuGpuTime(&CPUGPUTime02);
    EXPECT_TRUE(error);
    EXPECT_NE(0ULL, CPUGPUTime02.CPUTimeinNS);
    EXPECT_NE(0ULL, CPUGPUTime02.GPUTimeStamp);
    EXPECT_GT(CPUGPUTime02.GPUTimeStamp, CPUGPUTime01.GPUTimeStamp);
    EXPECT_GT(CPUGPUTime02.CPUTimeinNS, CPUGPUTime01.CPUTimeinNS);
}

TEST_F(DrmTimeTest, GivenDrmWhenGettingCpuGpuTimeThenSucceeds) {
    TimeStampData CPUGPUTime01 = {0, 0};
    TimeStampData CPUGPUTime02 = {0, 0};
    auto pDrm = new DrmMockTime(mockFd, *executionEnvironment.rootDeviceEnvironments[0]);
    osTime->updateDrm(pDrm);
    auto error = osTime->getCpuGpuTime(&CPUGPUTime01);
    EXPECT_TRUE(error);
    EXPECT_NE(0ULL, CPUGPUTime01.CPUTimeinNS);
    EXPECT_NE(0ULL, CPUGPUTime01.GPUTimeStamp);
    error = osTime->getCpuGpuTime(&CPUGPUTime02);
    EXPECT_TRUE(error);
    EXPECT_NE(0ULL, CPUGPUTime02.CPUTimeinNS);
    EXPECT_NE(0ULL, CPUGPUTime02.GPUTimeStamp);
    EXPECT_GT(CPUGPUTime02.GPUTimeStamp, CPUGPUTime01.GPUTimeStamp);
    EXPECT_GT(CPUGPUTime02.CPUTimeinNS, CPUGPUTime01.CPUTimeinNS);
}

TEST_F(DrmTimeTest, givenGetCpuGpuTimeWhenItIsUnavailableThenReturnFalse) {
    TimeStampData CPUGPUTime = {0, 0};
    auto error = osTime->getCpuGpuTime(&CPUGPUTime);
    EXPECT_FALSE(error);
}

TEST_F(DrmTimeTest, GivenInvalidDrmWhenGettingCpuGpuTimeThenFails) {
    TimeStampData CPUGPUTime01 = {0, 0};
    auto pDrm = new DrmMockFail(*executionEnvironment.rootDeviceEnvironments[0]);
    osTime->updateDrm(pDrm);
    auto error = osTime->getCpuGpuTime(&CPUGPUTime01);
    EXPECT_FALSE(error);
}

TEST_F(DrmTimeTest, GivenInvalidFuncTimeWhenGettingCpuGpuTimeCpuThenFails) {
    TimeStampData CPUGPUTime01 = {0, 0};
    auto pDrm = new DrmMockTime(mockFd, *executionEnvironment.rootDeviceEnvironments[0]);
    osTime->setGetTimeFunc(getTimeFuncFalse);
    osTime->updateDrm(pDrm);
    auto error = osTime->getCpuGpuTime(&CPUGPUTime01);
    EXPECT_FALSE(error);
}

TEST_F(DrmTimeTest, WhenGettingTimeThenTimeIsCorrect) {
    auto drm = new DrmMockCustom(*executionEnvironment.rootDeviceEnvironments[0]);
    osTime->updateDrm(drm);

    {
        auto p = osTime->getDeviceTime()->getGpuTime;
        EXPECT_EQ(p, &DeviceTimeDrm::getGpuTime36);
    }

    {
        drm->ioctl_res = -1;
        osTime->getDeviceTime()->timestampTypeDetect();
        auto p = osTime->getDeviceTime()->getGpuTime;
        EXPECT_EQ(p, &DeviceTimeDrm::getGpuTime32);
    }

    DrmMockCustom::IoctlResExt ioctlToPass = {1, 0};
    {
        drm->reset();
        drm->ioctl_res = -1;
        drm->ioctl_res_ext = &ioctlToPass; // 2nd ioctl is successful
        osTime->getDeviceTime()->timestampTypeDetect();
        auto p = osTime->getDeviceTime()->getGpuTime;
        EXPECT_EQ(p, &DeviceTimeDrm::getGpuTimeSplitted);
        drm->ioctl_res_ext = &drm->NONE;
    }
}

TEST_F(DrmTimeTest, givenGpuTimestampResolutionQueryWhenIoctlFailsThenDefaultResolutionIsReturned) {
    auto defaultResolution = defaultHwInfo->capabilityTable.defaultProfilingTimerResolution;

    auto drm = new DrmMockCustom(*executionEnvironment.rootDeviceEnvironments[0]);
    osTime->updateDrm(drm);

    drm->getParamRetValue = 0;
    drm->ioctl_res = -1;

    auto result = osTime->getDynamicDeviceTimerResolution(*defaultHwInfo);
    EXPECT_DOUBLE_EQ(result, defaultResolution);
}

TEST_F(DrmTimeTest, givenGetDynamicDeviceTimerClockWhenIoctlFailsThenDefaultClockIsReturned) {
    auto defaultResolution = defaultHwInfo->capabilityTable.defaultProfilingTimerResolution;

    auto drm = new DrmMockCustom(*executionEnvironment.rootDeviceEnvironments[0]);
    osTime->updateDrm(drm);

    drm->getParamRetValue = 0;
    drm->ioctl_res = -1;

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

TEST_F(DrmTimeTest, givenGpuTimestampResolutionQueryWhenNoDrmThenDefaultResolutionIsReturned) {
    osTime->updateDrm(nullptr);

    auto defaultResolution = defaultHwInfo->capabilityTable.defaultProfilingTimerResolution;

    auto result = osTime->getDynamicDeviceTimerResolution(*defaultHwInfo);
    EXPECT_DOUBLE_EQ(result, defaultResolution);
}

TEST_F(DrmTimeTest, givenGpuTimestampResolutionQueryWhenIoctlSuccedsThenCorrectResolutionIsReturned) {
    auto drm = new DrmMockCustom(*executionEnvironment.rootDeviceEnvironments[0]);
    osTime->updateDrm(drm);

    // 19200000 is frequency yelding 52.083ns resolution
    drm->getParamRetValue = 19200000;
    drm->ioctl_res = 0;

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
