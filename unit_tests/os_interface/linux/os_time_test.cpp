/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "unit_tests/os_interface/linux/device_command_stream_fixture.h"
#include "unit_tests/os_interface/linux/mock_os_time_linux.h"
#include "runtime/os_interface/linux/drm_neo.h"
#include "test.h"
#include "gtest/gtest.h"
#include "runtime/os_interface/linux/os_interface.h"
#include "runtime/os_interface/linux/os_time_linux.h"

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

using namespace OCLRT;
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
};

TEST_F(DrmTimeTest, DetectWithNullDrmNoCrash) {
}

TEST_F(DrmTimeTest, GetCpuTime) {
    uint64_t time = 0;
    auto error = osTime->getCpuTime(&time);
    EXPECT_TRUE(error);
    EXPECT_NE(0ULL, time);
}

TEST_F(DrmTimeTest, GetCpuTimeFail) {
    uint64_t time = 0;
    osTime->setGetTimeFunc(getTimeFuncFalse);
    auto error = osTime->getCpuTime(&time);
    EXPECT_FALSE(error);
}

TEST_F(DrmTimeTest, GetGpuTime) {
    uint64_t time = 0;
    auto pDrm = new DrmMockTime();
    osTime->updateDrm(pDrm);
    auto error = osTime->getGpuTime32(&time);
    EXPECT_TRUE(error);
    EXPECT_NE(0ULL, time);
    error = osTime->getGpuTime36(&time);
    EXPECT_TRUE(error);
    EXPECT_NE(0ULL, time);
    error = osTime->getGpuTimeSplitted(&time);
    EXPECT_TRUE(error);
    EXPECT_NE(0ULL, time);
    delete pDrm;
}

TEST_F(DrmTimeTest, GetGpuTimeFails) {
    uint64_t time = 0;
    auto pDrm = new DrmMockFail();
    osTime->updateDrm(pDrm);
    auto error = osTime->getGpuTime32(&time);
    EXPECT_FALSE(error);
    error = osTime->getGpuTime36(&time);
    EXPECT_FALSE(error);
    error = osTime->getGpuTimeSplitted(&time);
    EXPECT_FALSE(error);
    delete pDrm;
}

TEST_F(DrmTimeTest, GetCpuGpuTime) {
    TimeStampData CPUGPUTime01 = {0, 0};
    TimeStampData CPUGPUTime02 = {0, 0};
    auto pDrm = new DrmMockTime();
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
    delete pDrm;
}

TEST_F(DrmTimeTest, GIVENDrmWHENGetCpuGpuTimeTHENPassed) {
    TimeStampData CPUGPUTime01 = {0, 0};
    TimeStampData CPUGPUTime02 = {0, 0};
    auto pDrm = new DrmMockTime();
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
    delete pDrm;
}

TEST_F(DrmTimeTest, givenGetCpuGpuTimeWhenItIsUnavailableThenReturnFalse) {
    TimeStampData CPUGPUTime = {0, 0};
    auto error = osTime->getCpuGpuTime(&CPUGPUTime);
    EXPECT_FALSE(error);
}

TEST_F(DrmTimeTest, GetCpuGpuTimeFails) {
    TimeStampData CPUGPUTime01 = {0, 0};
    auto pDrm = new DrmMockFail();
    osTime->updateDrm(pDrm);
    auto error = osTime->getCpuGpuTime(&CPUGPUTime01);
    EXPECT_FALSE(error);
    delete pDrm;
}

TEST_F(DrmTimeTest, GetCpuGpuTimeCpuFails) {
    TimeStampData CPUGPUTime01 = {0, 0};
    auto pDrm = new DrmMockTime();
    osTime->setGetTimeFunc(getTimeFuncFalse);
    osTime->updateDrm(pDrm);
    auto error = osTime->getCpuGpuTime(&CPUGPUTime01);
    EXPECT_FALSE(error);
    delete pDrm;
}

TEST_F(DrmTimeTest, detect) {
    auto drm = new DrmMockCustom;
    OSTimeLinux::TimestampFunction t1 = &OSTimeLinux::getGpuTime32;
    void **p1 = (void **)&t1;
    OSTimeLinux::TimestampFunction t2 = &OSTimeLinux::getGpuTimeSplitted;
    void **p2 = (void **)&t2;
    OSTimeLinux::TimestampFunction t3 = &OSTimeLinux::getGpuTime36;
    void **p3 = (void **)&t3;
    osTime->updateDrm(drm);

    {
        void **p = (void **)&(osTime->getGpuTime);
        EXPECT_EQ(*p, *p3);
    }

    {
        drm->ioctl_res = -1;
        osTime->timestampTypeDetect();
        void **p = (void **)&(osTime->getGpuTime);
        EXPECT_EQ(*p, *p1);
    }

    DrmMockCustom::IoctlResExt ioctlToPass = {1, 0};
    {
        drm->reset();
        drm->ioctl_res = -1;
        drm->ioctl_res_ext = &ioctlToPass; // 2nd ioctl is successful
        osTime->timestampTypeDetect();
        void **p = (void **)&(osTime->getGpuTime);
        EXPECT_EQ(*p, *p2);
    }
    delete drm;
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
