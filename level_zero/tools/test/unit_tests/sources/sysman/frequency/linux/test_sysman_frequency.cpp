/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/tools/source/sysman/frequency/linux/os_frequency_imp.h"
#include "level_zero/tools/source/sysman/sysman_imp.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "mock_sysfs_frequency.h"

#include <cmath>

using ::testing::_;
using ::testing::DoAll;
using ::testing::InSequence;
using ::testing::Invoke;
using ::testing::Matcher;
using ::testing::NiceMock;
using ::testing::Return;

namespace L0 {
namespace ult {

constexpr double minFreq = 300.0;
constexpr double maxFreq = 1100.0;
constexpr double step = 100.0 / 6;
constexpr double request = 300.0;
constexpr double tdp = 1100.0;
constexpr double actual = 300.0;
constexpr double efficient = 300.0;
constexpr double maxVal = 1100.0;
constexpr double minVal = 300.0;
constexpr uint32_t numClocks = static_cast<uint32_t>((maxFreq - minFreq) / step) + 1;

class SysmanFrequencyFixture : public DeviceFixture, public ::testing::Test {

  protected:
    std::unique_ptr<SysmanImp> sysmanImp;
    zet_sysman_handle_t hSysman;
    zet_sysman_freq_handle_t hSysmanFrequency;
    Mock<FrequencySysfsAccess> *pSysfsAccess = nullptr;

    OsFrequency *pOsFrequency = nullptr;
    FrequencyImp *pFrequencyImp = nullptr;
    PublicLinuxFrequencyImp linuxFrequencyImp;

    double clockValue(const double calculatedClock) {
        // i915 specific. frequency step is a fraction
        // However, the i915 represents all clock
        // rates as integer values. So clocks are
        // rounded to the nearest integer.
        uint32_t actualClock = static_cast<uint32_t>(calculatedClock + 0.5);
        return static_cast<double>(actualClock);
    }

    void SetUp() override {
        DeviceFixture::SetUp();
        sysmanImp = std::make_unique<SysmanImp>(device->toHandle());
        pSysfsAccess = new NiceMock<Mock<FrequencySysfsAccess>>;
        linuxFrequencyImp.pSysfsAccess = pSysfsAccess;
        pOsFrequency = static_cast<OsFrequency *>(&linuxFrequencyImp);
        pFrequencyImp = new FrequencyImp();
        pFrequencyImp->pOsFrequency = pOsFrequency;
        pSysfsAccess->setVal(minFreqFile, minFreq);
        pSysfsAccess->setVal(maxFreqFile, maxFreq);
        pSysfsAccess->setVal(requestFreqFile, request);
        pSysfsAccess->setVal(tdpFreqFile, tdp);
        pSysfsAccess->setVal(actualFreqFile, actual);
        pSysfsAccess->setVal(efficientFreqFile, efficient);
        pSysfsAccess->setVal(maxValFreqFile, maxVal);
        pSysfsAccess->setVal(minValFreqFile, minVal);
        ON_CALL(*pSysfsAccess, read(_, _))
            .WillByDefault(::testing::Invoke(pSysfsAccess, &Mock<FrequencySysfsAccess>::getVal));
        ON_CALL(*pSysfsAccess, write(_, _))
            .WillByDefault(::testing::Invoke(pSysfsAccess, &Mock<FrequencySysfsAccess>::setVal));
        pFrequencyImp->init();
        sysmanImp->pFrequencyHandleContext->handle_list.push_back(pFrequencyImp);
        hSysman = sysmanImp->toHandle();
        hSysmanFrequency = pFrequencyImp->toHandle();
    }

    void TearDown() override {
        pFrequencyImp->pOsFrequency = nullptr;

        if (pSysfsAccess != nullptr) {
            delete pSysfsAccess;
            pSysfsAccess = nullptr;
        }
        DeviceFixture::TearDown();
    }
};

TEST_F(SysmanFrequencyFixture, GivenComponentCountZeroWhenCallingzetSysmanFrequencyGetThenNonZeroCountIsReturnedAndVerifyzetSysmanFrequencyGetCallSucceds) {
    zet_sysman_freq_handle_t freqHandle;
    uint32_t count;

    ze_result_t result = zetSysmanFrequencyGet(hSysman, &count, NULL);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_GT(count, 0u);

    uint32_t testCount = count + 1;
    result = zetSysmanFrequencyGet(hSysman, &testCount, NULL);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(testCount, count);

    result = zetSysmanFrequencyGet(hSysman, &count, &freqHandle);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(freqHandle, nullptr);
    EXPECT_GT(count, 0u);
}

TEST_F(SysmanFrequencyFixture, GivenValidFrequencyHandleWhenCallingzetSysmanFrequencyGetPropertiesThenVerifyzetSysmanFrequencyGetPropertiesCallSucceeds) {
    zet_freq_properties_t properties;

    ze_result_t result = zetSysmanFrequencyGetProperties(hSysmanFrequency, &properties);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(ZET_FREQ_DOMAIN_GPU, properties.type);
    EXPECT_FALSE(properties.onSubdevice);
    EXPECT_DOUBLE_EQ(maxFreq, properties.max);
    EXPECT_DOUBLE_EQ(minFreq, properties.min);
    EXPECT_TRUE(properties.canControl);
    EXPECT_DOUBLE_EQ(step, properties.step);
    ASSERT_NE(0.0, properties.step);
}

TEST_F(SysmanFrequencyFixture, GivenValidFrequencyHandleAndZeroCountWhenCallingzetSysmanFrequencyGetAvailableClocksCallSucceeds) {
    uint32_t count = 0;

    ze_result_t result = zetSysmanFrequencyGetAvailableClocks(hSysmanFrequency, &count, nullptr);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(numClocks, count);
}

TEST_F(SysmanFrequencyFixture, GivenValidFrequencyHandleAndCorrectCountWhenCallingzetSysmanFrequencyGetAvailableClocksCallSucceeds) {
    uint32_t count = 0;

    ze_result_t result = zetSysmanFrequencyGetAvailableClocks(hSysmanFrequency, &count, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(numClocks, count);

    double *clocks = new double[count];
    result = zetSysmanFrequencyGetAvailableClocks(hSysmanFrequency, &count, clocks);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(numClocks, count);
    for (uint32_t i = 0; i < count; i++) {
        EXPECT_DOUBLE_EQ(clockValue(minFreq + (step * i)), clocks[i]);
    }
    delete[] clocks;
}

TEST_F(SysmanFrequencyFixture, GivenValidFrequencyHandleWhenCallingzetSysmanFrequencyGetRangeThenVerifyzetSysmanFrequencyGetRangeTestCallSucceeds) {
    zet_freq_range_t limits;

    ze_result_t result = zetSysmanFrequencyGetRange(hSysmanFrequency, &limits);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_DOUBLE_EQ(minFreq, limits.min);
    EXPECT_DOUBLE_EQ(maxFreq, limits.max);
}

TEST_F(SysmanFrequencyFixture, GivenValidFrequencyHandleWhenCallingzetSysmanFrequencySetRangeThenVerifyzetSysmanFrequencySetRangeTest1CallSucceeds) {
    const double startingMin = 900.0;
    const double newMax = 600.0;

    zet_freq_range_t limits;

    pSysfsAccess->setVal(minFreqFile, startingMin);

    // If the new Max value is less than the old Min
    // value, the new Min must be set before the new Max

    limits.min = minFreq;
    limits.max = newMax;
    ze_result_t result = zetSysmanFrequencySetRange(hSysmanFrequency, &limits);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = zetSysmanFrequencyGetRange(hSysmanFrequency, &limits);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_DOUBLE_EQ(minFreq, limits.min);
    EXPECT_DOUBLE_EQ(newMax, limits.max);
}

TEST_F(SysmanFrequencyFixture, GivenValidFrequencyHandleWhenCallingzetSysmanFrequencySetRangeThenVerifyzetSysmanFrequencySetRangeTest2CallSucceeds) {
    const double startingMax = 600.0;
    const double newMin = 900.0;

    zet_freq_range_t limits;

    pSysfsAccess->setVal(maxFreqFile, startingMax);

    // If the new Min value is greater than the old Max
    // value, the new Max must be set before the new Min

    limits.min = newMin;
    limits.max = maxFreq;
    ze_result_t result = zetSysmanFrequencySetRange(hSysmanFrequency, &limits);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = zetSysmanFrequencyGetRange(hSysmanFrequency, &limits);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_DOUBLE_EQ(newMin, limits.min);
    EXPECT_DOUBLE_EQ(maxFreq, limits.max);
}

TEST_F(SysmanFrequencyFixture, GivenValidFrequencyHandleWhenCallingzetSysmanFrequencySetRangeThenVerifyzetSysmanFrequencySetRangeTest3CallSucceeds) {
    zet_freq_range_t limits;

    // Verify that Max must be within range.
    limits.min = minFreq;
    limits.max = clockValue(maxFreq + step);
    ze_result_t result = zetSysmanFrequencySetRange(hSysmanFrequency, &limits);

    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, result);
}

TEST_F(SysmanFrequencyFixture, GivenValidFrequencyHandleWhenCallingzetSysmanFrequencySetRangeThenVerifyzetSysmanFrequencySetRangeTest4CallSucceeds) {
    zet_freq_range_t limits;

    // Verify that Min must be within range.
    limits.min = clockValue(minFreq - step);
    limits.max = maxFreq;
    ze_result_t result = zetSysmanFrequencySetRange(hSysmanFrequency, &limits);

    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, result);
}

TEST_F(SysmanFrequencyFixture, GivenValidFrequencyHandleWhenCallingzetSysmanFrequencySetRangeThenVerifyzetSysmanFrequencySetRangeTest5CallSucceeds) {
    zet_freq_range_t limits;

    // Verify that values must be multiples of step.
    limits.min = clockValue(minFreq + (step * 0.5));
    limits.max = maxFreq;
    ze_result_t result = zetSysmanFrequencySetRange(hSysmanFrequency, &limits);

    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, result);
}

TEST_F(SysmanFrequencyFixture, GivenValidFrequencyHandleWhenCallingzetSysmanFrequencyGetStateThenVerifyzetSysmanFrequencyGetStateTestCallSucceeds) {
    const double testRequestValue = 450.0;
    const double testTdpValue = 1200.0;
    const double testEfficientValue = 400.0;
    const double testActualValue = 550.0;

    zet_freq_state_t state;

    pSysfsAccess->setVal(requestFreqFile, testRequestValue);
    pSysfsAccess->setVal(tdpFreqFile, testTdpValue);
    pSysfsAccess->setVal(actualFreqFile, testActualValue);
    pSysfsAccess->setVal(efficientFreqFile, testEfficientValue);

    ze_result_t result = zetSysmanFrequencyGetState(hSysmanFrequency, &state);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_DOUBLE_EQ(testRequestValue, state.request);
    EXPECT_DOUBLE_EQ(testTdpValue, state.tdp);
    EXPECT_DOUBLE_EQ(testEfficientValue, state.efficient);
    EXPECT_DOUBLE_EQ(testActualValue, state.actual);
    EXPECT_EQ(0u, state.throttleReasons);
}

} // namespace ult
} // namespace L0
