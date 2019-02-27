/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/helpers/options.h"
#include "unit_tests/os_interface/linux/drm_mock.h"
#include "unit_tests/os_interface/linux/mock_os_time_linux.h"

#include "gtest/gtest.h"
#include "mock_performance_counters_linux.h"

using namespace OCLRT;

struct PerformanceCountersLinuxTest : public PerformanceCountersFixture,
                                      public ::testing::Test {

    void SetUp() override {
        PerformanceCountersFixture::SetUp();
        PerfCounterFlagsLinux::resetPerfCountersFlags();

        drm = static_cast<DrmMock *>(osInterfaceBase->get()->getDrm());
        osTimeLinux = static_cast<MockOSTimeLinux *>(osTimeBase.get());
    }
    void TearDown() override {
        PerformanceCountersFixture::TearDown();
    }
    void createPerfCounters() override {
        PerformanceCountersFixture::createPerfCounters();
        performanceCountersLinux = static_cast<MockPerformanceCountersLinux *>(performanceCountersBase.get());
    }

    DrmMock *drm;
    MockOSTimeLinux *osTimeLinux;
    MockPerformanceCountersLinux *performanceCountersLinux;
};

struct PerformanceCountersLinuxSendConfigCommandPointersTest : public PerformanceCountersLinuxTest,
                                                               public ::testing::WithParamInterface<bool /*perfmon func is nullptr*/> {
    void SetUp() override {
        PerformanceCountersLinuxTest::SetUp();
        createPerfCounters();
        perfmonFuncIsNullptr = GetParam();
    }

    void TearDown() override {
        performanceCountersBase->shutdown();
        PerformanceCountersLinuxTest::TearDown();
    }
    bool perfmonFuncIsNullptr;
    InstrPmRegsCfg config;
    uint32_t lastConfigHandle;
    bool lastConfigPending;
};

struct PerformanceCountersLinuxGetPerfmonConfigTest : public PerformanceCountersLinuxTest,
                                                      public ::testing::WithParamInterface<std::tuple<bool /*change handle*/,
                                                                                                      bool /*zero value of handle*/>> {
    void SetUp() override {
        PerformanceCountersLinuxTest::SetUp();
        createPerfCounters();
        performanceCountersLinux->setPerfmonLoadConfigFunc(perfmonLoadConfigMock);
    }
    void TearDown() override {
        PerformanceCountersLinuxTest::TearDown();
    }
};

TEST_F(PerformanceCountersLinuxTest, enableWithCreatedAsInterface) {
    createPerfCounters();
    performanceCountersLinux->setDlopenFunc(dlopenMockPassing);
    performanceCountersLinux->setDlsymFunc(dlsymMockPassing);
    performanceCountersBase->initialize(platformDevices[0]);
    performanceCountersBase->setAsIface(new char[1]);
    performanceCountersBase->enable();
}

TEST_F(PerformanceCountersLinuxTest, initializePerformanceCountersWithPassingFuncs) {
    createPerfCounters();
    performanceCountersLinux->setDlopenFunc(dlopenMockPassing);
    performanceCountersLinux->setDlsymFunc(dlsymMockPassing);
    performanceCountersBase->initialize(platformDevices[0]);
    EXPECT_EQ(1, PerfCounterFlagsLinux::dlopenFuncCalled);
    EXPECT_EQ(1, PerfCounterFlagsLinux::dlsymFuncCalled);
    EXPECT_FALSE(performanceCountersBase->isAvailable());
    performanceCountersBase->shutdown();
    EXPECT_EQ(0, PerfCounterFlagsLinux::dlcloseFuncCalled);
    EXPECT_EQ(0, PerfCounterFlagsLinux::escHwMetricsCalled);
    EXPECT_EQ(0, PerfCounterFlagsLinux::autoSamplingFuncCalled);
    EXPECT_EQ(0, PerfCounterFlagsLinux::autoSamplingStopped);
    EXPECT_EQ(0, PerfCounterFlagsLinux::autoSamplingStarted);
    EXPECT_FALSE(performanceCountersBase->isAvailable());
}

TEST_F(PerformanceCountersLinuxTest, initializePerformanceCountersWithFailingDlopenFunc) {
    createPerfCounters();
    performanceCountersLinux->setDlopenFunc(dlopenMockFailing);
    performanceCountersBase->initialize(platformDevices[0]);
    EXPECT_EQ(1, PerfCounterFlagsLinux::dlopenFuncCalled);
    EXPECT_EQ(0, PerfCounterFlagsLinux::dlsymFuncCalled);
    performanceCountersBase->shutdown();
    EXPECT_EQ(0, PerfCounterFlagsLinux::dlcloseFuncCalled);
}

TEST_F(PerformanceCountersLinuxTest, initializePerformanceCountersWithPassingDlopenFuncAndFailingDlsymFunc) {
    createPerfCounters();
    performanceCountersLinux->setDlopenFunc(dlopenMockPassing);
    performanceCountersLinux->setDlsymFunc(dlsymMockFailing);
    performanceCountersBase->initialize(platformDevices[0]);
    EXPECT_EQ(1, PerfCounterFlagsLinux::dlopenFuncCalled);
    EXPECT_EQ(1, PerfCounterFlagsLinux::dlsymFuncCalled);
    performanceCountersBase->shutdown();
    EXPECT_EQ(0, PerfCounterFlagsLinux::dlcloseFuncCalled);
}

TEST_F(PerformanceCountersLinuxTest, enablePerformanceCountersWithoutInitializing) {
    uint32_t refNum = 0;
    createPerfCounters();
    refNum = performanceCountersBase->getPerfCountersReferenceNumber();
    EXPECT_EQ(0u, refNum);
    performanceCountersBase->enable();
    EXPECT_EQ(0, PerfCounterFlagsLinux::escHwMetricsCalled);
    EXPECT_EQ(0, PerfCounterFlagsLinux::autoSamplingFuncCalled);
    EXPECT_EQ(0, PerfCounterFlagsLinux::hwMetricsEnableStatus);
    refNum = performanceCountersBase->getPerfCountersReferenceNumber();
    EXPECT_EQ(1u, refNum);
    performanceCountersBase->shutdown();
    EXPECT_EQ(0, PerfCounterFlagsLinux::dlcloseFuncCalled);
    EXPECT_EQ(0, PerfCounterFlagsLinux::escHwMetricsCalled);
    EXPECT_EQ(0, PerfCounterFlagsLinux::autoSamplingFuncCalled);
    EXPECT_EQ(0, PerfCounterFlagsLinux::autoSamplingStopped);
    refNum = performanceCountersBase->getPerfCountersReferenceNumber();
    EXPECT_EQ(0u, refNum);
}

TEST_F(PerformanceCountersLinuxTest, enablePerformanceCountersWithPassingEscHwMetricsFuncTwice) {
    uint32_t refNum = 0;
    createPerfCounters();
    EXPECT_FALSE(performanceCountersBase->isAvailable());
    refNum = performanceCountersBase->getPerfCountersReferenceNumber();
    EXPECT_EQ(0u, refNum);
    performanceCountersBase->enable();
    refNum = performanceCountersBase->getPerfCountersReferenceNumber();
    EXPECT_EQ(0, PerfCounterFlagsLinux::escHwMetricsCalled);
    EXPECT_EQ(0, PerfCounterFlagsLinux::autoSamplingFuncCalled);
    EXPECT_EQ(0, PerfCounterFlagsLinux::autoSamplingStarted);
    EXPECT_EQ(1u, refNum);
    EXPECT_FALSE(performanceCountersBase->isAvailable());
    performanceCountersBase->enable();
    refNum = performanceCountersBase->getPerfCountersReferenceNumber();
    EXPECT_EQ(0, PerfCounterFlagsLinux::escHwMetricsCalled);
    EXPECT_EQ(0, PerfCounterFlagsLinux::autoSamplingFuncCalled);
    EXPECT_EQ(0, PerfCounterFlagsLinux::autoSamplingStarted);
    EXPECT_EQ(2u, refNum);
    EXPECT_FALSE(performanceCountersBase->isAvailable());
    performanceCountersBase->shutdown();
    refNum = performanceCountersBase->getPerfCountersReferenceNumber();
    EXPECT_EQ(0, PerfCounterFlagsLinux::escHwMetricsCalled);
    EXPECT_EQ(0, PerfCounterFlagsLinux::autoSamplingFuncCalled);
    EXPECT_EQ(0, PerfCounterFlagsLinux::autoSamplingStopped);
    EXPECT_EQ(1u, refNum);
    EXPECT_FALSE(performanceCountersBase->isAvailable());
    performanceCountersBase->shutdown();
    refNum = performanceCountersBase->getPerfCountersReferenceNumber();
    EXPECT_EQ(0, PerfCounterFlagsLinux::escHwMetricsCalled);
    EXPECT_EQ(0, PerfCounterFlagsLinux::autoSamplingFuncCalled);
    EXPECT_EQ(0, PerfCounterFlagsLinux::autoSamplingStopped);
    EXPECT_EQ(0u, refNum);
    EXPECT_FALSE(performanceCountersBase->isAvailable());
}

TEST_F(PerformanceCountersLinuxTest, enablePerformanceCountersWithPassingHwMetricsFuncAfterProperInitializing) {
    uint32_t refNum = 0;
    createPerfCounters();
    performanceCountersLinux->setDlopenFunc(dlopenMockPassing);
    performanceCountersLinux->setDlsymFunc(dlsymMockPassing);
    performanceCountersBase->setEscHwMetricsFunc(hwMetricsEnableFuncPassing);
    performanceCountersBase->initialize(platformDevices[0]);
    refNum = performanceCountersBase->getPerfCountersReferenceNumber();
    EXPECT_EQ(0u, refNum);
    EXPECT_FALSE(performanceCountersBase->isAvailable());
    performanceCountersBase->enable();
    EXPECT_EQ(1, PerfCounterFlagsLinux::hwMetricsEnableStatus);
    EXPECT_EQ(1, PerfCounterFlagsLinux::escHwMetricsCalled);
    EXPECT_EQ(1, PerfCounterFlagsLinux::autoSamplingFuncCalled);
    EXPECT_EQ(1, PerfCounterFlagsLinux::autoSamplingStarted);
    refNum = performanceCountersBase->getPerfCountersReferenceNumber();
    EXPECT_EQ(1u, refNum);
    EXPECT_TRUE(performanceCountersBase->isAvailable());
    performanceCountersBase->shutdown();
    EXPECT_EQ(0, PerfCounterFlagsLinux::hwMetricsEnableStatus);
    EXPECT_EQ(0, PerfCounterFlagsLinux::dlcloseFuncCalled);
    EXPECT_EQ(2, PerfCounterFlagsLinux::escHwMetricsCalled);
    EXPECT_EQ(2, PerfCounterFlagsLinux::autoSamplingFuncCalled);
    EXPECT_EQ(1, PerfCounterFlagsLinux::autoSamplingStopped);
    refNum = performanceCountersBase->getPerfCountersReferenceNumber();
    EXPECT_EQ(0u, refNum);
    EXPECT_FALSE(performanceCountersBase->isAvailable());
}

TEST_F(PerformanceCountersLinuxTest, enablePerformanceCountersWithFailingAutoStartFuncAfterProperInitializing) {
    uint32_t refNum = 0;
    createPerfCounters();
    performanceCountersLinux->setDlopenFunc(dlopenMockPassing);
    performanceCountersLinux->setDlsymFunc(dlsymMockPassing);
    performanceCountersBase->setEscHwMetricsFunc(hwMetricsEnableFuncPassing);
    performanceCountersBase->setAutoSamplingStartFunc(autoSamplingStartFailing);
    performanceCountersBase->initialize(platformDevices[0]);
    refNum = performanceCountersBase->getPerfCountersReferenceNumber();
    EXPECT_EQ(0u, refNum);
    EXPECT_FALSE(performanceCountersBase->isAvailable());
    performanceCountersBase->enable();
    EXPECT_EQ(1, PerfCounterFlagsLinux::hwMetricsEnableStatus);
    EXPECT_EQ(1, PerfCounterFlagsLinux::escHwMetricsCalled);
    EXPECT_EQ(1, PerfCounterFlagsLinux::autoSamplingFuncCalled);
    EXPECT_EQ(1, PerfCounterFlagsLinux::autoSamplingStarted);
    refNum = performanceCountersBase->getPerfCountersReferenceNumber();
    EXPECT_EQ(1u, refNum);
    EXPECT_FALSE(performanceCountersBase->isAvailable());
    performanceCountersBase->shutdown();
    EXPECT_EQ(0, PerfCounterFlagsLinux::hwMetricsEnableStatus);
    EXPECT_EQ(0, PerfCounterFlagsLinux::dlcloseFuncCalled);
    EXPECT_EQ(2, PerfCounterFlagsLinux::escHwMetricsCalled);
    EXPECT_EQ(1, PerfCounterFlagsLinux::autoSamplingFuncCalled);
    EXPECT_EQ(0, PerfCounterFlagsLinux::autoSamplingStopped);
    refNum = performanceCountersBase->getPerfCountersReferenceNumber();
    EXPECT_EQ(0u, refNum);
    EXPECT_FALSE(performanceCountersBase->isAvailable());
}

TEST_F(PerformanceCountersLinuxTest, enablePerformanceCountersWithFailingHwMetricsFuncAfterProperInitializing) {
    createPerfCounters();
    performanceCountersLinux->setDlopenFunc(dlopenMockPassing);
    performanceCountersLinux->setDlsymFunc(dlsymMockPassing);
    performanceCountersBase->setEscHwMetricsFunc(hwMetricsEnableFuncFailing);
    performanceCountersBase->initialize(platformDevices[0]);
    performanceCountersBase->enable();
    EXPECT_EQ(1, PerfCounterFlagsLinux::hwMetricsEnableStatus);
    EXPECT_EQ(1, PerfCounterFlagsLinux::escHwMetricsCalled);
    EXPECT_EQ(0, PerfCounterFlagsLinux::autoSamplingFuncCalled);
    EXPECT_EQ(0, PerfCounterFlagsLinux::autoSamplingStarted);
    performanceCountersBase->shutdown();
    EXPECT_EQ(1, PerfCounterFlagsLinux::hwMetricsEnableStatus);
    EXPECT_EQ(0, PerfCounterFlagsLinux::dlcloseFuncCalled);
    EXPECT_EQ(1, PerfCounterFlagsLinux::escHwMetricsCalled);
    EXPECT_EQ(0, PerfCounterFlagsLinux::autoSamplingFuncCalled);
    EXPECT_EQ(0, PerfCounterFlagsLinux::autoSamplingStopped);
}

TEST_F(PerformanceCountersLinuxTest, enablePerformanceCountersWithPassingHwMetricsFuncAfterProperInitializingTwice) {
    createPerfCounters();
    performanceCountersLinux->setDlopenFunc(dlopenMockPassing);
    performanceCountersLinux->setDlsymFunc(dlsymMockPassing);
    performanceCountersBase->setEscHwMetricsFunc(hwMetricsEnableFuncPassing);
    performanceCountersBase->initialize(platformDevices[0]);
    performanceCountersBase->enable();
    EXPECT_EQ(1, PerfCounterFlagsLinux::hwMetricsEnableStatus);
    EXPECT_EQ(1, PerfCounterFlagsLinux::escHwMetricsCalled);
    EXPECT_EQ(1, PerfCounterFlagsLinux::autoSamplingFuncCalled);
    EXPECT_EQ(1, PerfCounterFlagsLinux::autoSamplingStarted);
    performanceCountersBase->enable();
    EXPECT_EQ(1, PerfCounterFlagsLinux::hwMetricsEnableStatus);
    EXPECT_EQ(1, PerfCounterFlagsLinux::escHwMetricsCalled);
    EXPECT_EQ(1, PerfCounterFlagsLinux::autoSamplingFuncCalled);
    EXPECT_EQ(1, PerfCounterFlagsLinux::autoSamplingStarted);
    performanceCountersBase->shutdown();
    EXPECT_EQ(1, PerfCounterFlagsLinux::hwMetricsEnableStatus);
    EXPECT_EQ(0, PerfCounterFlagsLinux::dlcloseFuncCalled);
    EXPECT_EQ(1, PerfCounterFlagsLinux::escHwMetricsCalled);
    EXPECT_EQ(1, PerfCounterFlagsLinux::autoSamplingFuncCalled);
    EXPECT_EQ(0, PerfCounterFlagsLinux::autoSamplingStopped);
    performanceCountersBase->shutdown();
    EXPECT_EQ(0, PerfCounterFlagsLinux::hwMetricsEnableStatus);
    EXPECT_EQ(0, PerfCounterFlagsLinux::dlcloseFuncCalled);
    EXPECT_EQ(2, PerfCounterFlagsLinux::escHwMetricsCalled);
    EXPECT_EQ(2, PerfCounterFlagsLinux::autoSamplingFuncCalled);
    EXPECT_EQ(1, PerfCounterFlagsLinux::autoSamplingStopped);
}

TEST_F(PerformanceCountersLinuxTest, enablePerformanceCountersWithFailingHwMetricsFuncAfterStartingAutoSampling) {
    createPerfCounters();
    performanceCountersLinux->setDlopenFunc(dlopenMockPassing);
    performanceCountersLinux->setDlsymFunc(dlsymMockPassing);
    performanceCountersBase->setEscHwMetricsFunc(hwMetricsEnableFuncPassing);
    performanceCountersBase->initialize(platformDevices[0]);
    performanceCountersBase->enable();
    EXPECT_EQ(1, PerfCounterFlagsLinux::hwMetricsEnableStatus);
    EXPECT_EQ(1, PerfCounterFlagsLinux::escHwMetricsCalled);
    EXPECT_EQ(1, PerfCounterFlagsLinux::autoSamplingFuncCalled);
    EXPECT_EQ(1, PerfCounterFlagsLinux::autoSamplingStarted);
    performanceCountersBase->setEscHwMetricsFunc(hwMetricsEnableFuncFailing);
    performanceCountersBase->enable();
    EXPECT_EQ(1, PerfCounterFlagsLinux::hwMetricsEnableStatus);
    EXPECT_EQ(1, PerfCounterFlagsLinux::escHwMetricsCalled);
    EXPECT_EQ(1, PerfCounterFlagsLinux::autoSamplingFuncCalled);
    EXPECT_EQ(1, PerfCounterFlagsLinux::autoSamplingStarted);
    performanceCountersBase->shutdown();
    EXPECT_EQ(1, PerfCounterFlagsLinux::hwMetricsEnableStatus);
    EXPECT_EQ(0, PerfCounterFlagsLinux::dlcloseFuncCalled);
    EXPECT_EQ(1, PerfCounterFlagsLinux::escHwMetricsCalled);
    EXPECT_EQ(1, PerfCounterFlagsLinux::autoSamplingFuncCalled);
    EXPECT_EQ(0, PerfCounterFlagsLinux::autoSamplingStopped);
    performanceCountersBase->shutdown();
    EXPECT_EQ(0, PerfCounterFlagsLinux::dlcloseFuncCalled);
    EXPECT_EQ(2, PerfCounterFlagsLinux::escHwMetricsCalled);
    EXPECT_EQ(2, PerfCounterFlagsLinux::autoSamplingFuncCalled);
    EXPECT_EQ(1, PerfCounterFlagsLinux::autoSamplingStopped);
}

TEST_F(PerformanceCountersLinuxTest, enablePerformanceCountersWithMdLibHandleAndNotLoadedFunc) {
    createPerfCounters();
    performanceCountersLinux->setDlopenFunc(dlopenMockPassing);
    performanceCountersLinux->setDlsymFunc(dlsymMockFailing);
    performanceCountersBase->setEscHwMetricsFunc(hwMetricsEnableFuncPassing);
    performanceCountersBase->initialize(platformDevices[0]);
    EXPECT_EQ(1, PerfCounterFlagsLinux::dlopenFuncCalled);
    EXPECT_EQ(1, PerfCounterFlagsLinux::dlsymFuncCalled);
    performanceCountersBase->enable();
    EXPECT_EQ(0, PerfCounterFlagsLinux::hwMetricsEnableStatus);
    EXPECT_EQ(0, PerfCounterFlagsLinux::escHwMetricsCalled);
    EXPECT_EQ(0, PerfCounterFlagsLinux::autoSamplingFuncCalled);
    EXPECT_EQ(0, PerfCounterFlagsLinux::autoSamplingStarted);
    performanceCountersBase->shutdown();
    EXPECT_EQ(0, PerfCounterFlagsLinux::hwMetricsEnableStatus);
    EXPECT_EQ(0, PerfCounterFlagsLinux::dlcloseFuncCalled);
    EXPECT_EQ(0, PerfCounterFlagsLinux::escHwMetricsCalled);
    EXPECT_EQ(0, PerfCounterFlagsLinux::autoSamplingFuncCalled);
    EXPECT_EQ(0, PerfCounterFlagsLinux::autoSamplingStopped);
}

TEST_F(PerformanceCountersLinuxTest, setCpuTimestamp) {
    createPerfCounters();
    EXPECT_EQ(0ull, performanceCountersBase->getCpuRawTimestamp());
    osTimeLinux->setGetTimeFunc(getTimeFuncPassing);
    performanceCountersBase->setCpuTimestamp();
    EXPECT_NE(0ull, performanceCountersBase->getCpuRawTimestamp());
    EXPECT_EQ(performanceCountersBase->getCpuRawTimestamp(),
              osTimeBase->getCpuRawTimestamp());
}

TEST_P(PerformanceCountersLinuxSendConfigCommandPointersTest, givenPerformanceCountersWithPassingFuncsWhenSendPmRegsCfgCommandsIsCalledWithPerfmonFuncNullptrThenReturnsFalseOtherwiseTrue) {
    if (!perfmonFuncIsNullptr) {
        performanceCountersBase->initialize(platformDevices[0]);
    }
    lastConfigPending = false;

    auto retVal = performanceCountersBase->sendPmRegsCfgCommands(&config, &lastConfigHandle, &lastConfigPending);
    if (perfmonFuncIsNullptr) {
        EXPECT_FALSE(retVal);
        EXPECT_EQ(0, PerfCounterFlagsLinux::checkPmRegsCfgCalled);
        EXPECT_EQ(0, PerfCounterFlagsLinux::loadPmRegsCfgCalled);
    } else {
        EXPECT_TRUE(retVal);
        EXPECT_TRUE(lastConfigPending);
        EXPECT_EQ(1, PerfCounterFlagsLinux::checkPmRegsCfgCalled);
        EXPECT_EQ(1, PerfCounterFlagsLinux::loadPmRegsCfgCalled);
    }
}

INSTANTIATE_TEST_CASE_P(
    PerfCountersTests,
    PerformanceCountersLinuxSendConfigCommandPointersTest,
    testing::Bool());

int perfmonLoadConfigFailing(int fd, drm_intel_context *ctx, uint32_t *oaCfgId, uint32_t *gpCfgId) {
    PerfCounterFlagsLinux::perfmonLoadConfigCalled++;
    return -1;
}

TEST_F(PerformanceCountersLinuxTest, givenPerformanceCountersWithFailingPerfmonLoadConfigFuncWhenGetPerfmonConfigIsCalledThenReturnsFalse) {
    InstrPmRegsCfg config;
    uint32_t lastConfigHandle;
    bool lastConfigPending;
    createPerfCounters();
    performanceCountersLinux->setPerfmonLoadConfigFunc(perfmonLoadConfigFailing);
    performanceCountersBase->setCheckPmRegsCfgFunc(checkPmRegsCfgPassing);
    performanceCountersBase->setLoadPmRegsCfgFunc(loadPmRegsCfgPassing);
    auto retVal = performanceCountersBase->sendPmRegsCfgCommands(&config, &lastConfigHandle, &lastConfigPending);
    EXPECT_EQ(1, PerfCounterFlagsLinux::perfmonLoadConfigCalled);
    EXPECT_FALSE(retVal);
}

int perfmonLoadConfigChangingOa(int fd, drm_intel_context *ctx, uint32_t *oaCfgId, uint32_t *gpCfgId) {
    PerfCounterFlagsLinux::perfmonLoadConfigCalled++;
    (*oaCfgId)++;
    return 0;
}

int perfmonLoadConfigChangingGp(int fd, drm_intel_context *ctx, uint32_t *oaCfgId, uint32_t *gpCfgId) {
    PerfCounterFlagsLinux::perfmonLoadConfigCalled++;
    (*gpCfgId)++;
    return 0;
}

TEST_P(PerformanceCountersLinuxGetPerfmonConfigTest, givenPassingPerfmonLoadConfigFuncWhenGetPerfmonConfigIsCalledThenReturnsTrueIfDoesntChangeOaHandle) {
    bool changingOaHandle;
    bool isZeroValue;
    InstrPmRegsCfg config;
    std::tie(changingOaHandle, isZeroValue) = GetParam();

    if (changingOaHandle) {
        performanceCountersLinux->setPerfmonLoadConfigFunc(perfmonLoadConfigChangingOa);
    }
    if (isZeroValue) {
        config.OaCounters.Handle = 0;
    } else {
        config.OaCounters.Handle = 1;
    }
    auto retVal = performanceCountersLinux->getPerfmonConfig(&config);
    EXPECT_EQ(1, PerfCounterFlagsLinux::perfmonLoadConfigCalled);
    if (!isZeroValue && changingOaHandle) {
        EXPECT_FALSE(retVal);
    } else {
        EXPECT_TRUE(retVal);
    }
}

TEST_P(PerformanceCountersLinuxGetPerfmonConfigTest, givenPassingPerfmonLoadConfigFuncWhenGetPerfmonConfigIsCalledThenReturnsTrueIfDoesntChangeGpHandle) {
    bool changingGpHandle;
    bool isZeroValue;
    InstrPmRegsCfg config;
    std::tie(changingGpHandle, isZeroValue) = GetParam();

    if (changingGpHandle) {
        performanceCountersLinux->setPerfmonLoadConfigFunc(perfmonLoadConfigChangingGp);
    }
    if (isZeroValue) {
        config.GpCounters.Handle = 0;
    } else {
        config.GpCounters.Handle = 1;
    }
    auto retVal = performanceCountersLinux->getPerfmonConfig(&config);
    EXPECT_EQ(1, PerfCounterFlagsLinux::perfmonLoadConfigCalled);
    if (!isZeroValue && changingGpHandle) {
        EXPECT_FALSE(retVal);
    } else {
        EXPECT_TRUE(retVal);
    }
}

INSTANTIATE_TEST_CASE_P(
    PerfCountersTests,
    PerformanceCountersLinuxGetPerfmonConfigTest,
    testing::Combine(
        testing::Bool(),
        testing::Bool()));

TEST_F(PerformanceCountersLinuxTest, givenPerfCountersWithHwContextEqualsZeroWhenGetCurrentReportIdsIsCalledManyTimesThenReturnProperNumber) {
    unsigned int retVal;
    createPerfCounters();
    retVal = performanceCountersBase->getCurrentReportId();
    EXPECT_EQ(0x00000u, osInterfaceBase->getHwContextId());
    EXPECT_EQ(0x00000001u, retVal);
}
