/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "mock_frequency.h"

#include <cmath>

using ::testing::_;
using ::testing::DoAll;
using ::testing::InSequence;
using ::testing::NiceMock;
using ::testing::Return;

namespace L0 {
namespace ult {

ACTION_P(SetFloat, value) {
    arg0 = value;
}

class SysmanFrequencyFixture : public DeviceFixture, public ::testing::Test {

  protected:
    SysmanImp *sysmanImp;
    zet_sysman_handle_t hSysman;
    zet_sysman_freq_handle_t hSysmanFrequency;

    Mock<OsFrequency> *pOsFrequency;
    FrequencyImp *pFrequencyImp;
    const float minFreq = 300.0;
    const float maxFreq = 1100.0;

    void SetUp() override {
        DeviceFixture::SetUp();
        sysmanImp = new SysmanImp(device->toHandle());
        pOsFrequency = new NiceMock<Mock<OsFrequency>>;
        pFrequencyImp = new FrequencyImp();
        pFrequencyImp->pOsFrequency = pOsFrequency;
        ON_CALL(*pOsFrequency, getMin(_))
            .WillByDefault(DoAll(
                SetFloat(minFreq),
                Return(ZE_RESULT_SUCCESS)));
        ON_CALL(*pOsFrequency, getMax(_))
            .WillByDefault(DoAll(
                SetFloat(maxFreq),
                Return(ZE_RESULT_SUCCESS)));
        ON_CALL(*pOsFrequency, getRequest(_))
            .WillByDefault(DoAll(
                SetFloat(minFreq),
                Return(ZE_RESULT_SUCCESS)));
        ON_CALL(*pOsFrequency, getTdp(_))
            .WillByDefault(DoAll(
                SetFloat(maxFreq),
                Return(ZE_RESULT_SUCCESS)));
        ON_CALL(*pOsFrequency, getActual(_))
            .WillByDefault(DoAll(
                SetFloat(minFreq),
                Return(ZE_RESULT_SUCCESS)));
        ON_CALL(*pOsFrequency, getEfficient(_))
            .WillByDefault(DoAll(
                SetFloat(minFreq),
                Return(ZE_RESULT_SUCCESS)));
        ON_CALL(*pOsFrequency, getMaxVal(_))
            .WillByDefault(DoAll(
                SetFloat(maxFreq),
                Return(ZE_RESULT_SUCCESS)));
        ON_CALL(*pOsFrequency, getMinVal(_))
            .WillByDefault(DoAll(
                SetFloat(minFreq),
                Return(ZE_RESULT_SUCCESS)));

        pFrequencyImp->init();
        sysmanImp->pFrequencyHandleContext->handle_list.push_back(pFrequencyImp);

        hSysman = sysmanImp->toHandle();
        hSysmanFrequency = pFrequencyImp->toHandle();
    }

    void TearDown() override {
        if (sysmanImp != nullptr) {
            delete sysmanImp;
            sysmanImp = nullptr;
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
    EXPECT_DOUBLE_EQ((100.0 / 6.0), properties.step);
    ASSERT_NE(0.0, properties.step);
}

TEST_F(SysmanFrequencyFixture, GivenValidFrequencyHandleWhenCallingzetSysmanFrequencyGetRangeThenVerifyzetSysmanFrequencyGetRangeTestCallSucceeds) {
    zet_freq_range_t limits;

    ze_result_t result = zetSysmanFrequencyGetRange(hSysmanFrequency, &limits);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_DOUBLE_EQ(minFreq, limits.min);
    EXPECT_DOUBLE_EQ(maxFreq, limits.max);
}

TEST_F(SysmanFrequencyFixture, GivenValidFrequencyHandleWhenCallingzetSysmanFrequencySetRangeThenVerifyzetSysmanFrequencySetRangeTest1CallSucceeds) {
    zet_freq_range_t limits;

    ON_CALL(*pOsFrequency, getMin(_))
        .WillByDefault(DoAll(
            SetFloat(900.0),
            Return(ZE_RESULT_SUCCESS)));
    ON_CALL(*pOsFrequency, getMax(_))
        .WillByDefault(DoAll(
            SetFloat(maxFreq),
            Return(ZE_RESULT_SUCCESS)));

    // If the new Max value is less than the old Min
    // value, the new Min must be set before the new Max
    InSequence s;
    EXPECT_CALL(*pOsFrequency, setMin(450))
        .Times(1)
        .WillOnce(Return(ZE_RESULT_SUCCESS));
    EXPECT_CALL(*pOsFrequency, setMax(600.0))
        .Times(1)
        .WillOnce(Return(ZE_RESULT_SUCCESS));

    limits.min = 450.0;
    limits.max = 600.0;
    ze_result_t result = zetSysmanFrequencySetRange(hSysmanFrequency, &limits);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

TEST_F(SysmanFrequencyFixture, GivenValidFrequencyHandleWhenCallingzetSysmanFrequencySetRangeThenVerifyzetSysmanFrequencySetRangeTest2CallSucceeds) {
    zet_freq_range_t limits;

    ON_CALL(*pOsFrequency, getMin(_))
        .WillByDefault(DoAll(
            SetFloat(450.0),
            Return(ZE_RESULT_SUCCESS)));
    ON_CALL(*pOsFrequency, getMax(_))
        .WillByDefault(DoAll(
            SetFloat(600.0),
            Return(ZE_RESULT_SUCCESS)));

    // If the new Min value is greater than the old Max
    // value, the new Max must be set before the new Min
    InSequence s;
    EXPECT_CALL(*pOsFrequency, setMax(maxFreq))
        .Times(1)
        .WillOnce(Return(ZE_RESULT_SUCCESS));
    EXPECT_CALL(*pOsFrequency, setMin(900.0))
        .Times(1)
        .WillOnce(Return(ZE_RESULT_SUCCESS));

    limits.min = 900.0;
    limits.max = maxFreq;
    ze_result_t result = zetSysmanFrequencySetRange(hSysmanFrequency, &limits);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

TEST_F(SysmanFrequencyFixture, GivenValidFrequencyHandleWhenCallingzetSysmanFrequencySetRangeThenVerifyzetSysmanFrequencySetRangeTest3CallSucceeds) {
    zet_freq_range_t limits;

    // Verify that Max must be within range.
    limits.min = minFreq;
    limits.max = 1200.0;
    ze_result_t result = zetSysmanFrequencySetRange(hSysmanFrequency, &limits);

    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, result);
}

TEST_F(SysmanFrequencyFixture, GivenValidFrequencyHandleWhenCallingzetSysmanFrequencySetRangeThenVerifyzetSysmanFrequencySetRangeTest4CallSucceeds) {
    zet_freq_range_t limits;

    // Verify that Min must be within range.
    limits.min = 200.0;
    limits.max = maxFreq;
    ze_result_t result = zetSysmanFrequencySetRange(hSysmanFrequency, &limits);

    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, result);
}

TEST_F(SysmanFrequencyFixture, GivenValidFrequencyHandleWhenCallingzetSysmanFrequencySetRangeThenVerifyzetSysmanFrequencySetRangeTest5CallSucceeds) {
    zet_freq_range_t limits;

    // Verify that values must be multiples of step (16.666667).
    limits.min = 310.0;
    limits.max = maxFreq;
    ze_result_t result = zetSysmanFrequencySetRange(hSysmanFrequency, &limits);

    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, result);
}

TEST_F(SysmanFrequencyFixture, GivenValidFrequencyHandleWhenCallingzetSysmanFrequencyGetStateThenVerifyzetSysmanFrequencyGetStateTestCallSucceeds) {
    zet_freq_state_t state;

    EXPECT_CALL(*pOsFrequency, getRequest(_))
        .Times(1)
        .WillOnce(DoAll(
            SetFloat(450.0),
            Return(ZE_RESULT_SUCCESS)));
    EXPECT_CALL(*pOsFrequency, getTdp(_))
        .Times(1)
        .WillOnce(DoAll(
            SetFloat(1200.0),
            Return(ZE_RESULT_SUCCESS)));
    EXPECT_CALL(*pOsFrequency, getEfficient(_))
        .Times(1)
        .WillOnce(DoAll(
            SetFloat(400.0),
            Return(ZE_RESULT_SUCCESS)));
    EXPECT_CALL(*pOsFrequency, getActual(_))
        .Times(1)
        .WillOnce(DoAll(
            SetFloat(550.0),
            Return(ZE_RESULT_SUCCESS)));
    EXPECT_CALL(*pOsFrequency, getThrottleReasons(_))
        .Times(1)
        .WillOnce(Return(ZE_RESULT_ERROR_UNKNOWN));

    ze_result_t result = zetSysmanFrequencyGetState(hSysmanFrequency, &state);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_DOUBLE_EQ(450.0, state.request);
    EXPECT_DOUBLE_EQ(1200.0, state.tdp);
    EXPECT_DOUBLE_EQ(400.0, state.efficient);
    EXPECT_DOUBLE_EQ(550.0, state.actual);
    EXPECT_EQ(0u, state.throttleReasons);
}

} // namespace ult
} // namespace L0
