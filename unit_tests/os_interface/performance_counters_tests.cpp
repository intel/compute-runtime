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
#include "runtime/os_interface/os_time.h"
#include "runtime/os_interface/os_interface.h"
#include "unit_tests/os_interface/mock_performance_counters.h"
#include "unit_tests/mocks/mock_device.h"
#include "gtest/gtest.h"

using namespace OCLRT;

struct PerformanceCountersDeviceTest : public PerformanceCountersDeviceFixture,
                                       public ::testing::Test {
    void SetUp() override {
        PerformanceCountersDeviceFixture::SetUp();
        PerfCounterFlags::resetPerfCountersFlags();
        hwInfoToModify = (HardwareInfo *)(platformDevices[0]);
        instrumentationEnabledFlag = hwInfoToModify->capabilityTable.instrumentationEnabled;
    }
    void TearDown() override {
        PerformanceCountersDeviceFixture::TearDown();
        hwInfoToModify->capabilityTable.instrumentationEnabled = instrumentationEnabledFlag;
    }
    HardwareInfo *hwInfoToModify;
    bool instrumentationEnabledFlag;
};

TEST_F(PerformanceCountersDeviceTest, createDeviceWithPerformanceCounters) {
    hwInfoToModify->capabilityTable.instrumentationEnabled = true;
    auto device = MockDevice::createWithNewExecutionEnvironment<Device>(hwInfoToModify);
    EXPECT_NE(nullptr, device->getPerformanceCounters());
    EXPECT_EQ(1, PerfCounterFlags::initalizeCalled);
    delete device;
}

TEST_F(PerformanceCountersDeviceTest, createDeviceWithoutPerformanceCounters) {
    hwInfoToModify->capabilityTable.instrumentationEnabled = false;
    auto device = MockDevice::createWithNewExecutionEnvironment<Device>(hwInfoToModify);
    EXPECT_EQ(nullptr, device->getPerformanceCounters());
    EXPECT_EQ(0, PerfCounterFlags::initalizeCalled);
    delete device;
}

struct PerformanceCountersTest : public PerformanceCountersFixture,
                                 public ::testing::Test {
  public:
    void SetUp() override {
        PerformanceCountersFixture::SetUp();
    }

    void TearDown() override {
        PerformanceCountersFixture::TearDown();
    }
};

TEST_F(PerformanceCountersTest, createPerformanceCounters) {
    auto performanceCounters = PerformanceCounters::create(osTimeBase.get());
    EXPECT_NE(nullptr, performanceCounters);
    EXPECT_NE(nullptr, performanceCounters.get());

    EXPECT_FALSE(performanceCounters->isAvailable());
}

TEST_F(PerformanceCountersTest, shutdownPerformanceCountersWithoutEnabling) {
    createPerfCounters();
    performanceCountersBase->shutdown();
    EXPECT_EQ(0, PerfCounterFlags::escHwMetricsCalled);
    EXPECT_EQ(0, PerfCounterFlags::autoSamplingFuncCalled);
    EXPECT_EQ(0, PerfCounterFlags::autoSamplingStopped);
}

TEST_F(PerformanceCountersTest, givenPerformanceCountersWhenCreatedThenAllValuesProperlyInitialized) {
    createPerfCounters();

    EXPECT_EQ(IGFX_UNKNOWN_CORE, performanceCountersBase->getGfxFamily());

    EXPECT_EQ(nullptr, performanceCountersBase->getCbData().hDevice);

    EXPECT_FALSE(performanceCountersBase->getHwMetricsEnabled());
    EXPECT_FALSE(performanceCountersBase->getUseMIRPC());
    EXPECT_EQ(nullptr, performanceCountersBase->getPAutoSamplingInterface());
    EXPECT_EQ(0u, performanceCountersBase->getCpuRawTimestamp());
    EXPECT_EQ(1u, performanceCountersBase->getReportId());
    EXPECT_FALSE(performanceCountersBase->getAvailable());
    EXPECT_EQ(0u, performanceCountersBase->getPerfCountersReferenceNumber());
}

TEST_F(PerformanceCountersTest, givenPerformanceCountersNotEnabledWhenGetPmRegsCfgIsCalledThenReturnsNullptr) {
    createPerfCounters();
    auto pConfig = performanceCountersBase->getPmRegsCfg(0);
    EXPECT_EQ(nullptr, pConfig);
}

TEST_F(PerformanceCountersTest, givenPerformanceCountersEnabledWithPassingGetPmRegsCfgFuncWhenGetPmRegsCfgIsCalledThenReturnsPtr) {
    createPerfCounters();
    performanceCountersBase->setGetPmRegsCfgFunc(getPmRegsCfgPassing);
    performanceCountersBase->initialize(platformDevices[0]);
    performanceCountersBase->enable();
    auto pConfig = performanceCountersBase->getPmRegsCfg(0);
    EXPECT_EQ(1, PerfCounterFlags::getPmRegsCfgCalled);
    EXPECT_NE(nullptr, pConfig);
    delete pConfig;
    performanceCountersBase->shutdown();
}

TEST_F(PerformanceCountersTest, givenPerformanceCountersEnabledWithFailingGetPmRegsCfgFuncWhenGetPmRegsCfgIsCalledThenReturnsNullptr) {
    createPerfCounters();
    performanceCountersBase->setGetPmRegsCfgFunc(getPmRegsCfgFailing);
    performanceCountersBase->initialize(platformDevices[0]);
    performanceCountersBase->enable();
    auto pConfig = performanceCountersBase->getPmRegsCfg(0);
    EXPECT_EQ(1, PerfCounterFlags::getPmRegsCfgCalled);
    EXPECT_EQ(nullptr, pConfig);
    performanceCountersBase->shutdown();
}

TEST_F(PerformanceCountersTest, givenPerfCountersWhenGetReportIdsIsCalledManyTimesThenReturnNextNumbers) {
    int retVal;
    createPerfCounters();
    for (int i = 0; i < 4095; i++) {
        retVal = performanceCountersBase->getReportId();
        EXPECT_EQ((i + 1), retVal);
    }
    retVal = performanceCountersBase->getReportId();
    EXPECT_EQ(0, retVal);
}

TEST_F(PerformanceCountersTest, sendPerfConfigPositiveReadRegsTag) {
    cl_int ret;
    cl_uint count = 2;
    cl_uint offsets[2];
    cl_uint values[2];
    offsets[0] = INSTR_READ_REGS_CFG_TAG - 1;
    createPerfCounters();
    performanceCountersBase->initialize(platformDevices[0]);
    ret = performanceCountersBase->sendPerfConfiguration(count, offsets, values);
    EXPECT_EQ(CL_SUCCESS, ret);
    EXPECT_EQ(1, PerfCounterFlags::setPmRegsCfgCalled);
    EXPECT_EQ(0, PerfCounterFlags::sendReadRegsCfgCalled);
}

TEST_F(PerformanceCountersTest, sendPerfConfigPositiveReadRegs) {
    cl_int ret;
    cl_uint count = 2;
    cl_uint offsets[2];
    cl_uint values[2];
    offsets[0] = INSTR_READ_REGS_CFG_TAG;
    createPerfCounters();
    performanceCountersBase->initialize(platformDevices[0]);
    ret = performanceCountersBase->sendPerfConfiguration(count, offsets, values);
    EXPECT_EQ(CL_SUCCESS, ret);
    EXPECT_EQ(0, PerfCounterFlags::setPmRegsCfgCalled);
    EXPECT_EQ(1, PerfCounterFlags::sendReadRegsCfgCalled);
}

TEST_F(PerformanceCountersTest, negativeInvalidVal) {
    cl_int ret;
    cl_uint count = 2;
    cl_uint offsets[2];
    cl_uint values[2];
    createPerfCounters();
    performanceCountersBase->initialize(platformDevices[0]);
    ret = performanceCountersBase->sendPerfConfiguration(0, offsets, values);
    EXPECT_EQ(CL_INVALID_VALUE, ret);
    ret = performanceCountersBase->sendPerfConfiguration(count, nullptr, values);
    EXPECT_EQ(CL_INVALID_VALUE, ret);
    ret = performanceCountersBase->sendPerfConfiguration(count, offsets, nullptr);
    EXPECT_EQ(CL_INVALID_VALUE, ret);

    EXPECT_EQ(0, PerfCounterFlags::setPmRegsCfgCalled);
    EXPECT_EQ(0, PerfCounterFlags::sendReadRegsCfgCalled);
}

TEST_F(PerformanceCountersTest, negativeNoMatchingData) {
    cl_int ret;
    cl_uint count = 1;
    cl_uint offsets[2];
    cl_uint values[2];
    offsets[0] = INSTR_READ_REGS_CFG_TAG;
    createPerfCounters();
    performanceCountersBase->initialize(platformDevices[0]);
    ret = performanceCountersBase->sendPerfConfiguration(count, offsets, values);
    EXPECT_EQ(CL_PROFILING_INFO_NOT_AVAILABLE, ret);

    EXPECT_EQ(0, PerfCounterFlags::setPmRegsCfgCalled);
    EXPECT_EQ(0, PerfCounterFlags::sendReadRegsCfgCalled);
}

TEST_F(PerformanceCountersTest, sendPerfConfigNegativeFailingReadRegsTag) {
    cl_int ret;
    cl_uint count = 1;
    cl_uint offsets[1];
    cl_uint values[1];
    offsets[0] = INSTR_READ_REGS_CFG_TAG - 1;
    createPerfCounters();
    performanceCountersBase->setSetPmRegsCfgFunc(setPmRegsCfgFuncFailing);
    performanceCountersBase->initialize(platformDevices[0]);
    ret = performanceCountersBase->sendPerfConfiguration(count, offsets, values);
    EXPECT_EQ(CL_PROFILING_INFO_NOT_AVAILABLE, ret);

    EXPECT_EQ(1, PerfCounterFlags::setPmRegsCfgCalled);
    EXPECT_EQ(0, PerfCounterFlags::sendReadRegsCfgCalled);
}

TEST_F(PerformanceCountersTest, sendPerfConfigNegativeFailingReadRegs) {
    cl_int ret;
    cl_uint count = 2;
    cl_uint offsets[2];
    cl_uint values[2];
    offsets[0] = INSTR_READ_REGS_CFG_TAG;
    createPerfCounters();
    performanceCountersBase->setSendReadRegsCfgFunc(sendReadRegsCfgFuncFailing);
    performanceCountersBase->initialize(platformDevices[0]);
    ret = performanceCountersBase->sendPerfConfiguration(count, offsets, values);
    EXPECT_EQ(CL_PROFILING_INFO_NOT_AVAILABLE, ret);

    EXPECT_EQ(0, PerfCounterFlags::setPmRegsCfgCalled);
    EXPECT_EQ(1, PerfCounterFlags::sendReadRegsCfgCalled);
}

struct PerformanceCountersGetConfigTest : public PerformanceCountersTest,
                                          public ::testing::WithParamInterface<std::tuple<
                                              unsigned int /*given configuration type*/,
                                              bool /*expected value*/>> {
    void SetUp() override {
        PerformanceCountersTest::SetUp();
    }

    void TearDown() override {
        PerformanceCountersTest::TearDown();
    }

    unsigned int givenConfigType;
    bool expectedValue;
};

TEST_P(PerformanceCountersGetConfigTest, givenPerformanceCountersEnabledWithPassingGetPmRegsCfgFuncWhenGetPmRegsCfgIsCalledWithProperConfigurationTypeThenReturnsPtrOtherwiseNullptr) {
    std::tie(givenConfigType, expectedValue) = GetParam();
    createPerfCounters();
    performanceCountersBase->setGetPmRegsCfgFunc(getPmRegsCfgPassing);
    performanceCountersBase->initialize(platformDevices[0]);
    performanceCountersBase->enable();
    auto pConfig = performanceCountersBase->getPmRegsCfg(givenConfigType);
    if (expectedValue) {
        EXPECT_EQ(1, PerfCounterFlags::getPmRegsCfgCalled);
        EXPECT_NE(nullptr, pConfig);
        delete pConfig;
    } else {
        EXPECT_EQ(0, PerfCounterFlags::getPmRegsCfgCalled);
        EXPECT_EQ(nullptr, pConfig);
    }
    performanceCountersBase->shutdown();
}

std::tuple<unsigned int, bool> configsForGetPmRegsCfgTests[] = {
    std::make_tuple(GTDI_CONFIGURATION_SET_DYNAMIC, true),
    std::make_tuple(GTDI_CONFIGURATION_SET_1, true),
    std::make_tuple(GTDI_CONFIGURATION_SET_2, true),
    std::make_tuple(GTDI_CONFIGURATION_SET_3, true),
    std::make_tuple(GTDI_CONFIGURATION_SET_4, false),
    std::make_tuple(GTDI_CONFIGURATION_SET_COUNT, false),
    std::make_tuple(GTDI_CONFIGURATION_SET_MAX, false)};

INSTANTIATE_TEST_CASE_P(
    PerfCountersTests,
    PerformanceCountersGetConfigTest,
    testing::ValuesIn(configsForGetPmRegsCfgTests));

struct PerformanceCountersProcessEventTest : public PerformanceCountersTest,
                                             public ::testing::WithParamInterface<bool> {

    void SetUp() override {
        PerformanceCountersTest::SetUp();
        createPerfCounters();
        performanceCountersBase->initialize(platformDevices[0]);
        privateData.reset(new HwPerfCounter());
        eventComplete = true;
        outputParamSize = 0;
        inputParam.reset(new GTDI_QUERY());
        inputParamSize = sizeof(GTDI_QUERY);
    }
    void TearDown() override {
        performanceCountersBase->shutdown();
        PerformanceCountersTest::TearDown();
    }
    std::unique_ptr<GTDI_QUERY> inputParam;
    size_t inputParamSize;
    size_t outputParamSize;
    std::unique_ptr<HwPerfCounter> privateData;
    bool eventComplete;
};

TEST_P(PerformanceCountersProcessEventTest, givenNullptrInputParamWhenProcessEventPerfCountersIsCalledThenReturnsFalse) {
    eventComplete = GetParam();
    auto retVal = performanceCountersBase->processEventReport(inputParamSize, nullptr, &outputParamSize, privateData.get(),
                                                              nullptr, eventComplete);

    EXPECT_FALSE(retVal);
    EXPECT_EQ(0, PerfCounterFlags::getPerfCountersQueryDataCalled);
}

TEST_P(PerformanceCountersProcessEventTest, givenCorrectInputParamWhenProcessEventPerfCountersIsCalledAndEventIsCompletedThenReturnsTrue) {
    eventComplete = GetParam();
    EXPECT_EQ(0ull, outputParamSize);
    auto retVal = performanceCountersBase->processEventReport(inputParamSize, inputParam.get(), &outputParamSize, privateData.get(),
                                                              nullptr, eventComplete);

    if (eventComplete) {
        EXPECT_TRUE(retVal);
        EXPECT_EQ(1, PerfCounterFlags::getPerfCountersQueryDataCalled);
        EXPECT_EQ(outputParamSize, inputParamSize);
    } else {
        EXPECT_FALSE(retVal);
        EXPECT_EQ(0, PerfCounterFlags::getPerfCountersQueryDataCalled);
        EXPECT_EQ(inputParamSize, outputParamSize);
    }
}

TEST_F(PerformanceCountersProcessEventTest, givenInvalidInputParamSizeWhenProcessEventPerfCountersIsCalledThenReturnsFalse) {
    EXPECT_EQ(0ull, outputParamSize);
    auto retVal = performanceCountersBase->processEventReport(inputParamSize - 1, inputParam.get(), &outputParamSize, privateData.get(),
                                                              nullptr, eventComplete);

    EXPECT_FALSE(retVal);
    EXPECT_EQ(0, PerfCounterFlags::getPerfCountersQueryDataCalled);
    EXPECT_EQ(outputParamSize, inputParamSize);
}

TEST_F(PerformanceCountersProcessEventTest, givenNullptrOutputParamSizeWhenProcessEventPerfCountersIsCalledThenDoesNotReturnsOutputSize) {
    EXPECT_EQ(0ull, outputParamSize);
    auto retVal = performanceCountersBase->processEventReport(inputParamSize, inputParam.get(), nullptr, privateData.get(),
                                                              nullptr, eventComplete);

    EXPECT_TRUE(retVal);
    EXPECT_EQ(1, PerfCounterFlags::getPerfCountersQueryDataCalled);
    EXPECT_EQ(0ull, outputParamSize);
}

TEST_F(PerformanceCountersProcessEventTest, givenNullptrInputZeroSizeWhenProcessEventPerfCountersIsCalledThenQueryProperSize) {
    EXPECT_EQ(0ull, outputParamSize);
    auto retVal = performanceCountersBase->processEventReport(0, nullptr, &outputParamSize, privateData.get(),
                                                              nullptr, eventComplete);

    EXPECT_TRUE(retVal);
    EXPECT_EQ(0, PerfCounterFlags::getPerfCountersQueryDataCalled);
    EXPECT_EQ(inputParamSize, outputParamSize);
}

TEST_F(PerformanceCountersProcessEventTest, givenNullptrInputZeroSizeAndNullptrOutputSizeWhenProcessEventPerfCountersIsCalledThenReturnFalse) {
    EXPECT_EQ(0ull, outputParamSize);
    auto retVal = performanceCountersBase->processEventReport(0, nullptr, nullptr, privateData.get(),
                                                              nullptr, eventComplete);

    EXPECT_FALSE(retVal);
    EXPECT_EQ(0, PerfCounterFlags::getPerfCountersQueryDataCalled);
    EXPECT_EQ(0ull, outputParamSize);
}

INSTANTIATE_TEST_CASE_P(
    PerfCountersTests,
    PerformanceCountersProcessEventTest,
    testing::Bool());

struct PerformanceCountersSendConfigCommandPointersTest : public PerformanceCountersTest,
                                                          public ::testing::WithParamInterface<std::tuple<bool /*pConfig is nullptr*/,
                                                                                                          bool /*pLastConfigHandle is nullptr */,
                                                                                                          bool /*pLastConfigPending is nullptr*/>> {
    void SetUp() override {
        PerformanceCountersTest::SetUp();
        createPerfCounters();
        performanceCountersBase->initialize(platformDevices[0]);
        std::tie(pConfigIsNullptr, pLastConfigHandleIsNullptr, pLastConfigPendingIsNullptr) = GetParam();
    }

    void TearDown() override {
        performanceCountersBase->shutdown();
        PerformanceCountersTest::TearDown();
    }
    bool pConfigIsNullptr;
    bool pLastConfigHandleIsNullptr;
    bool pLastConfigPendingIsNullptr;
    InstrPmRegsCfg config;
    uint32_t lastConfigHandle;
    bool lastConfigPending;
};

TEST_P(PerformanceCountersSendConfigCommandPointersTest, givenPerformanceCountersWithPassingFuncsWhenSendPmRegsCfgCommandsIsCalledWithAnyNullptrThenReturnsFalseOtherwiseTrue) {
    InstrPmRegsCfg *pConfig = nullptr;
    uint32_t *pLastConfigHandle = nullptr;
    bool *pLastConfigPending = nullptr;

    if (!pConfigIsNullptr) {
        pConfig = &config;
    }
    if (!pLastConfigHandleIsNullptr) {
        pLastConfigHandle = &lastConfigHandle;
    }
    if (!pLastConfigPendingIsNullptr) {
        lastConfigPending = false;
        pLastConfigPending = &lastConfigPending;
    }

    auto retVal = performanceCountersBase->sendPmRegsCfgCommands(pConfig, pLastConfigHandle, pLastConfigPending);
    if (pConfigIsNullptr || pLastConfigHandleIsNullptr || pLastConfigPendingIsNullptr) {
        EXPECT_FALSE(retVal);
        EXPECT_EQ(0, PerfCounterFlags::checkPmRegsCfgCalled);
        EXPECT_EQ(0, PerfCounterFlags::loadPmRegsCfgCalled);
    } else {
        EXPECT_TRUE(retVal);
        EXPECT_TRUE(lastConfigPending);
        EXPECT_EQ(1, PerfCounterFlags::checkPmRegsCfgCalled);
        EXPECT_EQ(1, PerfCounterFlags::loadPmRegsCfgCalled);
    }
}

INSTANTIATE_TEST_CASE_P(
    PerfCountersTests,
    PerformanceCountersSendConfigCommandPointersTest,
    testing::Combine(
        testing::Bool(),
        testing::Bool(),
        testing::Bool()));

struct PerformanceCountersSendConfigCommandFuncPointersTest : public PerformanceCountersTest,
                                                              public ::testing::WithParamInterface<std::tuple<bool /*check config func returns true*/,
                                                                                                              bool /*load config func returns true*/>> {
    void SetUp() override {

        PerformanceCountersTest::SetUp();
        createPerfCounters();
        std::tie(checkConfigFuncPassing, loadConfigFuncPassing) = GetParam();
        if (checkConfigFuncPassing) {
            performanceCountersBase->setCheckPmRegsCfgFunc(checkPmRegsCfgPassing);
        } else {
            performanceCountersBase->setCheckPmRegsCfgFunc(checkPmRegsCfgFailing);
        }
        if (loadConfigFuncPassing) {
            performanceCountersBase->setLoadPmRegsCfgFunc(loadPmRegsCfgPassing);
        } else {
            performanceCountersBase->setLoadPmRegsCfgFunc(loadPmRegsCfgFailing);
        }
        lastConfigPending = false;
        performanceCountersBase->initialize(platformDevices[0]);
    }

    void TearDown() override {
        performanceCountersBase->shutdown();
        PerformanceCountersTest::TearDown();
    }
    bool checkConfigFuncPassing;
    bool loadConfigFuncPassing;
    InstrPmRegsCfg config;
    uint32_t lastConfigHandle;
    bool lastConfigPending;
};

TEST_P(PerformanceCountersSendConfigCommandFuncPointersTest, givenPerformanceCountersSendPmRegsCfgCommandsFuncWhenAllLoadConfigFuncAndCheckConfigFuncAndGetPerfmonConfigReturnTrueThenReturnsTrueOtherwiseFalse) {
    auto retVal = performanceCountersBase->sendPmRegsCfgCommands(&config, &lastConfigHandle, &lastConfigPending);
    EXPECT_EQ(1, PerfCounterFlags::checkPmRegsCfgCalled);
    if (checkConfigFuncPassing) {
        EXPECT_EQ(1, PerfCounterFlags::loadPmRegsCfgCalled);
        if (loadConfigFuncPassing) {
            EXPECT_TRUE(retVal);
            EXPECT_TRUE(lastConfigPending);
        } else {
            EXPECT_FALSE(retVal);
            EXPECT_EQ(1, PerfCounterFlags::loadPmRegsCfgCalled);
        }
    } else {
        EXPECT_FALSE(retVal);
        EXPECT_EQ(0, PerfCounterFlags::loadPmRegsCfgCalled);
    }
}

INSTANTIATE_TEST_CASE_P(
    PerfCountersTests,
    PerformanceCountersSendConfigCommandFuncPointersTest,
    testing::Combine(
        testing::Bool(),
        testing::Bool()));
