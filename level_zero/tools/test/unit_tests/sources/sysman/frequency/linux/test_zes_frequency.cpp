/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/test/unit_tests/sources/sysman/linux/mock_sysman_fixture.h"

#include "gtest/gtest.h"
#include "mock_sysfs_frequency.h"

#include <cmath>

extern bool sysmanUltsEnable;

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
constexpr uint32_t handleComponentCount = 1u;
class SysmanDeviceFrequencyFixture : public SysmanDeviceFixture {
  protected:
    std::unique_ptr<MockFrequencySysfsAccess> pSysfsAccess;
    SysfsAccess *pSysfsAccessOld = nullptr;
    std::vector<ze_device_handle_t> deviceHandles;

    void SetUp() override {
        if (!sysmanUltsEnable) {
            GTEST_SKIP();
        }
        SysmanDeviceFixture::SetUp();
        pSysfsAccessOld = pLinuxSysmanImp->pSysfsAccess;
        pSysfsAccess = std::make_unique<MockFrequencySysfsAccess>();
        pLinuxSysmanImp->pSysfsAccess = pSysfsAccess.get();

        pSysfsAccess->setVal(minFreqFile, minFreq);
        pSysfsAccess->setVal(maxFreqFile, maxFreq);
        pSysfsAccess->setVal(requestFreqFile, request);
        pSysfsAccess->setVal(tdpFreqFile, tdp);
        pSysfsAccess->setVal(actualFreqFile, actual);
        pSysfsAccess->setVal(efficientFreqFile, efficient);
        pSysfsAccess->setVal(maxValFreqFile, maxVal);
        pSysfsAccess->setVal(minValFreqFile, minVal);

        // delete handles created in initial SysmanDeviceHandleContext::init() call
        for (auto handle : pSysmanDeviceImp->pFrequencyHandleContext->handleList) {
            delete handle;
        }
        pSysmanDeviceImp->pFrequencyHandleContext->handleList.clear();
        uint32_t subDeviceCount = 0;
        // We received a device handle. Check for subdevices in this device
        Device::fromHandle(device->toHandle())->getSubDevices(&subDeviceCount, nullptr);
        if (subDeviceCount == 0) {
            deviceHandles.resize(1, device->toHandle());
        } else {
            deviceHandles.resize(subDeviceCount, nullptr);
            Device::fromHandle(device->toHandle())->getSubDevices(&subDeviceCount, deviceHandles.data());
        }
        getFreqHandles(0);
    }

    void TearDown() override {
        if (!sysmanUltsEnable) {
            GTEST_SKIP();
        }
        pLinuxSysmanImp->pSysfsAccess = pSysfsAccessOld;
        SysmanDeviceFixture::TearDown();
    }

    double clockValue(const double calculatedClock) {
        // i915 specific. frequency step is a fraction
        // However, the i915 represents all clock
        // rates as integer values. So clocks are
        // rounded to the nearest integer.
        uint32_t actualClock = static_cast<uint32_t>(calculatedClock + 0.5);
        return static_cast<double>(actualClock);
    }

    std::vector<zes_freq_handle_t> getFreqHandles(uint32_t count) {
        std::vector<zes_freq_handle_t> handles(count, nullptr);
        EXPECT_EQ(zesDeviceEnumFrequencyDomains(device->toHandle(), &count, handles.data()), ZE_RESULT_SUCCESS);
        return handles;
    }
};

TEST_F(SysmanDeviceFrequencyFixture, GivenComponentCountZeroWhenEnumeratingFrequencyHandlesThenNonZeroCountIsReturnedAndCallSucceds) {
    uint32_t count = 0U;

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEnumFrequencyDomains(device->toHandle(), &count, nullptr));
    EXPECT_EQ(count, handleComponentCount);

    uint32_t testCount = count + 1;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEnumFrequencyDomains(device->toHandle(), &testCount, nullptr));
    EXPECT_EQ(count, testCount);

    auto handles = getFreqHandles(count);
    for (auto handle : handles) {
        EXPECT_NE(handle, nullptr);
    }
}

TEST_F(SysmanDeviceFrequencyFixture, GivenComponentCountZeroAndValidPtrWhenEnumeratingFrequencyHandlesThenNonZeroCountAndNoHandlesAreReturnedAndCallSucceds) {
    uint32_t count = 0U;
    zes_freq_handle_t handle = static_cast<zes_freq_handle_t>(0UL);

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEnumFrequencyDomains(device->toHandle(), &count, &handle));
    EXPECT_EQ(count, handleComponentCount);
    EXPECT_EQ(handle, static_cast<zes_freq_handle_t>(0UL));
}

TEST_F(SysmanDeviceFrequencyFixture, GivenActualComponentCountTwoWhenTryingToGetOneComponentOnlyThenOneComponentIsReturnedAndCountUpdated) {
    auto pFrequencyHandleContextTest = std::make_unique<FrequencyHandleContext>(pOsSysman);
    pFrequencyHandleContextTest->handleList.push_back(new FrequencyImp(pOsSysman, device->toHandle(), ZES_FREQ_DOMAIN_GPU));
    pFrequencyHandleContextTest->handleList.push_back(new FrequencyImp(pOsSysman, device->toHandle(), ZES_FREQ_DOMAIN_GPU));

    uint32_t count = 1;
    std::vector<zes_freq_handle_t> phFrequency(count, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, pFrequencyHandleContextTest->frequencyGet(&count, phFrequency.data()));
    EXPECT_EQ(count, 1u);
}

TEST_F(SysmanDeviceFrequencyFixture, GivenValidFrequencyHandleWhenCallingzesFrequencyGetPropertiesThenSuccessIsReturned) {
    auto handles = getFreqHandles(handleComponentCount);
    for (auto handle : handles) {
        EXPECT_NE(handle, nullptr);
        zes_freq_properties_t properties;
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesFrequencyGetProperties(handle, &properties));
        EXPECT_EQ(nullptr, properties.pNext);
        EXPECT_EQ(ZES_FREQ_DOMAIN_GPU, properties.type);
        EXPECT_FALSE(properties.onSubdevice);
        EXPECT_DOUBLE_EQ(maxFreq, properties.max);
        EXPECT_DOUBLE_EQ(minFreq, properties.min);
        EXPECT_TRUE(properties.canControl);
    }
}

TEST_F(SysmanDeviceFrequencyFixture, GivenValidFrequencyHandleAndZeroCountWhenCallingzesFrequencyGetAvailableClocksThenCallSucceeds) {
    auto handles = getFreqHandles(handleComponentCount);
    for (auto handle : handles) {
        EXPECT_NE(handle, nullptr);
        uint32_t count = 0;
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesFrequencyGetAvailableClocks(handle, &count, nullptr));
        EXPECT_EQ(numClocks, count);
    }
}

TEST_F(SysmanDeviceFrequencyFixture, GivenValidFrequencyHandleAndZeroCountWhenCountIsMoreThanNumClocksThenCallSucceeds) {
    auto handles = getFreqHandles(handleComponentCount);
    for (auto handle : handles) {
        EXPECT_NE(handle, nullptr);
        uint32_t count = 80;
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesFrequencyGetAvailableClocks(handle, &count, nullptr));
        EXPECT_EQ(numClocks, count);
    }
}

TEST_F(SysmanDeviceFrequencyFixture, GivenValidFrequencyHandleAndZeroCountWhenCountIsLessThanNumClocksThenCallSucceeds) {
    auto handles = getFreqHandles(handleComponentCount);
    for (auto handle : handles) {
        EXPECT_NE(handle, nullptr);
        uint32_t count = 20;
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesFrequencyGetAvailableClocks(handle, &count, nullptr));
    }
}

TEST_F(SysmanDeviceFrequencyFixture, GivenValidFrequencyHandleAndCorrectCountWhenCallingzesFrequencyGetAvailableClocksThenCallSucceeds) {
    auto handles = getFreqHandles(handleComponentCount);
    for (auto handle : handles) {
        uint32_t count = 0;

        EXPECT_EQ(ZE_RESULT_SUCCESS, zesFrequencyGetAvailableClocks(handle, &count, nullptr));
        EXPECT_EQ(numClocks, count);

        double *clocks = new double[count];
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesFrequencyGetAvailableClocks(handle, &count, clocks));
        EXPECT_EQ(numClocks, count);
        for (uint32_t i = 0; i < count; i++) {
            EXPECT_DOUBLE_EQ(clockValue(minFreq + (step * i)), clocks[i]);
        }
        delete[] clocks;
    }
}

TEST_F(SysmanDeviceFrequencyFixture, GivenValidateFrequencyGetRangeWhengetMaxAndgetMinFailsThenFrequencyGetRangeCallReturnsNegativeValuesForRange) {
    auto pFrequencyImp = std::make_unique<FrequencyImp>(pOsSysman, device->toHandle(), ZES_FREQ_DOMAIN_GPU);
    zes_freq_range_t limit = {};
    pSysfsAccess->mockReadDoubleValResult = ZE_RESULT_ERROR_NOT_AVAILABLE;
    EXPECT_EQ(ZE_RESULT_SUCCESS, pFrequencyImp->frequencyGetRange(&limit));
    EXPECT_EQ(-1, limit.max);
    EXPECT_EQ(-1, limit.min);

    pSysfsAccess->mockReadDoubleValResult = ZE_RESULT_ERROR_UNKNOWN;
    EXPECT_EQ(ZE_RESULT_SUCCESS, pFrequencyImp->frequencyGetRange(&limit));
    EXPECT_EQ(-1, limit.max);
    EXPECT_EQ(-1, limit.min);
}

TEST_F(SysmanDeviceFrequencyFixture, GivenValidFrequencyHandleWhenCallingzesFrequencyGetRangeThenVerifyzesFrequencyGetRangeTestCallSucceeds) {
    auto handles = getFreqHandles(handleComponentCount);
    for (auto handle : handles) {
        zes_freq_range_t limits;
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesFrequencyGetRange(handle, &limits));
        EXPECT_DOUBLE_EQ(minFreq, limits.min);
        EXPECT_DOUBLE_EQ(maxFreq, limits.max);
    }
}

TEST_F(SysmanDeviceFrequencyFixture, GivenValidFrequencyLimitsWhenCallingFrequencySetRangeForFailures1ThenAPIExitsGracefully) {
    auto pFrequencyImp = std::make_unique<FrequencyImp>(pOsSysman, device->toHandle(), ZES_FREQ_DOMAIN_GPU);
    zes_freq_range_t limits = {};

    // Verify that Max must be within range.
    limits.min = minFreq;
    limits.max = 600.0;
    pSysfsAccess->mockWriteMinResult = ZE_RESULT_ERROR_NOT_AVAILABLE;
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, pFrequencyImp->frequencySetRange(&limits));

    pSysfsAccess->mockWriteMinResult = ZE_RESULT_ERROR_UNKNOWN;
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, pFrequencyImp->frequencySetRange(&limits));
}

TEST_F(SysmanDeviceFrequencyFixture, GivenValidFrequencyLimitsWhenCallingFrequencySetRangeForFailures2ThenAPIExitsGracefully) {
    auto pFrequencyImp = std::make_unique<FrequencyImp>(pOsSysman, device->toHandle(), ZES_FREQ_DOMAIN_GPU);
    zes_freq_range_t limits = {};

    // Verify that Max must be within range.
    limits.min = 900.0;
    limits.max = maxFreq;
    pSysfsAccess->mockWriteMaxResult = ZE_RESULT_ERROR_NOT_AVAILABLE;
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, pFrequencyImp->frequencySetRange(&limits));

    pSysfsAccess->mockWriteMaxResult = ZE_RESULT_ERROR_UNKNOWN;
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, pFrequencyImp->frequencySetRange(&limits));
}

TEST_F(SysmanDeviceFrequencyFixture, GivenValidFrequencyHandleWhenCallingzesFrequencySetRangeThenVerifyzesFrequencySetRangeTest1CallSucceeds) {
    auto handles = getFreqHandles(handleComponentCount);
    for (auto handle : handles) {
        const double startingMin = 900.0;
        const double newMax = 600.0;
        zes_freq_range_t limits;

        pSysfsAccess->setVal(minFreqFile, startingMin);
        // If the new Max value is less than the old Min
        // value, the new Min must be set before the new Max
        limits.min = minFreq;
        limits.max = newMax;
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesFrequencySetRange(handle, &limits));
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesFrequencyGetRange(handle, &limits));
        EXPECT_DOUBLE_EQ(minFreq, limits.min);
        EXPECT_DOUBLE_EQ(newMax, limits.max);
        EXPECT_DOUBLE_EQ(pSysfsAccess->mockBoost, limits.max);
    }
}

TEST_F(SysmanDeviceFrequencyFixture, GivenNegativeRangeSetWhenGetRangeIsCalledThenReturnsValueFromDefaultPath) {
    auto handles = getFreqHandles(handleComponentCount);
    for (auto &handle : handles) {
        const double negativeMin = -1;
        const double negativeMax = -1;
        zes_freq_range_t limits;

        limits.min = negativeMin;
        limits.max = negativeMax;
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesFrequencySetRange(handle, &limits));
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesFrequencyGetRange(handle, &limits));
        EXPECT_EQ(pSysfsAccess->mockDefaultMin, limits.min);
        EXPECT_EQ(pSysfsAccess->mockDefaultMax, limits.max);
        EXPECT_DOUBLE_EQ(pSysfsAccess->mockBoost, limits.max);
    }
}

TEST_F(SysmanDeviceFrequencyFixture, GivenNegativeRangeWhenSetRangeIsCalledAndSettingMaxValueFailsThenFailureIsReturned) {
    pSysfsAccess->mockWriteMaxResult = ZE_RESULT_ERROR_NOT_AVAILABLE;
    auto handles = getFreqHandles(handleComponentCount);
    for (auto &handle : handles) {
        const double negativeMin = -1;
        const double negativeMax = -1;
        zes_freq_range_t limits;

        limits.min = negativeMin;
        limits.max = negativeMax;
        EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesFrequencySetRange(handle, &limits));
    }
}

TEST_F(SysmanDeviceFrequencyFixture, GivenNegativeRangeWhenSetRangeIsCalledAndGettingDefaultMaxValueFailsThenNoFreqRangeIsInEffect) {
    pSysfsAccess->mockReadDefaultMaxResult = ZE_RESULT_ERROR_NOT_AVAILABLE;
    auto handles = getFreqHandles(handleComponentCount);
    for (auto &handle : handles) {
        const double negativeMin = -1;
        const double negativeMax = -1;
        zes_freq_range_t limits;

        limits.min = negativeMin;
        limits.max = negativeMax;
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesFrequencySetRange(handle, &limits));
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesFrequencyGetRange(handle, &limits));
        EXPECT_DOUBLE_EQ(-1, limits.min);
        EXPECT_DOUBLE_EQ(-1, limits.max);
    }
}

TEST_F(SysmanDeviceFrequencyFixture, GivenValidFrequencyHandleWhenCallingzesFrequencySetRangeThenVerifyzesFrequencySetRangeTest2CallSucceeds) {
    auto handles = getFreqHandles(handleComponentCount);
    for (auto handle : handles) {
        const double startingMax = 600.0;
        const double newMin = 900.0;
        zes_freq_range_t limits;

        pSysfsAccess->setVal(maxFreqFile, startingMax);
        // If the new Min value is greater than the old Max
        // value, the new Max must be set before the new Min
        limits.min = newMin;
        limits.max = maxFreq;
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesFrequencySetRange(handle, &limits));
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesFrequencyGetRange(handle, &limits));
        EXPECT_DOUBLE_EQ(newMin, limits.min);
        EXPECT_DOUBLE_EQ(maxFreq, limits.max);
        EXPECT_DOUBLE_EQ(pSysfsAccess->mockBoost, limits.max);
    }
}

TEST_F(SysmanDeviceFrequencyFixture, GivenInvalidFrequencyLimitsWhenCallingFrequencySetRangeThenVerifyFrequencySetRangeTest4ReturnsError) {
    auto pFrequencyImp = std::make_unique<FrequencyImp>(pOsSysman, device->toHandle(), ZES_FREQ_DOMAIN_GPU);
    zes_freq_range_t limits;

    // Verify that Max must be greater than min range.
    limits.min = clockValue(maxFreq + step);
    limits.max = minFreq;
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, pFrequencyImp->frequencySetRange(&limits));
}

TEST_F(SysmanDeviceFrequencyFixture, GivenValidFrequencyHandleWhenCallingzesFrequencyGetStateThenVerifyzesFrequencyGetStateTestCallSucceeds) {
    auto handles = getFreqHandles(handleComponentCount);
    for (auto handle : handles) {
        const double testRequestValue = 450.0;
        const double testTdpValue = 1200.0;
        const double testEfficientValue = 400.0;
        const double testActualValue = 550.0;
        const uint32_t invalidReason = 0;
        zes_freq_state_t state{ZES_STRUCTURE_TYPE_FREQ_STATE};

        pSysfsAccess->setValU32(throttleReasonStatusFile, invalidReason);
        pSysfsAccess->setVal(requestFreqFile, testRequestValue);
        pSysfsAccess->setVal(tdpFreqFile, testTdpValue);
        pSysfsAccess->setVal(actualFreqFile, testActualValue);
        pSysfsAccess->setVal(efficientFreqFile, testEfficientValue);
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesFrequencyGetState(handle, &state));
        EXPECT_DOUBLE_EQ(testRequestValue, state.request);
        EXPECT_DOUBLE_EQ(testTdpValue, state.tdp);
        EXPECT_DOUBLE_EQ(testEfficientValue, state.efficient);
        EXPECT_DOUBLE_EQ(testActualValue, state.actual);
        EXPECT_EQ(0u, state.throttleReasons);
        EXPECT_EQ(nullptr, state.pNext);
        EXPECT_LE(state.currentVoltage, 0);
    }
}

TEST_F(SysmanDeviceFrequencyFixture, GivenValidFrequencyHandleWhenCallingzesFrequencyGetStateWithLegacyPathThenVerifyzesFrequencyGetStateTestCallSucceeds) {
    pSysfsAccess->isLegacy = true;
    pSysfsAccess->directoryExistsResult = false;
    for (auto handle : pSysmanDeviceImp->pFrequencyHandleContext->handleList) {
        delete handle;
    }
    pSysmanDeviceImp->pFrequencyHandleContext->handleList.clear();
    pSysmanDeviceImp->pFrequencyHandleContext->init(deviceHandles);

    auto handles = getFreqHandles(handleComponentCount);
    for (auto handle : handles) {
        const double testRequestValue = 400.0;
        const double testTdpValue = 1100.0;
        const double testEfficientValue = 300.0;
        const double testActualValue = 550.0;
        zes_freq_state_t state{ZES_STRUCTURE_TYPE_FREQ_STATE};
        uint32_t validReason = 1;
        uint32_t setAllThrottleReasons = (ZES_FREQ_THROTTLE_REASON_FLAG_THERMAL_LIMIT |
                                          ZES_FREQ_THROTTLE_REASON_FLAG_CURRENT_LIMIT |
                                          ZES_FREQ_THROTTLE_REASON_FLAG_BURST_PWR_CAP |
                                          ZES_FREQ_THROTTLE_REASON_FLAG_AVE_PWR_CAP);

        pSysfsAccess->setValLegacy(requestFreqFileLegacy, testRequestValue);
        pSysfsAccess->setValLegacy(tdpFreqFileLegacy, testTdpValue);
        pSysfsAccess->setValLegacy(actualFreqFileLegacy, testActualValue);
        pSysfsAccess->setValLegacy(efficientFreqFileLegacy, testEfficientValue);
        pSysfsAccess->setValU32Legacy(throttleReasonStatusFileLegacy, validReason);
        pSysfsAccess->setValU32Legacy(throttleReasonPL1FileLegacy, validReason);
        pSysfsAccess->setValU32Legacy(throttleReasonPL2FileLegacy, validReason);
        pSysfsAccess->setValU32Legacy(throttleReasonPL4FileLegacy, validReason);
        pSysfsAccess->setValU32Legacy(throttleReasonThermalFileLegacy, validReason);
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesFrequencyGetState(handle, &state));
        EXPECT_DOUBLE_EQ(testRequestValue, state.request);
        EXPECT_DOUBLE_EQ(testTdpValue, state.tdp);
        EXPECT_DOUBLE_EQ(testEfficientValue, state.efficient);
        EXPECT_DOUBLE_EQ(testActualValue, state.actual);
        EXPECT_EQ(setAllThrottleReasons, state.throttleReasons);
        EXPECT_EQ(nullptr, state.pNext);
        EXPECT_LE(state.currentVoltage, 0);
    }
}

TEST_F(SysmanDeviceFrequencyFixture, GivenValidFrequencyHandleWhenCallingzesFrequencyGetStateThenVerifyzesFrequencyThrottleReasonsStatusforInvalidReasons) {
    pSysfsAccess->mockReadVal32Result = ZE_RESULT_ERROR_NOT_AVAILABLE;
    zes_freq_state_t state{ZES_STRUCTURE_TYPE_FREQ_STATE};
    uint32_t validReason = 1;
    uint32_t invalidReason = 0;
    uint32_t unsetAllThrottleReasons = 0u;
    PublicLinuxFrequencyImp linuxFrequencyImp(pOsSysman, 0, 0, ZES_FREQ_DOMAIN_GPU);
    pSysfsAccess->setValU32(throttleReasonStatusFile, invalidReason);
    pSysfsAccess->setValU32(throttleReasonPL1File, validReason);
    pSysfsAccess->setValU32(throttleReasonPL2File, validReason);
    pSysfsAccess->setValU32(throttleReasonPL4File, validReason);
    pSysfsAccess->setValU32(throttleReasonThermalFile, validReason);

    EXPECT_EQ(ZE_RESULT_SUCCESS, linuxFrequencyImp.osFrequencyGetState(&state));
    EXPECT_EQ(unsetAllThrottleReasons, state.throttleReasons);
}

TEST_F(SysmanDeviceFrequencyFixture, GivenValidFrequencyHandleWhenCallingzesFrequencyGetStateThenVerifyzesFrequencyThrottleReasonAveragePower) {
    auto handles = getFreqHandles(handleComponentCount);
    for (auto handle : handles) {
        zes_freq_state_t state{ZES_STRUCTURE_TYPE_FREQ_STATE};
        uint32_t validReason = 1;
        pSysfsAccess->setValU32(throttleReasonStatusFile, validReason);
        pSysfsAccess->setValU32(throttleReasonPL1File, validReason);
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesFrequencyGetState(handle, &state));
        EXPECT_EQ((ZES_FREQ_THROTTLE_REASON_FLAG_AVE_PWR_CAP), state.throttleReasons);
        EXPECT_EQ(nullptr, state.pNext);
    }
}

TEST_F(SysmanDeviceFrequencyFixture, GivenValidFrequencyHandleWhenCallingzesFrequencyGetStateThenVerifyzesFrequencyThrottleReasonBurstPower) {
    auto handles = getFreqHandles(handleComponentCount);
    for (auto handle : handles) {
        zes_freq_state_t state{ZES_STRUCTURE_TYPE_FREQ_STATE};
        uint32_t validReason = 1;
        pSysfsAccess->setValU32(throttleReasonStatusFile, validReason);
        pSysfsAccess->setValU32(throttleReasonPL2File, validReason);
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesFrequencyGetState(handle, &state));
        EXPECT_EQ((ZES_FREQ_THROTTLE_REASON_FLAG_BURST_PWR_CAP), state.throttleReasons);
        EXPECT_EQ(nullptr, state.pNext);
    }
}

TEST_F(SysmanDeviceFrequencyFixture, GivenValidFrequencyHandleWhenCallingzesFrequencyGetStateThenVerifyzesFrequencyThrottleReasonsCurrentExcursion) {
    auto handles = getFreqHandles(handleComponentCount);
    for (auto handle : handles) {
        zes_freq_state_t state{ZES_STRUCTURE_TYPE_FREQ_STATE};
        uint32_t validReason = 1;
        pSysfsAccess->setValU32(throttleReasonStatusFile, validReason);
        pSysfsAccess->setValU32(throttleReasonPL4File, validReason);
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesFrequencyGetState(handle, &state));
        EXPECT_EQ((ZES_FREQ_THROTTLE_REASON_FLAG_CURRENT_LIMIT), state.throttleReasons);
        EXPECT_EQ(nullptr, state.pNext);
    }
}

TEST_F(SysmanDeviceFrequencyFixture, GivenValidFrequencyHandleWhenCallingzesFrequencyGetStateThenVerifyzesFrequencyThrottleReasonsThermalExcursion) {
    auto handles = getFreqHandles(handleComponentCount);
    for (auto handle : handles) {
        zes_freq_state_t state{ZES_STRUCTURE_TYPE_FREQ_STATE};
        uint32_t validReason = 1;
        pSysfsAccess->setValU32(throttleReasonStatusFile, validReason);
        pSysfsAccess->setValU32(throttleReasonThermalFile, validReason);
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesFrequencyGetState(handle, &state));
        EXPECT_EQ((ZES_FREQ_THROTTLE_REASON_FLAG_THERMAL_LIMIT), state.throttleReasons);
        EXPECT_EQ(nullptr, state.pNext);
    }
}

TEST_F(SysmanDeviceFrequencyFixture, GivenValidFrequencyHandleWhenCallingzesFrequencyGetStateThenVerifyzesFrequencyThrottleReasonsInvalidThermalExcursion) {
    auto handles = getFreqHandles(handleComponentCount);
    for (auto handle : handles) {
        zes_freq_state_t state{ZES_STRUCTURE_TYPE_FREQ_STATE};
        uint32_t validReason = 1;
        uint32_t invalidReason = 0;
        pSysfsAccess->setValU32(throttleReasonStatusFile, validReason);
        pSysfsAccess->setValU32(throttleReasonPL4File, validReason);
        pSysfsAccess->setValU32(throttleReasonThermalFile, invalidReason);
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesFrequencyGetState(handle, &state));
        EXPECT_EQ((ZES_FREQ_THROTTLE_REASON_FLAG_CURRENT_LIMIT), state.throttleReasons);
        EXPECT_EQ(nullptr, state.pNext);
    }
}

TEST_F(SysmanDeviceFrequencyFixture, GivenValidFrequencyHandleWhenCallingzesFrequencyGetStateThenVerifyzesFrequencyThrottleReasonsStatusforValidReasons) {
    zes_freq_state_t state{ZES_STRUCTURE_TYPE_FREQ_STATE};
    uint32_t validReason = 1;
    uint32_t setAllThrottleReasons = (ZES_FREQ_THROTTLE_REASON_FLAG_THERMAL_LIMIT |
                                      ZES_FREQ_THROTTLE_REASON_FLAG_CURRENT_LIMIT |
                                      ZES_FREQ_THROTTLE_REASON_FLAG_BURST_PWR_CAP |
                                      ZES_FREQ_THROTTLE_REASON_FLAG_AVE_PWR_CAP);

    PublicLinuxFrequencyImp linuxFrequencyImp(pOsSysman, 0, 0, ZES_FREQ_DOMAIN_GPU);
    pSysfsAccess->setValU32(throttleReasonStatusFile, validReason);
    pSysfsAccess->setValU32(throttleReasonPL1File, validReason);
    pSysfsAccess->setValU32(throttleReasonPL2File, validReason);
    pSysfsAccess->setValU32(throttleReasonPL4File, validReason);
    pSysfsAccess->setValU32(throttleReasonThermalFile, validReason);

    EXPECT_EQ(ZE_RESULT_SUCCESS, linuxFrequencyImp.osFrequencyGetState(&state));
    EXPECT_EQ(setAllThrottleReasons, state.throttleReasons);
}

TEST_F(SysmanDeviceFrequencyFixture, GivenValidFrequencyHandleWhenCallingzesFrequencyGetStateThenVerifyzesFrequencyThrottleReasonsStatusforMissingTHermalStatusFile) {
    pSysfsAccess->mockReadThermalError = true;
    zes_freq_state_t state{ZES_STRUCTURE_TYPE_FREQ_STATE};
    uint32_t validReason = 1;
    uint32_t invalidReason = 0;
    uint32_t setAllThrottleReasonsExceptThermal =
        (ZES_FREQ_THROTTLE_REASON_FLAG_CURRENT_LIMIT |
         ZES_FREQ_THROTTLE_REASON_FLAG_BURST_PWR_CAP |
         ZES_FREQ_THROTTLE_REASON_FLAG_AVE_PWR_CAP);

    PublicLinuxFrequencyImp linuxFrequencyImp(pOsSysman, 0, 0, ZES_FREQ_DOMAIN_GPU);
    pSysfsAccess->setValU32(throttleReasonStatusFile, validReason);
    pSysfsAccess->setValU32(throttleReasonPL1File, validReason);
    pSysfsAccess->setValU32(throttleReasonPL2File, validReason);
    pSysfsAccess->setValU32(throttleReasonPL4File, validReason);

    EXPECT_EQ(ZE_RESULT_SUCCESS, linuxFrequencyImp.osFrequencyGetState(&state));
    EXPECT_EQ(setAllThrottleReasonsExceptThermal, state.throttleReasons);

    pSysfsAccess->setValU32(throttleReasonThermalFile, invalidReason);
    EXPECT_EQ(ZE_RESULT_SUCCESS, linuxFrequencyImp.osFrequencyGetState(&state));
    EXPECT_EQ(setAllThrottleReasonsExceptThermal, state.throttleReasons);
}

TEST_F(SysmanDeviceFrequencyFixture, GivenValidFrequencyHandleWhenCallingzesFrequencyGetStateThenVerifyzesFrequencyThrottleReasonsStatusforMissingPL4StatusFile) {
    pSysfsAccess->mockReadPL4Error = true;
    zes_freq_state_t state{ZES_STRUCTURE_TYPE_FREQ_STATE};
    uint32_t validReason = 1;
    uint32_t invalidReason = 0;
    uint32_t setAllThrottleReasonsExceptPL4 =
        (ZES_FREQ_THROTTLE_REASON_FLAG_THERMAL_LIMIT |
         ZES_FREQ_THROTTLE_REASON_FLAG_BURST_PWR_CAP |
         ZES_FREQ_THROTTLE_REASON_FLAG_AVE_PWR_CAP);

    PublicLinuxFrequencyImp linuxFrequencyImp(pOsSysman, 0, 0, ZES_FREQ_DOMAIN_GPU);
    pSysfsAccess->setValU32(throttleReasonStatusFile, validReason);
    pSysfsAccess->setValU32(throttleReasonPL1File, validReason);
    pSysfsAccess->setValU32(throttleReasonPL2File, validReason);
    pSysfsAccess->setValU32(throttleReasonThermalFile, validReason);

    EXPECT_EQ(ZE_RESULT_SUCCESS, linuxFrequencyImp.osFrequencyGetState(&state));
    EXPECT_EQ(setAllThrottleReasonsExceptPL4, state.throttleReasons);

    pSysfsAccess->setValU32(throttleReasonPL4File, invalidReason);
    EXPECT_EQ(ZE_RESULT_SUCCESS, linuxFrequencyImp.osFrequencyGetState(&state));
    EXPECT_EQ(setAllThrottleReasonsExceptPL4, state.throttleReasons);
}

TEST_F(SysmanDeviceFrequencyFixture, GivenValidFrequencyHandleWhenCallingzesFrequencyGetStateThenVerifyzesFrequencyThrottleReasonsStatusforMissingPL1StatusFile) {
    pSysfsAccess->mockReadPL1Error = true;
    zes_freq_state_t state{ZES_STRUCTURE_TYPE_FREQ_STATE};
    uint32_t validReason = 1;
    uint32_t invalidReason = 0;
    uint32_t setAllThrottleReasonsExceptPL1 =
        (ZES_FREQ_THROTTLE_REASON_FLAG_THERMAL_LIMIT |
         ZES_FREQ_THROTTLE_REASON_FLAG_CURRENT_LIMIT |
         ZES_FREQ_THROTTLE_REASON_FLAG_BURST_PWR_CAP);

    PublicLinuxFrequencyImp linuxFrequencyImp(pOsSysman, 0, 0, ZES_FREQ_DOMAIN_GPU);
    pSysfsAccess->setValU32(throttleReasonStatusFile, validReason);
    pSysfsAccess->setValU32(throttleReasonPL2File, validReason);
    pSysfsAccess->setValU32(throttleReasonPL4File, validReason);
    pSysfsAccess->setValU32(throttleReasonThermalFile, validReason);

    EXPECT_EQ(ZE_RESULT_SUCCESS, linuxFrequencyImp.osFrequencyGetState(&state));
    EXPECT_EQ(setAllThrottleReasonsExceptPL1, state.throttleReasons);

    pSysfsAccess->setValU32(throttleReasonPL1File, invalidReason);
    EXPECT_EQ(ZE_RESULT_SUCCESS, linuxFrequencyImp.osFrequencyGetState(&state));
    EXPECT_EQ(setAllThrottleReasonsExceptPL1, state.throttleReasons);
}

TEST_F(SysmanDeviceFrequencyFixture, GivenValidFrequencyHandleWhenCallingzesFrequencyGetStateThenVerifyzesFrequencyThrottleReasonsStatusforMissingPL2StatusFile) {
    pSysfsAccess->mockReadPL2Error = true;
    zes_freq_state_t state{ZES_STRUCTURE_TYPE_FREQ_STATE};
    uint32_t validReason = 1;
    uint32_t invalidReason = 0;
    uint32_t setAllThrottleReasonsExceptPL2 =
        (ZES_FREQ_THROTTLE_REASON_FLAG_THERMAL_LIMIT |
         ZES_FREQ_THROTTLE_REASON_FLAG_CURRENT_LIMIT |
         ZES_FREQ_THROTTLE_REASON_FLAG_AVE_PWR_CAP);

    PublicLinuxFrequencyImp linuxFrequencyImp(pOsSysman, 0, 0, ZES_FREQ_DOMAIN_GPU);
    pSysfsAccess->setValU32(throttleReasonStatusFile, validReason);
    pSysfsAccess->setValU32(throttleReasonPL1File, validReason);
    pSysfsAccess->setValU32(throttleReasonPL4File, validReason);
    pSysfsAccess->setValU32(throttleReasonThermalFile, validReason);

    EXPECT_EQ(ZE_RESULT_SUCCESS, linuxFrequencyImp.osFrequencyGetState(&state));
    EXPECT_EQ(setAllThrottleReasonsExceptPL2, state.throttleReasons);

    pSysfsAccess->setValU32(throttleReasonPL2File, invalidReason);
    EXPECT_EQ(ZE_RESULT_SUCCESS, linuxFrequencyImp.osFrequencyGetState(&state));
    EXPECT_EQ(setAllThrottleReasonsExceptPL2, state.throttleReasons);
}

TEST_F(SysmanDeviceFrequencyFixture, GivenValidFrequencyHandleWhenCallingzesFrequencySetRangeWithLegacyPathThenVerifyzesFrequencySetRangeTestCallSucceeds) {
    pSysfsAccess->isLegacy = true;
    pSysfsAccess->directoryExistsResult = false;
    for (auto handle : pSysmanDeviceImp->pFrequencyHandleContext->handleList) {
        delete handle;
    }
    pSysmanDeviceImp->pFrequencyHandleContext->handleList.clear();
    pSysmanDeviceImp->pFrequencyHandleContext->init(deviceHandles);
    auto handles = getFreqHandles(handleComponentCount);
    double minFreqLegacy = 400.0;
    double maxFreqLegacy = 1200.0;
    pSysfsAccess->setValLegacy(minFreqFileLegacy, minFreqLegacy);
    pSysfsAccess->setValLegacy(maxFreqFileLegacy, maxFreqLegacy);
    for (auto handle : handles) {
        zes_freq_range_t limits;
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesFrequencyGetRange(handle, &limits));
        EXPECT_DOUBLE_EQ(minFreqLegacy, limits.min);
        EXPECT_DOUBLE_EQ(maxFreqLegacy, limits.max);
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesFrequencySetRange(handle, &limits));
        EXPECT_DOUBLE_EQ(pSysfsAccess->mockBoost, limits.max);
    }
}

TEST_F(SysmanDeviceFrequencyFixture, GivenValidStatePointerWhenValidatingfrequencyGetStateWhenOneOfTheFrequencyStateThenNegativeValueIsReturned) {
    auto pFrequencyImp = std::make_unique<FrequencyImp>(pOsSysman, device->toHandle(), ZES_FREQ_DOMAIN_GPU);
    zes_freq_state_t state = {};
    pSysfsAccess->mockReadRequestResult = ZE_RESULT_ERROR_NOT_AVAILABLE;
    EXPECT_EQ(ZE_RESULT_SUCCESS, pFrequencyImp->frequencyGetState(&state));
    EXPECT_EQ(-1, state.request);

    pSysfsAccess->mockReadRequestResult = ZE_RESULT_ERROR_UNKNOWN;
    EXPECT_EQ(ZE_RESULT_SUCCESS, pFrequencyImp->frequencyGetState(&state));
    EXPECT_EQ(-1, state.request);

    pSysfsAccess->mockReadRequestResult = ZE_RESULT_SUCCESS;
    pSysfsAccess->mockReadTdpResult = ZE_RESULT_ERROR_NOT_AVAILABLE;
    EXPECT_EQ(ZE_RESULT_SUCCESS, pFrequencyImp->frequencyGetState(&state));
    EXPECT_EQ(-1, state.tdp);

    pSysfsAccess->mockReadTdpResult = ZE_RESULT_ERROR_UNKNOWN;
    EXPECT_EQ(ZE_RESULT_SUCCESS, pFrequencyImp->frequencyGetState(&state));
    EXPECT_EQ(-1, state.tdp);

    pSysfsAccess->mockReadTdpResult = ZE_RESULT_SUCCESS;
    pSysfsAccess->mockReadEfficientResult = ZE_RESULT_ERROR_NOT_AVAILABLE;
    EXPECT_EQ(ZE_RESULT_SUCCESS, pFrequencyImp->frequencyGetState(&state));
    EXPECT_EQ(-1, state.efficient);

    pSysfsAccess->mockReadEfficientResult = ZE_RESULT_ERROR_UNKNOWN;
    EXPECT_EQ(ZE_RESULT_SUCCESS, pFrequencyImp->frequencyGetState(&state));
    EXPECT_EQ(-1, state.efficient);

    pSysfsAccess->mockReadEfficientResult = ZE_RESULT_SUCCESS;
    pSysfsAccess->mockReadActualResult = ZE_RESULT_ERROR_NOT_AVAILABLE;
    EXPECT_EQ(ZE_RESULT_SUCCESS, pFrequencyImp->frequencyGetState(&state));
    EXPECT_EQ(-1, state.actual);

    pSysfsAccess->mockReadActualResult = ZE_RESULT_ERROR_UNKNOWN;
    EXPECT_EQ(ZE_RESULT_SUCCESS, pFrequencyImp->frequencyGetState(&state));
    EXPECT_EQ(-1, state.actual);
}

TEST_F(SysmanDeviceFrequencyFixture, GivenThrottleTimeStructPointerWhenCallingfrequencyGetThrottleTimeThenUnsupportedIsReturned) {
    auto pFrequencyImp = std::make_unique<FrequencyImp>(pOsSysman, device->toHandle(), ZES_FREQ_DOMAIN_GPU);
    zes_freq_throttle_time_t throttleTime = {};
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, pFrequencyImp->frequencyGetThrottleTime(&throttleTime));
}

TEST_F(SysmanDeviceFrequencyFixture, GivengetMinFunctionReturnsErrorWhenValidatinggetMinFailuresThenAPIReturnsErrorAccordingly) {
    PublicLinuxFrequencyImp linuxFrequencyImp(pOsSysman, 0, 0, ZES_FREQ_DOMAIN_GPU);
    double min = 0;
    pSysfsAccess->mockReadDoubleValResult = ZE_RESULT_ERROR_NOT_AVAILABLE;
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, linuxFrequencyImp.getMin(min));

    pSysfsAccess->mockReadDoubleValResult = ZE_RESULT_ERROR_UNKNOWN;
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, linuxFrequencyImp.getMin(min));
}

TEST_F(SysmanDeviceFrequencyFixture, GivengetMinValFunctionReturnsErrorWhenValidatinggetMinValFailuresThenAPIReturnsErrorAccordingly) {
    PublicLinuxFrequencyImp linuxFrequencyImp(pOsSysman, 0, 0, ZES_FREQ_DOMAIN_GPU);
    double val = 0;
    pSysfsAccess->mockReadMinValResult = ZE_RESULT_ERROR_NOT_AVAILABLE;
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, linuxFrequencyImp.getMinVal(val));

    pSysfsAccess->mockReadMinValResult = ZE_RESULT_ERROR_UNKNOWN;
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, linuxFrequencyImp.getMinVal(val));
}

TEST_F(SysmanDeviceFrequencyFixture, GivengetMaxValFunctionReturnsErrorWhenValidatinggetMaxValFailuresThenAPIReturnsErrorAccordingly) {
    PublicLinuxFrequencyImp linuxFrequencyImp(pOsSysman, 0, 0, ZES_FREQ_DOMAIN_GPU);
    double val = 0;
    pSysfsAccess->mockReadMaxValResult = ZE_RESULT_ERROR_NOT_AVAILABLE;
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, linuxFrequencyImp.getMaxVal(val));

    pSysfsAccess->mockReadMaxValResult = ZE_RESULT_ERROR_UNKNOWN;
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, linuxFrequencyImp.getMaxVal(val));
}

TEST_F(SysmanDeviceFrequencyFixture, GivengetMaxValFunctionReturnsErrorWhenValidatingosFrequencyGetPropertiesThenAPIBehavesAsExpected) {
    zes_freq_properties_t properties = {};
    PublicLinuxFrequencyImp linuxFrequencyImp(pOsSysman, 0, 0, ZES_FREQ_DOMAIN_GPU);
    pSysfsAccess->mockReadMaxValResult = ZE_RESULT_ERROR_NOT_AVAILABLE;
    EXPECT_EQ(ZE_RESULT_SUCCESS, linuxFrequencyImp.osFrequencyGetProperties(properties));
    EXPECT_EQ(0, properties.canControl);
}

TEST_F(SysmanDeviceFrequencyFixture, GivengetMinValFunctionReturnsErrorWhenValidatingosFrequencyGetPropertiesThenAPIBehavesAsExpected) {
    zes_freq_properties_t properties = {};
    PublicLinuxFrequencyImp linuxFrequencyImp(pOsSysman, 0, 0, ZES_FREQ_DOMAIN_GPU);
    pSysfsAccess->mockReadMinValResult = ZE_RESULT_ERROR_NOT_AVAILABLE;
    EXPECT_EQ(ZE_RESULT_SUCCESS, linuxFrequencyImp.osFrequencyGetProperties(properties));
    EXPECT_EQ(0, properties.canControl);
}

TEST_F(SysmanDeviceFrequencyFixture, GivenOnSubdeviceSetWhenValidatingAnyFrequencyAPIThenSuccessIsReturned) {
    zes_freq_properties_t properties = {};
    PublicLinuxFrequencyImp linuxFrequencyImp(pOsSysman, 1, 0, ZES_FREQ_DOMAIN_GPU);
    EXPECT_EQ(ZE_RESULT_SUCCESS, linuxFrequencyImp.osFrequencyGetProperties(properties));
    EXPECT_EQ(1, properties.canControl);
}

TEST_F(SysmanDeviceFrequencyFixture, GivenValidFrequencyHandleWhenCallingzesFrequencySetRangeAndIfgetMaxFailsThenVerifyzesFrequencySetRangeTestCallFail) {
    auto handles = getFreqHandles(handleComponentCount);
    for (auto handle : handles) {
        const double startingMax = 600.0;
        const double newMin = 900.0;
        zes_freq_range_t limits;

        pSysfsAccess->setVal(maxFreqFile, startingMax);
        // If the new Min value is greater than the old Max
        // value, the new Max must be set before the new Min
        limits.min = newMin;
        limits.max = maxFreq;
        pSysfsAccess->mockReadMaxResult = ZE_RESULT_ERROR_NOT_AVAILABLE;
        EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesFrequencySetRange(handle, &limits));
    }
}

TEST_F(SysmanDeviceFrequencyFixture, GivenValidFrequencyHandleWhenCallingzesFrequencySetRangeAndIfsetMaxFailsThenVerifyzesFrequencySetRangeTestCallFail) {
    auto handles = getFreqHandles(handleComponentCount);
    for (auto handle : handles) {
        const double startingMax = 600.0;
        const double newMin = 900.0;
        zes_freq_range_t limits;

        pSysfsAccess->setVal(maxFreqFile, startingMax);
        // If the new Min value is greater than the old Max
        // value, the new Max must be set before the new Min
        limits.min = newMin;
        limits.max = maxFreq;
        pSysfsAccess->mockWriteMaxResult = ZE_RESULT_ERROR_NOT_AVAILABLE;
        EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesFrequencySetRange(handle, &limits));
    }
}

TEST_F(SysmanDeviceFrequencyFixture, GivenValidFrequencyHandleWhenCallingzesFrequencyOcGetFrequencyTargetThenVerifyTestCallFail) {
    auto handles = getFreqHandles(handleComponentCount);
    for (auto handle : handles) {
        double freqTarget = 0.0;
        EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesFrequencyOcGetFrequencyTarget(handle, &freqTarget));
    }
}

TEST_F(SysmanDeviceFrequencyFixture, GivenValidFrequencyHandleWhenCallingzesFrequencyOcSetFrequencyTargetThenVerifyTestCallFail) {
    auto handles = getFreqHandles(handleComponentCount);
    for (auto handle : handles) {
        double freqTarget = 0.0;
        EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesFrequencyOcSetFrequencyTarget(handle, freqTarget));
    }
}

TEST_F(SysmanDeviceFrequencyFixture, GivenValidFrequencyHandleWhenCallingzesFrequencyOcGetVoltageTargetThenVerifyTestCallFail) {
    auto handles = getFreqHandles(handleComponentCount);
    for (auto handle : handles) {
        double voltTarget = 0.0, voltOffset = 0.0;
        EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesFrequencyOcGetVoltageTarget(handle, &voltTarget, &voltOffset));
    }
}

TEST_F(SysmanDeviceFrequencyFixture, GivenValidFrequencyHandleWhenCallingzesFrequencyOcSetVoltageTargetThenVerifyTestCallFail) {
    auto handles = getFreqHandles(handleComponentCount);
    for (auto handle : handles) {
        double voltTarget = 0.0, voltOffset = 0.0;
        EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesFrequencyOcSetVoltageTarget(handle, voltTarget, voltOffset));
    }
}

TEST_F(SysmanDeviceFrequencyFixture, GivenValidFrequencyHandleWhenCallingzesFrequencyOcSetModeThenVerifyTestCallFail) {
    auto handles = getFreqHandles(handleComponentCount);
    for (auto handle : handles) {
        zes_oc_mode_t mode = ZES_OC_MODE_OFF;
        EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesFrequencyOcSetMode(handle, mode));
    }
}

TEST_F(SysmanDeviceFrequencyFixture, GivenValidFrequencyHandleWhenCallingzesFrequencyOcGetModeThenVerifyTestCallFail) {
    auto handles = getFreqHandles(handleComponentCount);
    for (auto handle : handles) {
        zes_oc_mode_t mode = ZES_OC_MODE_OFF;
        EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesFrequencyOcGetMode(handle, &mode));
    }
}

TEST_F(SysmanDeviceFrequencyFixture, GivenValidFrequencyHandleWhenCallingzesFrequencyOcGetCapabilitiesThenVerifyTestCallFail) {
    auto handles = getFreqHandles(handleComponentCount);
    for (auto handle : handles) {
        zes_oc_capabilities_t caps = {};
        EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesFrequencyOcGetCapabilities(handle, &caps));
    }
}

TEST_F(SysmanDeviceFrequencyFixture, GivenValidFrequencyHandleWhenCallingzesFrequencyOcGetIccMaxThenVerifyTestCallFail) {
    auto handles = getFreqHandles(handleComponentCount);
    for (auto handle : handles) {
        double iccMax = 0.0;
        EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesFrequencyOcGetIccMax(handle, &iccMax));
    }
}

TEST_F(SysmanDeviceFrequencyFixture, GivenValidFrequencyHandleWhenCallingzesFrequencyOcSetIccMaxThenVerifyTestCallFail) {
    auto handles = getFreqHandles(handleComponentCount);
    for (auto handle : handles) {
        double iccMax = 0.0;
        EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesFrequencyOcSetIccMax(handle, iccMax));
    }
}

TEST_F(SysmanDeviceFrequencyFixture, GivenValidFrequencyHandleWhenCallingzesFrequencyOcGetTjMaxThenVerifyTestCallFail) {
    auto handles = getFreqHandles(handleComponentCount);
    for (auto handle : handles) {
        double tjMax = 0.0;
        EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesFrequencyOcGetTjMax(handle, &tjMax));
    }
}

TEST_F(SysmanDeviceFrequencyFixture, GivenValidFrequencyHandleWhenCallingzesFrequencyOcSetTjMaxThenVerifyTestCallFail) {
    auto handles = getFreqHandles(handleComponentCount);
    for (auto handle : handles) {
        double tjMax = 0.0;
        EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesFrequencyOcSetTjMax(handle, tjMax));
    }
}

TEST_F(SysmanMultiDeviceFixture, GivenValidDevicePointerWhenGettingFrequencyPropertiesThenValidFreqPropertiesRetrieved) {
    zes_freq_properties_t properties = {};
    ze_device_properties_t deviceProperties = {ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES};
    Device::fromHandle(device)->getProperties(&deviceProperties);
    LinuxFrequencyImp *pLinuxFrequencyImp = new LinuxFrequencyImp(pOsSysman, deviceProperties.flags & ZE_DEVICE_PROPERTY_FLAG_SUBDEVICE,
                                                                  deviceProperties.subdeviceId, ZES_FREQ_DOMAIN_GPU);
    EXPECT_EQ(ZE_RESULT_SUCCESS, pLinuxFrequencyImp->osFrequencyGetProperties(properties));
    EXPECT_EQ(properties.subdeviceId, deviceProperties.subdeviceId);
    EXPECT_EQ(properties.onSubdevice, deviceProperties.flags & ZE_DEVICE_PROPERTY_FLAG_SUBDEVICE);
    delete pLinuxFrequencyImp;
}

class FreqMultiDeviceFixture : public SysmanMultiDeviceFixture {
  protected:
    DebugManagerStateRestore restorer;
    std::unique_ptr<MockFrequencySysfsAccess> pSysfsAccess;
    SysfsAccess *pSysfsAccessOld = nullptr;
    std::vector<ze_device_handle_t> deviceHandles;

    void SetUp() override {
        if (!sysmanUltsEnable) {
            GTEST_SKIP();
        }
        NEO::debugManager.flags.ZE_AFFINITY_MASK.set("0.1");
        SysmanMultiDeviceFixture::SetUp();
        pSysfsAccessOld = pLinuxSysmanImp->pSysfsAccess;
        pSysfsAccess = std::make_unique<MockFrequencySysfsAccess>();
        pLinuxSysmanImp->pSysfsAccess = pSysfsAccess.get();
        // delete handles created in initial SysmanDeviceHandleContext::init() call
        for (auto handle : pSysmanDeviceImp->pFrequencyHandleContext->handleList) {
            delete handle;
        }
        pSysmanDeviceImp->pFrequencyHandleContext->handleList.clear();
        uint32_t subDeviceCount = 0;
        // We received a device handle. Check for subdevices in this device
        Device::fromHandle(device->toHandle())->getSubDevices(&subDeviceCount, nullptr);
        if (subDeviceCount == 0) {
            deviceHandles.resize(1, device->toHandle());
        } else {
            deviceHandles.resize(subDeviceCount, nullptr);
            Device::fromHandle(device->toHandle())->getSubDevices(&subDeviceCount, deviceHandles.data());
        }
        getFreqHandles(0);
    }

    void TearDown() override {
        if (!sysmanUltsEnable) {
            GTEST_SKIP();
        }
        pLinuxSysmanImp->pSysfsAccess = pSysfsAccessOld;
        SysmanMultiDeviceFixture::TearDown();
    }

    std::vector<zes_freq_handle_t> getFreqHandles(uint32_t count) {
        std::vector<zes_freq_handle_t> handles(count, nullptr);
        EXPECT_EQ(zesDeviceEnumFrequencyDomains(device->toHandle(), &count, handles.data()), ZE_RESULT_SUCCESS);
        return handles;
    }
};

TEST_F(FreqMultiDeviceFixture, GivenAffinityMaskIsSetWhenCallingFrequencyPropertiesThenPropertiesAreReturnedForTheSubDevicesAccordingToAffinityMask) {
    uint32_t count = 0U;

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEnumFrequencyDomains(device->toHandle(), &count, nullptr));
    EXPECT_EQ(count, handleComponentCount);
    auto handles = getFreqHandles(handleComponentCount);
    for (auto handle : handles) {
        EXPECT_NE(handle, nullptr);
        zes_freq_properties_t properties;
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesFrequencyGetProperties(handle, &properties));
        EXPECT_EQ(nullptr, properties.pNext);
        EXPECT_EQ(ZES_FREQ_DOMAIN_GPU, properties.type);
        EXPECT_TRUE(properties.onSubdevice);
        EXPECT_EQ(1u, properties.subdeviceId); // Affinity mask 0.1 is set which means only subdevice 1 is exposed
    }
}

} // namespace ult
} // namespace L0
