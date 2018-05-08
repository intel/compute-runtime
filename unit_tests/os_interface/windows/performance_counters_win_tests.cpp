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

#include "runtime/helpers/options.h"
#include "runtime/os_interface/windows/windows_wrapper.h"
#include "unit_tests/mocks/mock_wddm.h"
#include "unit_tests/os_interface/windows/mock_performance_counters_win.h"
#include "gtest/gtest.h"

using namespace OCLRT;

struct PerformanceCountersWinTest : public PerformanceCountersFixture,
                                    public ::testing::Test {
  public:
    void SetUp() override {
        PerformanceCountersFixture::SetUp();
        PerfCounterFlagsWin::resetPerfCountersFlags();

        wddm = static_cast<WddmMock *>(osInterfaceBase->get()->getWddm());
    }

    void TearDown() override {
        PerformanceCountersFixture::TearDown();
    }

    WddmMock *wddm;
};

TEST_F(PerformanceCountersWinTest, initializePerformanceCounters) {
    createPerfCounters();
    performanceCountersBase->initialize(platformDevices[0]);
    EXPECT_EQ(1, PerfCounterFlagsWin::setAvailableFuncCalled);
    EXPECT_EQ(1, PerfCounterFlagsWin::verifyEnableFuncCalled);

    EXPECT_FALSE(performanceCountersBase->isAvailable());
}

TEST_F(PerformanceCountersWinTest, enableWithCreatedAsInterface) {
    createPerfCounters();
    performanceCountersBase->initialize(platformDevices[0]);
    performanceCountersBase->setAsIface(new char[1]);
    performanceCountersBase->enable();
}

TEST_F(PerformanceCountersWinTest, enablePerformanceCountersWithPassingEscHwMetricsFunc) {
    createPerfCounters();
    performanceCountersBase->enable();
    EXPECT_EQ(1, PerfCounterFlagsWin::escHwMetricsCalled);
    EXPECT_EQ(1, PerfCounterFlagsWin::autoSamplingFuncCalled);
    EXPECT_EQ(1, PerfCounterFlagsWin::autoSamplingStarted);
    EXPECT_EQ(1, PerfCounterFlagsWin::hwMetricsEnableStatus);
    EXPECT_TRUE(performanceCountersBase->isAvailable());

    performanceCountersBase->shutdown();
    EXPECT_EQ(2, PerfCounterFlagsWin::escHwMetricsCalled);
    EXPECT_EQ(2, PerfCounterFlagsWin::autoSamplingFuncCalled);
    EXPECT_EQ(1, PerfCounterFlagsWin::autoSamplingStopped);
    EXPECT_EQ(0, PerfCounterFlagsWin::hwMetricsEnableStatus);
    EXPECT_FALSE(performanceCountersBase->isAvailable());
}

TEST_F(PerformanceCountersWinTest, enablePerformanceCountersWithFailingEscHwMetricsFunc) {
    createPerfCounters();
    performanceCountersBase->setEscHwMetricsFunc(hwMetricsEnableFuncFailing);
    performanceCountersBase->enable();
    EXPECT_EQ(1, PerfCounterFlagsWin::escHwMetricsCalled);
    EXPECT_EQ(0, PerfCounterFlagsWin::autoSamplingFuncCalled);
    EXPECT_EQ(0, PerfCounterFlagsWin::autoSamplingStarted);
    EXPECT_EQ(1, PerfCounterFlagsWin::hwMetricsEnableStatus);
    performanceCountersBase->shutdown();
    EXPECT_EQ(1, PerfCounterFlagsWin::escHwMetricsCalled);
    EXPECT_EQ(0, PerfCounterFlagsWin::autoSamplingFuncCalled);
    EXPECT_EQ(0, PerfCounterFlagsWin::autoSamplingStopped);
}

TEST_F(PerformanceCountersWinTest, enablePerformanceCountersWithPassingEscHwMetricsFuncTwice) {
    uint32_t refNum = 0;
    createPerfCounters();
    EXPECT_FALSE(performanceCountersBase->isAvailable());
    refNum = performanceCountersBase->getPerfCountersReferenceNumber();
    EXPECT_EQ(0u, refNum);
    performanceCountersBase->enable();
    refNum = performanceCountersBase->getPerfCountersReferenceNumber();
    EXPECT_EQ(1, PerfCounterFlagsWin::escHwMetricsCalled);
    EXPECT_EQ(1, PerfCounterFlagsWin::autoSamplingFuncCalled);
    EXPECT_EQ(1, PerfCounterFlagsWin::autoSamplingStarted);
    EXPECT_EQ(1u, refNum);
    EXPECT_TRUE(performanceCountersBase->isAvailable());
    performanceCountersBase->enable();
    refNum = performanceCountersBase->getPerfCountersReferenceNumber();
    EXPECT_EQ(1, PerfCounterFlagsWin::escHwMetricsCalled);
    EXPECT_EQ(1, PerfCounterFlagsWin::autoSamplingFuncCalled);
    EXPECT_EQ(1, PerfCounterFlagsWin::autoSamplingStarted);
    EXPECT_EQ(2u, refNum);
    EXPECT_TRUE(performanceCountersBase->isAvailable());
    performanceCountersBase->shutdown();
    refNum = performanceCountersBase->getPerfCountersReferenceNumber();
    EXPECT_EQ(1, PerfCounterFlagsWin::escHwMetricsCalled);
    EXPECT_EQ(1, PerfCounterFlagsWin::autoSamplingFuncCalled);
    EXPECT_EQ(0, PerfCounterFlagsWin::autoSamplingStopped);
    EXPECT_EQ(1u, refNum);
    EXPECT_TRUE(performanceCountersBase->isAvailable());
    performanceCountersBase->shutdown();
    refNum = performanceCountersBase->getPerfCountersReferenceNumber();
    EXPECT_EQ(2, PerfCounterFlagsWin::escHwMetricsCalled);
    EXPECT_EQ(2, PerfCounterFlagsWin::autoSamplingFuncCalled);
    EXPECT_EQ(1, PerfCounterFlagsWin::autoSamplingStopped);
    EXPECT_EQ(0u, refNum);
    EXPECT_FALSE(performanceCountersBase->isAvailable());
}

TEST_F(PerformanceCountersWinTest, enablePerformanceCountersWithFailingEscHwMetricsFuncTwice) {
    createPerfCounters();
    performanceCountersBase->setEscHwMetricsFunc(hwMetricsEnableFuncFailing);
    performanceCountersBase->enable();
    EXPECT_EQ(1, PerfCounterFlagsWin::escHwMetricsCalled);
    EXPECT_EQ(0, PerfCounterFlagsWin::autoSamplingFuncCalled);
    EXPECT_EQ(0, PerfCounterFlagsWin::autoSamplingStarted);
    EXPECT_FALSE(performanceCountersBase->isAvailable());
    performanceCountersBase->enable();
    EXPECT_EQ(1, PerfCounterFlagsWin::escHwMetricsCalled);
    EXPECT_EQ(0, PerfCounterFlagsWin::autoSamplingFuncCalled);
    EXPECT_EQ(0, PerfCounterFlagsWin::autoSamplingStarted);
    performanceCountersBase->shutdown();
    EXPECT_EQ(1, PerfCounterFlagsWin::escHwMetricsCalled);
    EXPECT_EQ(0, PerfCounterFlagsWin::autoSamplingFuncCalled);
    EXPECT_EQ(0, PerfCounterFlagsWin::autoSamplingStopped);
}

TEST_F(PerformanceCountersWinTest, enablePerformanceCountersWithFailingAutoStartFuncAfterProperInitializing) {
    uint32_t refNum = 0;
    createPerfCounters();
    performanceCountersBase->setEscHwMetricsFunc(hwMetricsEnableFuncPassing);
    performanceCountersBase->setAutoSamplingStartFunc(autoSamplingStartFailing);
    performanceCountersBase->initialize(platformDevices[0]);
    refNum = performanceCountersBase->getPerfCountersReferenceNumber();
    EXPECT_EQ(0u, refNum);
    EXPECT_FALSE(performanceCountersBase->isAvailable());
    performanceCountersBase->enable();
    EXPECT_EQ(1, PerfCounterFlagsWin::hwMetricsEnableStatus);
    EXPECT_EQ(1, PerfCounterFlagsWin::escHwMetricsCalled);
    EXPECT_EQ(1, PerfCounterFlagsWin::autoSamplingFuncCalled);
    EXPECT_EQ(1, PerfCounterFlagsWin::autoSamplingStarted);
    refNum = performanceCountersBase->getPerfCountersReferenceNumber();
    EXPECT_FALSE(performanceCountersBase->isAvailable());
    EXPECT_EQ(1u, refNum);
    performanceCountersBase->shutdown();
    EXPECT_EQ(0, PerfCounterFlagsWin::hwMetricsEnableStatus);
    EXPECT_EQ(2, PerfCounterFlagsWin::escHwMetricsCalled);
    EXPECT_EQ(1, PerfCounterFlagsWin::autoSamplingFuncCalled);
    EXPECT_EQ(0, PerfCounterFlagsWin::autoSamplingStopped);
    refNum = performanceCountersBase->getPerfCountersReferenceNumber();
    EXPECT_EQ(0u, refNum);
    EXPECT_FALSE(performanceCountersBase->isAvailable());
}

TEST_F(PerformanceCountersWinTest, enablePerformanceCountersWithFailingEscHwMetricsFuncAfterStartingAutoSampling) {
    createPerfCounters();
    performanceCountersBase->setEscHwMetricsFunc(hwMetricsEnableFuncPassing);
    performanceCountersBase->enable();
    EXPECT_EQ(1, PerfCounterFlagsWin::escHwMetricsCalled);
    EXPECT_EQ(1, PerfCounterFlagsWin::autoSamplingFuncCalled);
    EXPECT_EQ(1, PerfCounterFlagsWin::autoSamplingStarted);
    performanceCountersBase->setEscHwMetricsFunc(hwMetricsEnableFuncFailing);
    performanceCountersBase->enable();
    EXPECT_EQ(1, PerfCounterFlagsWin::escHwMetricsCalled);
    EXPECT_EQ(1, PerfCounterFlagsWin::autoSamplingFuncCalled);
    EXPECT_EQ(1, PerfCounterFlagsWin::autoSamplingStarted);
    performanceCountersBase->shutdown();
    EXPECT_EQ(1, PerfCounterFlagsWin::escHwMetricsCalled);
    EXPECT_EQ(1, PerfCounterFlagsWin::autoSamplingFuncCalled);
    EXPECT_EQ(0, PerfCounterFlagsWin::autoSamplingStopped);
    performanceCountersBase->shutdown();
    EXPECT_EQ(2, PerfCounterFlagsWin::escHwMetricsCalled);
    EXPECT_EQ(2, PerfCounterFlagsWin::autoSamplingFuncCalled);
    EXPECT_EQ(1, PerfCounterFlagsWin::autoSamplingStopped);
}

TEST_F(PerformanceCountersWinTest, setCpuTimestamp) {
    createPerfCounters();
    EXPECT_EQ(0ull, performanceCountersBase->getCpuRawTimestamp());
    performanceCountersBase->setCpuTimestamp();
    EXPECT_NE(0ull, performanceCountersBase->getCpuRawTimestamp());
}

TEST_F(PerformanceCountersWinTest, givenPerfCountersWithWddmWithSetHwContextIdWhenGetCurrentReportIdsIsCalledManyTimesThenReturnProperNumber) {
    unsigned int retVal;
    wddm->setHwContextId(0x12345);
    createPerfCounters();
    retVal = performanceCountersBase->getCurrentReportId();
    EXPECT_EQ(0x12345u, osInterfaceBase->getHwContextId());
    EXPECT_EQ(0x12345001u, retVal);
}
