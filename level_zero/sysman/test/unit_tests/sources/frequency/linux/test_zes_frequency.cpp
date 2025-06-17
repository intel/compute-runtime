/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/sysman/source/shared/linux/kmd_interface/sysman_kmd_interface.h"
#include "level_zero/sysman/test/unit_tests/sources/frequency/linux/mock_sysman_frequency.h"
#include "level_zero/sysman/test/unit_tests/sources/linux/mock_sysman_fixture.h"
#include "level_zero/sysman/test/unit_tests/sources/linux/mocks/mock_sysman_product_helper.h"

#include <cmath>

namespace L0 {
namespace Sysman {
namespace ult {

constexpr double minFreq = 300.0;
constexpr double maxFreq = 1100.0;
constexpr double request = 300.0;
constexpr double tdp = 1100.0;
constexpr double actual = 300.0;
constexpr double efficient = 300.0;
constexpr double maxVal = 1100.0;
constexpr double minVal = 300.0;
constexpr uint32_t handleComponentCount = 1u;
constexpr uint32_t multiHandleComponentCount = 2u;
class SysmanDeviceFrequencyFixture : public SysmanDeviceFixture {

  protected:
    L0::Sysman::SysmanDevice *device = nullptr;
    std::unique_ptr<MockFrequencySysfsAccess> pSysfsAccess;
    L0::Sysman::SysFsAccessInterface *pSysfsAccessOld = nullptr;
    uint32_t numClocks = 0;
    double step = 0;

    void SetUp() override {
        SysmanDeviceFixture::SetUp();
        device = pSysmanDevice;
        pSysfsAccessOld = pLinuxSysmanImp->pSysfsAccess;
        pSysfsAccess = std::make_unique<MockFrequencySysfsAccess>();
        pLinuxSysmanImp->pSysfsAccess = pSysfsAccess.get();
        auto &rootDeviceEnvironment = pLinuxSysmanImp->getParentSysmanDeviceImp()->getRootDeviceEnvironmentRef();
        rootDeviceEnvironment.getMutableHardwareInfo()->capabilityTable.supportsImages = false;
        pSysfsAccess->setVal(minFreqFile, minFreq);
        pSysfsAccess->setVal(maxFreqFile, maxFreq);
        pSysfsAccess->setVal(requestFreqFile, request);
        pSysfsAccess->setVal(tdpFreqFile, tdp);
        pSysfsAccess->setVal(actualFreqFile, actual);
        pSysfsAccess->setVal(efficientFreqFile, efficient);
        pSysfsAccess->setVal(maxValFreqFile, maxVal);
        pSysfsAccess->setVal(minValFreqFile, minVal);
        step = 50;
        numClocks = static_cast<uint32_t>((maxFreq - minFreq) / step) + 1;

        // delete handles created in initial SysmanDeviceHandleContext::init() call
        for (auto handle : pSysmanDeviceImp->pFrequencyHandleContext->handleList) {
            delete handle;
        }
        pSysmanDeviceImp->pFrequencyHandleContext->handleList.clear();
    }

    void TearDown() override {
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

TEST_F(SysmanDeviceFrequencyFixture, GivenComponentCountZeroWhenEnumeratingFrequencyHandlesAndMediaFreqDomainIsPresentThenNonZeroCountIsReturnedAndCallSucceds) {
    pLinuxSysmanImp->getParentSysmanDeviceImp()->getRootDeviceEnvironmentRef().getMutableHardwareInfo()->capabilityTable.supportsImages = true;
    uint32_t count = 0U;

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEnumFrequencyDomains(device->toHandle(), &count, nullptr));
    EXPECT_EQ(count, 2U);

    uint32_t testCount = count + 1;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEnumFrequencyDomains(device->toHandle(), &testCount, nullptr));
    EXPECT_EQ(count, testCount);

    auto handles = getFreqHandles(count);
    for (auto handle : handles) {
        EXPECT_NE(handle, nullptr);
    }
}

TEST_F(SysmanDeviceFrequencyFixture, GivenComponentCountZeroWhenEnumeratingFrequencyHandlesAndMediaDomainIsAbsentThenNonZeroCountIsReturnedAndCallSucceds) {
    pSysfsAccess->directoryExistsResult = false;
    pLinuxSysmanImp->getParentSysmanDeviceImp()->getRootDeviceEnvironmentRef().getMutableHardwareInfo()->capabilityTable.supportsImages = true;
    uint32_t count = 0U;

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEnumFrequencyDomains(device->toHandle(), &count, nullptr));
    EXPECT_EQ(count, 1U);

    uint32_t testCount = count + 1;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEnumFrequencyDomains(device->toHandle(), &testCount, nullptr));
    EXPECT_EQ(count, testCount);

    auto handles = getFreqHandles(count);
    for (auto handle : handles) {
        EXPECT_NE(handle, nullptr);
    }
}

TEST_F(SysmanDeviceFrequencyFixture, GivenActualComponentCountTwoWhenTryingToGetOneComponentOnlyThenOneComponentIsReturnedAndCountUpdated) {
    auto subDeviceCount = pLinuxSysmanImp->getSubDeviceCount();
    ze_bool_t onSubdevice = (subDeviceCount == 0) ? false : true;
    uint32_t subdeviceId = 0;
    auto pFrequencyHandleContextTest = std::make_unique<L0::Sysman::FrequencyHandleContext>(pOsSysman);
    pFrequencyHandleContextTest->handleList.push_back(new L0::Sysman::FrequencyImp(pOsSysman, onSubdevice, subdeviceId, ZES_FREQ_DOMAIN_GPU));
    pFrequencyHandleContextTest->handleList.push_back(new L0::Sysman::FrequencyImp(pOsSysman, onSubdevice, subdeviceId, ZES_FREQ_DOMAIN_GPU));

    uint32_t count = 1;
    std::vector<zes_freq_handle_t> phFrequency(count, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, pFrequencyHandleContextTest->frequencyGet(&count, phFrequency.data()));
    EXPECT_EQ(count, 1u);
}

TEST_F(SysmanDeviceFrequencyFixture, GivenValidFrequencyHandleWhenCallingzesFrequencyGetPropertiesThenSuccessIsReturned) {
    MockSysmanProductHelper *pMockSysmanProductHelper = new MockSysmanProductHelper();
    pMockSysmanProductHelper->isFrequencySetRangeSupportedResult = true;
    std::unique_ptr<SysmanProductHelper> pSysmanProductHelper(static_cast<SysmanProductHelper *>(pMockSysmanProductHelper));
    std::swap(pLinuxSysmanImp->pSysmanProductHelper, pSysmanProductHelper);

    auto handles = getFreqHandles(handleComponentCount);
    for (auto handle : handles) {
        EXPECT_NE(handle, nullptr);
        zes_freq_properties_t properties;
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesFrequencyGetProperties(handle, &properties));
        EXPECT_EQ(nullptr, properties.pNext);
        EXPECT_GE(properties.type, ZES_FREQ_DOMAIN_GPU);
        EXPECT_LE(properties.type, ZES_FREQ_DOMAIN_MEDIA);
        EXPECT_FALSE(properties.onSubdevice);
        EXPECT_DOUBLE_EQ(maxFreq, properties.max);
        EXPECT_DOUBLE_EQ(minFreq, properties.min);
        EXPECT_TRUE(properties.canControl);
    }
}

TEST_F(SysmanDeviceFrequencyFixture, GivenValidFrequencyHandleAndFrequenceSetRangeIsUnsupportedWhenCallingzesFrequencyGetPropertiesThenVerifyCanControlIsSetToFalse) {
    std::unique_ptr<SysmanProductHelper> pSysmanProductHelper = std::make_unique<MockSysmanProductHelper>();
    std::swap(pLinuxSysmanImp->pSysmanProductHelper, pSysmanProductHelper);

    auto handles = getFreqHandles(handleComponentCount);
    for (auto handle : handles) {
        ASSERT_NE(nullptr, handle);
        zes_freq_properties_t properties{};
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesFrequencyGetProperties(handle, &properties));
        EXPECT_FALSE(properties.canControl);
    }
}

HWTEST2_F(SysmanDeviceFrequencyFixture, GivenValidFrequencyHandleAndZeroCountWhenCallingzesFrequencyGetAvailableClocksThenCallSucceeds, IsPVC) {
    auto handles = getFreqHandles(handleComponentCount);
    for (auto handle : handles) {
        EXPECT_NE(handle, nullptr);
        uint32_t count = 0;
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesFrequencyGetAvailableClocks(handle, &count, nullptr));
        EXPECT_EQ(numClocks, count);
    }
}

HWTEST2_F(SysmanDeviceFrequencyFixture, GivenValidFrequencyHandleAndZeroCountWhenCountIsMoreThanNumClocksThenCallSucceeds, IsPVC) {
    auto handles = getFreqHandles(handleComponentCount);
    for (auto handle : handles) {
        EXPECT_NE(handle, nullptr);
        uint32_t count = 80;
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesFrequencyGetAvailableClocks(handle, &count, nullptr));
        EXPECT_EQ(numClocks, count);
    }
}

HWTEST2_F(SysmanDeviceFrequencyFixture, GivenValidFrequencyHandleAndZeroCountWhenCountIsLessThanNumClocksThenCallSucceeds, IsPVC) {
    auto handles = getFreqHandles(handleComponentCount);
    for (auto handle : handles) {
        EXPECT_NE(handle, nullptr);
        uint32_t count = 20;
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesFrequencyGetAvailableClocks(handle, &count, nullptr));
    }
}

HWTEST2_F(SysmanDeviceFrequencyFixture, GivenValidFrequencyHandleAndCorrectCountWhenCallingzesFrequencyGetAvailableClocksThenCallSucceeds, IsPVC) {
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
    auto subDeviceCount = pLinuxSysmanImp->getSubDeviceCount();
    ze_bool_t onSubdevice = (subDeviceCount == 0) ? false : true;
    uint32_t subdeviceId = 0;
    auto pFrequencyImp = std::make_unique<L0::Sysman::FrequencyImp>(pOsSysman, onSubdevice, subdeviceId, ZES_FREQ_DOMAIN_GPU);
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

HWTEST2_F(SysmanDeviceFrequencyFixture, GivenValidFrequencyLimitsWhenCallingFrequencySetRangeForFailures1ThenAPIExitsGracefully, IsXeCore) {
    auto subDeviceCount = pLinuxSysmanImp->getSubDeviceCount();
    ze_bool_t onSubdevice = (subDeviceCount == 0) ? false : true;
    uint32_t subdeviceId = 0;
    auto pFrequencyImp = std::make_unique<L0::Sysman::FrequencyImp>(pOsSysman, onSubdevice, subdeviceId, ZES_FREQ_DOMAIN_GPU);
    zes_freq_range_t limits = {};

    // Verify that Max must be within range.
    limits.min = minFreq;
    limits.max = 600.0;
    pSysfsAccess->mockWriteMinResult = ZE_RESULT_ERROR_NOT_AVAILABLE;
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, pFrequencyImp->frequencySetRange(&limits));

    pSysfsAccess->mockWriteMinResult = ZE_RESULT_ERROR_UNKNOWN;
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, pFrequencyImp->frequencySetRange(&limits));
}

HWTEST2_F(SysmanDeviceFrequencyFixture, GivenValidFrequencyLimitsWhenCallingFrequencySetRangeForFailures2ThenAPIExitsGracefully, IsXeCore) {
    auto subDeviceCount = pLinuxSysmanImp->getSubDeviceCount();
    ze_bool_t onSubdevice = (subDeviceCount == 0) ? false : true;
    uint32_t subdeviceId = 0;
    auto pFrequencyImp = std::make_unique<L0::Sysman::FrequencyImp>(pOsSysman, onSubdevice, subdeviceId, ZES_FREQ_DOMAIN_GPU);
    zes_freq_range_t limits = {};

    // Verify that Max must be within range.
    limits.min = 900.0;
    limits.max = maxFreq;
    pSysfsAccess->mockWriteMaxResult = ZE_RESULT_ERROR_NOT_AVAILABLE;
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, pFrequencyImp->frequencySetRange(&limits));

    pSysfsAccess->mockWriteMaxResult = ZE_RESULT_ERROR_UNKNOWN;
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, pFrequencyImp->frequencySetRange(&limits));
}

HWTEST2_F(SysmanDeviceFrequencyFixture, GivenValidFrequencyHandleWhenCallingzesFrequencySetRangeThenVerifyzesFrequencySetRangeTest1CallSucceeds, IsXeCore) {
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

HWTEST2_F(SysmanDeviceFrequencyFixture, GivenNegativeUnityRangeSetWhenGetRangeIsCalledThenReturnsValueFromDefaultPath, IsXeCore) {
    auto handles = getFreqHandles(handleComponentCount);
    for (auto handle : handles) {
        const double negativeMin = -1;
        const double negativeMax = -1;
        zes_freq_range_t limits;

        limits.min = negativeMin;
        limits.max = negativeMax;
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesFrequencySetRange(handle, &limits));
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesFrequencyGetRange(handle, &limits));
        EXPECT_EQ(1, limits.min);
        EXPECT_EQ(1000, limits.max);
        EXPECT_DOUBLE_EQ(pSysfsAccess->mockBoost, limits.max);
    }
}

HWTEST2_F(SysmanDeviceFrequencyFixture, GivenValidFrequencyHandleWhenCallingzesFrequencySetRangeThenVerifyzesFrequencySetRangeTest2CallSucceeds, IsXeCore) {
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

HWTEST2_F(SysmanDeviceFrequencyFixture, GivenInvalidFrequencyLimitsWhenCallingFrequencySetRangeThenVerifyFrequencySetRangeTestReturnsError, IsXeCore) {
    auto subDeviceCount = pLinuxSysmanImp->getSubDeviceCount();
    ze_bool_t onSubdevice = (subDeviceCount == 0) ? false : true;
    uint32_t subdeviceId = 0;
    auto pFrequencyImp = std::make_unique<L0::Sysman::FrequencyImp>(pOsSysman, onSubdevice, subdeviceId, ZES_FREQ_DOMAIN_GPU);
    zes_freq_range_t limits;

    // Verify that Max must be greater than min range.
    limits.min = clockValue(maxFreq + step);
    limits.max = minFreq;
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, pFrequencyImp->frequencySetRange(&limits));
}

TEST_F(SysmanDeviceFrequencyFixture, GivenFrequencySetRangeNotSupportedWhenCallingzesFrequencySetRangeThenVerifyzesFrequencySetRangeFails) {
    std::unique_ptr<SysmanProductHelper> pSysmanProductHelper = std::make_unique<MockSysmanProductHelper>();
    std::swap(pLinuxSysmanImp->pSysmanProductHelper, pSysmanProductHelper);

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
        EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesFrequencySetRange(handle, &limits));
    }
}

TEST_F(SysmanDeviceFrequencyFixture, GivenValidFrequencyHandleWhenCallingzesFrequencyGetStateThenVerifyzesFrequencyGetStateTestCallSucceeds) {
    auto handles = getFreqHandles(handleComponentCount);
    for (auto handle : handles) {
        const double testRequestValue = 450.0;
        const double testTdpValue = 1200.0;
        const double testEfficientValue = 400.0;
        const double testActualValue = 550.0;
        const uint32_t invalidReason = 0;
        zes_freq_state_t state;
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

TEST_F(SysmanDeviceFrequencyFixture, GivenValidFrequencyHandlesWhenValidatingFrequencyGetStateWithSysfsReadFailsThenNegativeValueIsReturned) {

    auto handles = getFreqHandles(handleComponentCount);
    zes_freq_state_t state;
    for (auto handle : handles) {

        pSysfsAccess->mockReadRequestResult = ZE_RESULT_ERROR_NOT_AVAILABLE;
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesFrequencyGetState(handle, &state));
        EXPECT_EQ(-1, state.request);

        pSysfsAccess->mockReadRequestResult = ZE_RESULT_ERROR_UNKNOWN;
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesFrequencyGetState(handle, &state));
        EXPECT_EQ(-1, state.request);

        pSysfsAccess->mockReadTdpResult = ZE_RESULT_ERROR_NOT_AVAILABLE;
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesFrequencyGetState(handle, &state));
        EXPECT_EQ(-1, state.tdp);

        pSysfsAccess->mockReadTdpResult = ZE_RESULT_ERROR_UNKNOWN;
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesFrequencyGetState(handle, &state));
        EXPECT_EQ(-1, state.tdp);

        pSysfsAccess->mockReadEfficientResult = ZE_RESULT_ERROR_NOT_AVAILABLE;
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesFrequencyGetState(handle, &state));
        EXPECT_EQ(-1, state.efficient);

        pSysfsAccess->mockReadEfficientResult = ZE_RESULT_ERROR_UNKNOWN;
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesFrequencyGetState(handle, &state));
        EXPECT_EQ(-1, state.efficient);

        pSysfsAccess->mockReadActualResult = ZE_RESULT_ERROR_NOT_AVAILABLE;
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesFrequencyGetState(handle, &state));
        EXPECT_EQ(-1, state.actual);

        pSysfsAccess->mockReadActualResult = ZE_RESULT_ERROR_UNKNOWN;
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesFrequencyGetState(handle, &state));
        EXPECT_EQ(-1, state.actual);
    }
}

HWTEST2_F(SysmanDeviceFrequencyFixture, GivenValidFrequencyHandleWhenCallingzesFrequencySetRangeWithLegacyPathThenVerifyzesFrequencySetRangeTestCallSucceeds, IsXeCore) {
    pSysfsAccess->isLegacy = true;
    pSysfsAccess->directoryExistsResult = false;
    for (auto handle : pSysmanDeviceImp->pFrequencyHandleContext->handleList) {
        delete handle;
    }
    pSysmanDeviceImp->pFrequencyHandleContext->handleList.clear();
    pSysmanDeviceImp->pFrequencyHandleContext->init(pLinuxSysmanImp->getSubDeviceCount());
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

TEST_F(SysmanDeviceFrequencyFixture, GivenThrottleTimeStructPointerWhenCallingfrequencyGetThrottleTimeThenUnsupportedIsReturned) {
    auto subDeviceCount = pLinuxSysmanImp->getSubDeviceCount();
    ze_bool_t onSubdevice = (subDeviceCount == 0) ? false : true;
    uint32_t subdeviceId = 0;
    auto pFrequencyImp = std::make_unique<L0::Sysman::FrequencyImp>(pOsSysman, onSubdevice, subdeviceId, ZES_FREQ_DOMAIN_GPU);
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
    MockSysmanProductHelper *pMockSysmanProductHelper = new MockSysmanProductHelper();
    pMockSysmanProductHelper->isFrequencySetRangeSupportedResult = true;
    std::unique_ptr<SysmanProductHelper> pSysmanProductHelper(static_cast<SysmanProductHelper *>(pMockSysmanProductHelper));
    std::swap(pLinuxSysmanImp->pSysmanProductHelper, pSysmanProductHelper);

    zes_freq_properties_t properties = {};
    PublicLinuxFrequencyImp linuxFrequencyImp(pOsSysman, 1, 0, ZES_FREQ_DOMAIN_GPU);
    EXPECT_EQ(ZE_RESULT_SUCCESS, linuxFrequencyImp.osFrequencyGetProperties(properties));
    EXPECT_EQ(1, properties.canControl);
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
        pSysfsAccess->mockReadMaxResult = ZE_RESULT_ERROR_NOT_AVAILABLE;
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
    auto subDeviceCount = pLinuxSysmanImp->getSubDeviceCount();
    ze_bool_t onSubdevice = (subDeviceCount == 0) ? false : true;
    uint32_t subdeviceId = 0;

    zes_freq_properties_t properties = {};
    L0::Sysman::LinuxFrequencyImp *pLinuxFrequencyImp = new L0::Sysman::LinuxFrequencyImp(pOsSysman, onSubdevice, subdeviceId, ZES_FREQ_DOMAIN_GPU);
    EXPECT_EQ(ZE_RESULT_SUCCESS, pLinuxFrequencyImp->osFrequencyGetProperties(properties));
    EXPECT_EQ(properties.subdeviceId, subdeviceId);
    EXPECT_EQ(properties.onSubdevice, onSubdevice);
    delete pLinuxFrequencyImp;
}

class FreqMultiDeviceFixture : public SysmanMultiDeviceFixture {
  protected:
    L0::Sysman::SysmanDevice *device = nullptr;
    std::unique_ptr<MockFrequencySysfsAccess> pSysfsAccess;
    L0::Sysman::SysFsAccessInterface *pSysfsAccessOld = nullptr;

    void SetUp() override {
        SysmanMultiDeviceFixture::SetUp();
        device = pSysmanDevice;
        pSysfsAccessOld = pLinuxSysmanImp->pSysfsAccess;
        pSysfsAccess = std::make_unique<MockFrequencySysfsAccess>();
        pLinuxSysmanImp->pSysfsAccess = pSysfsAccess.get();
        auto &rootDeviceEnvironment = pLinuxSysmanImp->getParentSysmanDeviceImp()->getRootDeviceEnvironmentRef();
        rootDeviceEnvironment.getMutableHardwareInfo()->capabilityTable.supportsImages = false;
        // delete handles created in initial SysmanDeviceHandleContext::init() call
        for (auto handle : pSysmanDeviceImp->pFrequencyHandleContext->handleList) {
            delete handle;
        }
        pSysmanDeviceImp->pFrequencyHandleContext->handleList.clear();
        getFreqHandles(0);
    }

    void TearDown() override {
        pLinuxSysmanImp->pSysfsAccess = pSysfsAccessOld;
        SysmanMultiDeviceFixture::TearDown();
    }

    std::vector<zes_freq_handle_t> getFreqHandles(uint32_t count) {
        std::vector<zes_freq_handle_t> handles(count, nullptr);
        EXPECT_EQ(zesDeviceEnumFrequencyDomains(device->toHandle(), &count, handles.data()), ZE_RESULT_SUCCESS);
        return handles;
    }
};

TEST_F(FreqMultiDeviceFixture, GivenValidFrequencyHandleWhenCallingzesFrequencyGetPropertiesThenSuccessIsReturned) {
    uint32_t count = 0U;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEnumFrequencyDomains(device->toHandle(), &count, nullptr));
    EXPECT_EQ(count, multiHandleComponentCount);
    auto handles = getFreqHandles(multiHandleComponentCount);
    for (auto handle : handles) {
        EXPECT_NE(handle, nullptr);
        zes_freq_properties_t properties;
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesFrequencyGetProperties(handle, &properties));
        EXPECT_EQ(nullptr, properties.pNext);
        EXPECT_EQ(ZES_FREQ_DOMAIN_GPU, properties.type);
        EXPECT_TRUE(properties.onSubdevice);
    }
}

} // namespace ult
} // namespace Sysman
} // namespace L0
