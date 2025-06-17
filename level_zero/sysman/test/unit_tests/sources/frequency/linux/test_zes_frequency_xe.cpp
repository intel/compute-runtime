/*
 * Copyright (C) 2023-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/sysman/test/unit_tests/sources/frequency/linux/mock_sysfs_frequency_xe.h"
#include "level_zero/sysman/test/unit_tests/sources/linux/mock_sysman_fixture.h"
#include "level_zero/sysman/test/unit_tests/sources/linux/mocks/mock_sysman_product_helper.h"
#include "level_zero/sysman/test/unit_tests/sources/shared/linux/kmd_interface/mock_sysman_kmd_interface_xe.h"

#include <cmath>

namespace L0 {
namespace Sysman {
namespace ult {

constexpr double minFreq = 300.0;
constexpr double maxFreq = 1100.0;
constexpr double request = 300.0;
constexpr double actual = 300.0;
constexpr double efficient = 300.0;
constexpr double maxVal = 1100.0;
constexpr double minVal = 300.0;

class SysmanDeviceFrequencyFixtureXe : public SysmanDeviceFixture {
  protected:
    L0::Sysman::SysmanDevice *device = nullptr;
    MockSysmanKmdInterfaceXe *pSysmanKmdInterface = nullptr;
    uint32_t numClocks = 0;
    double step = 0;
    uint32_t handleComponentCount = 1u;
    MockXeFrequencySysfsAccess *sysfsAccess = nullptr;

    void SetUp() override {
        SysmanDeviceFixture::SetUp();
        device = pSysmanDevice;

        pSysmanKmdInterface = new MockSysmanKmdInterfaceXe(pLinuxSysmanImp->getSysmanProductHelper());
        pSysmanKmdInterface->pSysfsAccess = std::make_unique<MockXeFrequencySysfsAccess>();
        pLinuxSysmanImp->pSysfsAccess = pSysmanKmdInterface->pSysfsAccess.get();
        pLinuxSysmanImp->pSysmanKmdInterface.reset(pSysmanKmdInterface);

        auto &rootDeviceEnvironment = pLinuxSysmanImp->getParentSysmanDeviceImp()->getRootDeviceEnvironmentRef();
        if (rootDeviceEnvironment.getHardwareInfo()->capabilityTable.supportsImages) {
            handleComponentCount = 2;
        }

        sysfsAccess = static_cast<MockXeFrequencySysfsAccess *>(pSysmanKmdInterface->pSysfsAccess.get());
        sysfsAccess->setVal(minFreqFile, minFreq);
        sysfsAccess->setVal(maxFreqFile, maxFreq);
        sysfsAccess->setVal(requestFreqFile, request);
        sysfsAccess->setVal(actualFreqFile, actual);
        sysfsAccess->setVal(efficientFreqFile, efficient);
        sysfsAccess->setVal(maxValFreqFile, maxVal);
        sysfsAccess->setVal(minValFreqFile, minVal);
        step = 50;
        numClocks = static_cast<uint32_t>((maxFreq - minFreq) / step) + 1;
        for (auto handle : pSysmanDeviceImp->pFrequencyHandleContext->handleList) {
            delete handle;
        }
        pSysmanDeviceImp->pFrequencyHandleContext->handleList.clear();
    }

    void TearDown() override {
        SysmanDeviceFixture::TearDown();
    }

    double clockValue(const double calculatedClock) {
        uint32_t actualClock = static_cast<uint32_t>(calculatedClock + 0.5);
        return static_cast<double>(actualClock);
    }

    std::vector<zes_freq_handle_t> getFreqHandles(uint32_t count) {
        std::vector<zes_freq_handle_t> handles(count, nullptr);
        EXPECT_EQ(zesDeviceEnumFrequencyDomains(device->toHandle(), &count, handles.data()), ZE_RESULT_SUCCESS);
        return handles;
    }
};

TEST_F(SysmanDeviceFrequencyFixtureXe, GivenSysmanKmdInterfaceInstanceWhenCheckingAvailabilityOfFrequencyFilesThenFalseValueIsReturned) {
    auto pSysmanKmdInterface = pLinuxSysmanImp->pSysmanKmdInterface.get();
    EXPECT_FALSE(pSysmanKmdInterface->isDefaultFrequencyAvailable());
    EXPECT_FALSE(pSysmanKmdInterface->isBoostFrequencyAvailable());
    EXPECT_FALSE(pSysmanKmdInterface->isTdpFrequencyAvailable());
}

TEST_F(SysmanDeviceFrequencyFixtureXe, GivenComponentCountZeroWhenEnumeratingFrequencyHandlesThenNonZeroCountIsReturnedAndCallSucceds) {
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

TEST_F(SysmanDeviceFrequencyFixtureXe, GivenComponentCountZeroAndValidPtrWhenEnumeratingFrequencyHandlesThenNonZeroCountAndNoHandlesAreReturnedAndCallSucceds) {
    uint32_t count = 0U;
    zes_freq_handle_t handle = static_cast<zes_freq_handle_t>(0UL);

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEnumFrequencyDomains(device->toHandle(), &count, &handle));
    EXPECT_EQ(count, handleComponentCount);
    EXPECT_EQ(handle, static_cast<zes_freq_handle_t>(0UL));
}

TEST_F(SysmanDeviceFrequencyFixtureXe, GivenActualComponentCountTwoWhenTryingToGetOneComponentOnlyThenOneComponentIsReturnedAndCountUpdated) {
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

TEST_F(SysmanDeviceFrequencyFixtureXe, GivenValidFrequencyHandleWhenCallingzesFrequencyGetPropertiesThenSuccessIsReturned) {
    MockSysmanProductHelper *pMockSysmanProductHelper = new MockSysmanProductHelper();
    pMockSysmanProductHelper->isFrequencySetRangeSupportedResult = true;
    std::unique_ptr<SysmanProductHelper> pSysmanProductHelper(static_cast<SysmanProductHelper *>(pMockSysmanProductHelper));
    std::swap(pLinuxSysmanImp->pSysmanProductHelper, pSysmanProductHelper);

    auto handles = getFreqHandles(handleComponentCount);
    for (auto &handle : handles) {
        EXPECT_NE(handle, nullptr);
        zes_freq_properties_t properties;
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesFrequencyGetProperties(handle, &properties));
        EXPECT_EQ(nullptr, properties.pNext);
        EXPECT_FALSE(properties.onSubdevice);
        EXPECT_DOUBLE_EQ(maxFreq, properties.max);
        EXPECT_DOUBLE_EQ(minFreq, properties.min);
        EXPECT_TRUE(properties.canControl);
    }

    if (handleComponentCount == 1) {
        zes_freq_properties_t properties{};
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesFrequencyGetProperties(handles[0], &properties));
        EXPECT_EQ(ZES_FREQ_DOMAIN_GPU, properties.type);
    } else {
        zes_freq_properties_t properties{};
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesFrequencyGetProperties(handles[0], &properties));
        EXPECT_EQ(ZES_FREQ_DOMAIN_MEDIA, properties.type);
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesFrequencyGetProperties(handles[1], &properties));
        EXPECT_EQ(ZES_FREQ_DOMAIN_GPU, properties.type);
    }
}

HWTEST2_F(SysmanDeviceFrequencyFixtureXe, GivenValidFrequencyHandleAndZeroCountWhenCallingzesFrequencyGetAvailableClocksThenCallSucceeds, IsPVC) {
    auto handles = getFreqHandles(handleComponentCount);
    for (auto handle : handles) {
        EXPECT_NE(handle, nullptr);
        uint32_t count = 0;
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesFrequencyGetAvailableClocks(handle, &count, nullptr));
        EXPECT_EQ(numClocks, count);
    }
}

HWTEST2_F(SysmanDeviceFrequencyFixtureXe, GivenValidFrequencyHandleAndZeroCountWhenCountIsMoreThanNumClocksThenCallSucceeds, IsPVC) {
    auto handles = getFreqHandles(handleComponentCount);
    for (auto handle : handles) {
        EXPECT_NE(handle, nullptr);
        uint32_t count = 80;
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesFrequencyGetAvailableClocks(handle, &count, nullptr));
        EXPECT_EQ(numClocks, count);
    }
}

HWTEST2_F(SysmanDeviceFrequencyFixtureXe, GivenValidFrequencyHandleAndZeroCountWhenCountIsLessThanNumClocksThenCallSucceeds, IsPVC) {
    auto handles = getFreqHandles(handleComponentCount);
    for (auto handle : handles) {
        EXPECT_NE(handle, nullptr);
        uint32_t count = 20;
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesFrequencyGetAvailableClocks(handle, &count, nullptr));
    }
}

HWTEST2_F(SysmanDeviceFrequencyFixtureXe, GivenValidFrequencyHandleAndCorrectCountWhenCallingzesFrequencyGetAvailableClocksThenCallSucceeds, IsPVC) {
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

TEST_F(SysmanDeviceFrequencyFixtureXe, GivenValidateFrequencyGetRangeWhengetMaxAndgetMinFailsThenFrequencyGetRangeCallReturnsNegativeValuesForRange) {
    auto subDeviceCount = pLinuxSysmanImp->getSubDeviceCount();
    ze_bool_t onSubdevice = (subDeviceCount == 0) ? false : true;
    uint32_t subdeviceId = 0;
    auto pFrequencyImp = std::make_unique<L0::Sysman::FrequencyImp>(pOsSysman, onSubdevice, subdeviceId, ZES_FREQ_DOMAIN_GPU);
    zes_freq_range_t limit = {};
    sysfsAccess->mockReadDoubleValResult = ZE_RESULT_ERROR_NOT_AVAILABLE;
    EXPECT_EQ(ZE_RESULT_SUCCESS, pFrequencyImp->frequencyGetRange(&limit));
    EXPECT_EQ(-1, limit.max);
    EXPECT_EQ(-1, limit.min);

    sysfsAccess->mockReadDoubleValResult = ZE_RESULT_ERROR_UNKNOWN;
    EXPECT_EQ(ZE_RESULT_SUCCESS, pFrequencyImp->frequencyGetRange(&limit));
    EXPECT_EQ(-1, limit.max);
    EXPECT_EQ(-1, limit.min);
}

TEST_F(SysmanDeviceFrequencyFixtureXe, GivenValidFrequencyHandleWhenCallingzesFrequencyGetRangeThenVerifyzesFrequencyGetRangeTestCallSucceeds) {
    auto handles = getFreqHandles(handleComponentCount);
    for (auto handle : handles) {
        zes_freq_range_t limits;
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesFrequencyGetRange(handle, &limits));
        EXPECT_DOUBLE_EQ(minFreq, limits.min);
        EXPECT_DOUBLE_EQ(maxFreq, limits.max);
    }
}

HWTEST2_F(SysmanDeviceFrequencyFixtureXe, GivenValidFrequencyLimitsWhenCallingFrequencySetRangeForFailures1ThenAPIExitsGracefully, IsXeCore) {
    auto subDeviceCount = pLinuxSysmanImp->getSubDeviceCount();
    ze_bool_t onSubdevice = (subDeviceCount == 0) ? false : true;
    uint32_t subdeviceId = 0;
    auto pFrequencyImp = std::make_unique<L0::Sysman::FrequencyImp>(pOsSysman, onSubdevice, subdeviceId, ZES_FREQ_DOMAIN_GPU);
    zes_freq_range_t limits = {};

    // Verify that Max must be within range.
    limits.min = minFreq;
    limits.max = 600.0;
    sysfsAccess->mockWriteMinResult = ZE_RESULT_ERROR_NOT_AVAILABLE;
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, pFrequencyImp->frequencySetRange(&limits));

    sysfsAccess->mockWriteMinResult = ZE_RESULT_ERROR_UNKNOWN;
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, pFrequencyImp->frequencySetRange(&limits));
}

HWTEST2_F(SysmanDeviceFrequencyFixtureXe, GivenValidFrequencyLimitsWhenCallingFrequencySetRangeForFailures2ThenAPIExitsGracefully, IsXeCore) {
    auto subDeviceCount = pLinuxSysmanImp->getSubDeviceCount();
    ze_bool_t onSubdevice = (subDeviceCount == 0) ? false : true;
    uint32_t subdeviceId = 0;
    auto pFrequencyImp = std::make_unique<L0::Sysman::FrequencyImp>(pOsSysman, onSubdevice, subdeviceId, ZES_FREQ_DOMAIN_GPU);
    zes_freq_range_t limits = {};

    // Verify that Max must be within range.
    limits.min = 900.0;
    limits.max = maxFreq;
    sysfsAccess->mockWriteMaxResult = ZE_RESULT_ERROR_NOT_AVAILABLE;
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, pFrequencyImp->frequencySetRange(&limits));

    sysfsAccess->mockWriteMaxResult = ZE_RESULT_ERROR_UNKNOWN;
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, pFrequencyImp->frequencySetRange(&limits));
}

HWTEST2_F(SysmanDeviceFrequencyFixtureXe, GivenValidFrequencyHandleWhenCallingzesFrequencySetRangeThenVerifyzesFrequencySetRangeTest1CallSucceeds, IsXeCore) {
    auto handles = getFreqHandles(handleComponentCount);
    for (auto handle : handles) {
        const double startingMin = 900.0;
        const double newMax = 600.0;
        zes_freq_range_t limits;

        sysfsAccess->setVal(minFreqFile, startingMin);
        // If the new Max value is less than the old Min
        // value, the new Min must be set before the new Max
        limits.min = minFreq;
        limits.max = newMax;
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesFrequencySetRange(handle, &limits));
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesFrequencyGetRange(handle, &limits));
        EXPECT_DOUBLE_EQ(minFreq, limits.min);
        EXPECT_DOUBLE_EQ(newMax, limits.max);
    }
}

TEST_F(SysmanDeviceFrequencyFixtureXe, GivenNegativeRangeWhenSetRangeIsCalledAndSettingMaxValueFailsThenFailureIsReturned) {
    sysfsAccess->mockWriteMaxResult = ZE_RESULT_ERROR_NOT_AVAILABLE;
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

HWTEST2_F(SysmanDeviceFrequencyFixtureXe, GivenNegativeRangeWhenSetRangeIsCalledAndGettingDefaultMaxValueFailsThenNoFreqRangeIsInEffect, IsXeCore) {
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

HWTEST2_F(SysmanDeviceFrequencyFixtureXe, GivenValidFrequencyHandleWhenCallingzesFrequencySetRangeThenVerifyzesFrequencySetRangeTest2CallSucceeds, IsXeCore) {
    auto handles = getFreqHandles(handleComponentCount);
    for (auto handle : handles) {
        const double startingMax = 600.0;
        const double newMin = 900.0;
        zes_freq_range_t limits;

        sysfsAccess->setVal(maxFreqFile, startingMax);
        // If the new Min value is greater than the old Max
        // value, the new Max must be set before the new Min
        limits.min = newMin;
        limits.max = maxFreq;
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesFrequencySetRange(handle, &limits));
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesFrequencyGetRange(handle, &limits));
        EXPECT_DOUBLE_EQ(newMin, limits.min);
        EXPECT_DOUBLE_EQ(maxFreq, limits.max);
    }
}

TEST_F(SysmanDeviceFrequencyFixtureXe, GivenInvalidFrequencyLimitsWhenCallingFrequencySetRangeThenVerifyFrequencySetRangeTest4ReturnsError) {
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

TEST_F(SysmanDeviceFrequencyFixtureXe, GivenFrequencySetRangeNotSupportedWhenCallingzesFrequencySetRangeThenVerifyzesFrequencySetRangeFails) {
    std::unique_ptr<SysmanProductHelper> pSysmanProductHelper = std::make_unique<MockSysmanProductHelper>();
    std::swap(pLinuxSysmanImp->pSysmanProductHelper, pSysmanProductHelper);

    auto handles = getFreqHandles(handleComponentCount);
    for (auto handle : handles) {
        const double startingMin = 900.0;
        const double newMax = 600.0;
        zes_freq_range_t limits;

        sysfsAccess->setVal(minFreqFile, startingMin);
        // If the new Max value is less than the old Min
        // value, the new Min must be set before the new Max
        limits.min = minFreq;
        limits.max = newMax;
        EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesFrequencySetRange(handle, &limits));
    }
}

TEST_F(SysmanDeviceFrequencyFixtureXe, GivenValidFrequencyHandleWhenCallingzesFrequencyGetStateThenVerifyzesFrequencyGetStateTestCallSucceeds) {
    auto handles = getFreqHandles(handleComponentCount);
    for (auto handle : handles) {
        const double testRequestValue = 450.0;
        const double testEfficientValue = 400.0;
        const double testActualValue = 550.0;
        const uint32_t invalidReason = 0;
        zes_freq_state_t state;

        sysfsAccess->setValU32(throttleReasonStatusFile, invalidReason);
        sysfsAccess->setVal(requestFreqFile, testRequestValue);
        sysfsAccess->setVal(actualFreqFile, testActualValue);
        sysfsAccess->setVal(efficientFreqFile, testEfficientValue);
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesFrequencyGetState(handle, &state));
        EXPECT_DOUBLE_EQ(testRequestValue, state.request);
        EXPECT_DOUBLE_EQ(testEfficientValue, state.efficient);
        EXPECT_DOUBLE_EQ(testActualValue, state.actual);
        EXPECT_EQ(0u, state.throttleReasons);
        EXPECT_EQ(nullptr, state.pNext);
        EXPECT_LE(state.currentVoltage, 0);
    }
}

TEST_F(SysmanDeviceFrequencyFixtureXe, GivenValidStatePointerWhenValidatingfrequencyGetStateWhenOneOfTheFrequencyStateThenNegativeValueIsReturned) {
    auto subDeviceCount = pLinuxSysmanImp->getSubDeviceCount();
    ze_bool_t onSubdevice = (subDeviceCount == 0) ? false : true;
    uint32_t subdeviceId = 0;
    auto pFrequencyImp = std::make_unique<L0::Sysman::FrequencyImp>(pOsSysman, onSubdevice, subdeviceId, ZES_FREQ_DOMAIN_GPU);
    zes_freq_state_t state = {};
    sysfsAccess->mockReadRequestResult = ZE_RESULT_ERROR_NOT_AVAILABLE;
    EXPECT_EQ(ZE_RESULT_SUCCESS, pFrequencyImp->frequencyGetState(&state));
    EXPECT_EQ(-1, state.request);

    sysfsAccess->mockReadRequestResult = ZE_RESULT_ERROR_UNKNOWN;
    EXPECT_EQ(ZE_RESULT_SUCCESS, pFrequencyImp->frequencyGetState(&state));
    EXPECT_EQ(-1, state.request);

    sysfsAccess->mockReadEfficientResult = ZE_RESULT_ERROR_UNKNOWN;
    EXPECT_EQ(ZE_RESULT_SUCCESS, pFrequencyImp->frequencyGetState(&state));
    EXPECT_EQ(-1, state.efficient);

    sysfsAccess->mockReadEfficientResult = ZE_RESULT_SUCCESS;
    sysfsAccess->mockReadActualResult = ZE_RESULT_ERROR_NOT_AVAILABLE;
    EXPECT_EQ(ZE_RESULT_SUCCESS, pFrequencyImp->frequencyGetState(&state));
    EXPECT_EQ(-1, state.actual);

    sysfsAccess->mockReadActualResult = ZE_RESULT_ERROR_UNKNOWN;
    EXPECT_EQ(ZE_RESULT_SUCCESS, pFrequencyImp->frequencyGetState(&state));
    EXPECT_EQ(-1, state.actual);
}

TEST_F(SysmanDeviceFrequencyFixtureXe, GivenThrottleTimeStructPointerWhenCallingfrequencyGetThrottleTimeThenUnsupportedIsReturned) {
    auto subDeviceCount = pLinuxSysmanImp->getSubDeviceCount();
    ze_bool_t onSubdevice = (subDeviceCount == 0) ? false : true;
    uint32_t subdeviceId = 0;
    auto pFrequencyImp = std::make_unique<L0::Sysman::FrequencyImp>(pOsSysman, onSubdevice, subdeviceId, ZES_FREQ_DOMAIN_GPU);
    zes_freq_throttle_time_t throttleTime = {};
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, pFrequencyImp->frequencyGetThrottleTime(&throttleTime));
}

TEST_F(SysmanDeviceFrequencyFixtureXe, GivengetMinFunctionReturnsErrorWhenValidatinggetMinFailuresThenAPIReturnsErrorAccordingly) {
    PublicLinuxFrequencyImp linuxFrequencyImp(pOsSysman, 0, 0, ZES_FREQ_DOMAIN_GPU);
    double min = 0;
    sysfsAccess->mockReadDoubleValResult = ZE_RESULT_ERROR_NOT_AVAILABLE;
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, linuxFrequencyImp.getMin(min));

    sysfsAccess->mockReadDoubleValResult = ZE_RESULT_ERROR_UNKNOWN;
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, linuxFrequencyImp.getMin(min));
}

TEST_F(SysmanDeviceFrequencyFixtureXe, GivengetMinValFunctionReturnsErrorWhenValidatinggetMinValFailuresThenAPIReturnsErrorAccordingly) {
    PublicLinuxFrequencyImp linuxFrequencyImp(pOsSysman, 0, 0, ZES_FREQ_DOMAIN_GPU);
    double val = 0;
    sysfsAccess->mockReadMinValResult = ZE_RESULT_ERROR_NOT_AVAILABLE;
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, linuxFrequencyImp.getMinVal(val));

    sysfsAccess->mockReadMinValResult = ZE_RESULT_ERROR_UNKNOWN;
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, linuxFrequencyImp.getMinVal(val));
}

TEST_F(SysmanDeviceFrequencyFixtureXe, GivengetMaxValFunctionReturnsErrorWhenValidatinggetMaxValFailuresThenAPIReturnsErrorAccordingly) {
    PublicLinuxFrequencyImp linuxFrequencyImp(pOsSysman, 0, 0, ZES_FREQ_DOMAIN_GPU);
    double val = 0;
    sysfsAccess->mockReadMaxValResult = ZE_RESULT_ERROR_NOT_AVAILABLE;
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, linuxFrequencyImp.getMaxVal(val));

    sysfsAccess->mockReadMaxValResult = ZE_RESULT_ERROR_UNKNOWN;
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, linuxFrequencyImp.getMaxVal(val));
}

TEST_F(SysmanDeviceFrequencyFixtureXe, GivengetMaxValFunctionReturnsErrorWhenValidatingosFrequencyGetPropertiesThenAPIBehavesAsExpected) {
    zes_freq_properties_t properties = {};
    PublicLinuxFrequencyImp linuxFrequencyImp(pOsSysman, 0, 0, ZES_FREQ_DOMAIN_GPU);
    sysfsAccess->mockReadMaxValResult = ZE_RESULT_ERROR_NOT_AVAILABLE;
    EXPECT_EQ(ZE_RESULT_SUCCESS, linuxFrequencyImp.osFrequencyGetProperties(properties));
    EXPECT_EQ(0, properties.canControl);
}

TEST_F(SysmanDeviceFrequencyFixtureXe, GivengetMinValFunctionReturnsErrorWhenValidatingosFrequencyGetPropertiesThenAPIBehavesAsExpected) {
    zes_freq_properties_t properties = {};
    PublicLinuxFrequencyImp linuxFrequencyImp(pOsSysman, 0, 0, ZES_FREQ_DOMAIN_GPU);
    sysfsAccess->mockReadMinValResult = ZE_RESULT_ERROR_NOT_AVAILABLE;
    EXPECT_EQ(ZE_RESULT_SUCCESS, linuxFrequencyImp.osFrequencyGetProperties(properties));
    EXPECT_EQ(0, properties.canControl);
}

TEST_F(SysmanDeviceFrequencyFixtureXe, GivenOnSubdeviceSetWhenValidatingAnyFrequencyAPIThenSuccessIsReturned) {
    MockSysmanProductHelper *pMockSysmanProductHelper = new MockSysmanProductHelper();
    pMockSysmanProductHelper->isFrequencySetRangeSupportedResult = true;
    std::unique_ptr<SysmanProductHelper> pSysmanProductHelper(static_cast<SysmanProductHelper *>(pMockSysmanProductHelper));
    std::swap(pLinuxSysmanImp->pSysmanProductHelper, pSysmanProductHelper);

    zes_freq_properties_t properties = {};
    PublicLinuxFrequencyImp linuxFrequencyImp(pOsSysman, 1, 0, ZES_FREQ_DOMAIN_GPU);
    EXPECT_EQ(ZE_RESULT_SUCCESS, linuxFrequencyImp.osFrequencyGetProperties(properties));
    EXPECT_EQ(1, properties.canControl);
}

TEST_F(SysmanDeviceFrequencyFixtureXe, GivenValidFrequencyHandleWhenCallingzesFrequencySetRangeAndIfgetMaxFailsThenVerifyzesFrequencySetRangeTestCallFail) {
    auto handles = getFreqHandles(handleComponentCount);
    for (auto handle : handles) {
        const double startingMax = 600.0;
        const double newMin = 900.0;
        zes_freq_range_t limits;

        sysfsAccess->setVal(maxFreqFile, startingMax);
        // If the new Min value is greater than the old Max
        // value, the new Max must be set before the new Min
        limits.min = newMin;
        limits.max = maxFreq;
        sysfsAccess->mockReadMaxResult = ZE_RESULT_ERROR_NOT_AVAILABLE;
        EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesFrequencySetRange(handle, &limits));
    }
}

TEST_F(SysmanDeviceFrequencyFixtureXe, GivenValidFrequencyHandleWhenCallingzesFrequencySetRangeAndIfsetMaxFailsThenVerifyzesFrequencySetRangeTestCallFail) {
    auto handles = getFreqHandles(handleComponentCount);
    for (auto handle : handles) {
        const double startingMax = 600.0;
        const double newMin = 900.0;
        zes_freq_range_t limits;

        sysfsAccess->setVal(maxFreqFile, startingMax);
        // If the new Min value is greater than the old Max
        // value, the new Max must be set before the new Min
        limits.min = newMin;
        limits.max = maxFreq;
        sysfsAccess->mockWriteMaxResult = ZE_RESULT_ERROR_NOT_AVAILABLE;
        EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesFrequencySetRange(handle, &limits));
    }
}

TEST_F(SysmanDeviceFrequencyFixtureXe, GivenValidFrequencyHandleWhenCallingzesFrequencyOcGetFrequencyTargetThenVerifyTestCallFail) {
    auto handles = getFreqHandles(handleComponentCount);
    for (auto handle : handles) {
        double freqTarget = 0.0;
        EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesFrequencyOcGetFrequencyTarget(handle, &freqTarget));
    }
}

TEST_F(SysmanDeviceFrequencyFixtureXe, GivenValidFrequencyHandleWhenCallingzesFrequencyOcSetFrequencyTargetThenVerifyTestCallFail) {
    auto handles = getFreqHandles(handleComponentCount);
    for (auto handle : handles) {
        double freqTarget = 0.0;
        EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesFrequencyOcSetFrequencyTarget(handle, freqTarget));
    }
}

TEST_F(SysmanDeviceFrequencyFixtureXe, GivenValidFrequencyHandleWhenCallingzesFrequencyOcGetVoltageTargetThenVerifyTestCallFail) {
    auto handles = getFreqHandles(handleComponentCount);
    for (auto handle : handles) {
        double voltTarget = 0.0, voltOffset = 0.0;
        EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesFrequencyOcGetVoltageTarget(handle, &voltTarget, &voltOffset));
    }
}

TEST_F(SysmanDeviceFrequencyFixtureXe, GivenValidFrequencyHandleWhenCallingzesFrequencyOcSetVoltageTargetThenVerifyTestCallFail) {
    auto handles = getFreqHandles(handleComponentCount);
    for (auto handle : handles) {
        double voltTarget = 0.0, voltOffset = 0.0;
        EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesFrequencyOcSetVoltageTarget(handle, voltTarget, voltOffset));
    }
}

TEST_F(SysmanDeviceFrequencyFixtureXe, GivenValidFrequencyHandleWhenCallingzesFrequencyOcSetModeThenVerifyTestCallFail) {
    auto handles = getFreqHandles(handleComponentCount);
    for (auto handle : handles) {
        zes_oc_mode_t mode = ZES_OC_MODE_OFF;
        EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesFrequencyOcSetMode(handle, mode));
    }
}

TEST_F(SysmanDeviceFrequencyFixtureXe, GivenValidFrequencyHandleWhenCallingzesFrequencyOcGetModeThenVerifyTestCallFail) {
    auto handles = getFreqHandles(handleComponentCount);
    for (auto handle : handles) {
        zes_oc_mode_t mode = ZES_OC_MODE_OFF;
        EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesFrequencyOcGetMode(handle, &mode));
    }
}

TEST_F(SysmanDeviceFrequencyFixtureXe, GivenValidFrequencyHandleWhenCallingzesFrequencyOcGetCapabilitiesThenVerifyTestCallFail) {
    auto handles = getFreqHandles(handleComponentCount);
    for (auto handle : handles) {
        zes_oc_capabilities_t caps = {};
        EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesFrequencyOcGetCapabilities(handle, &caps));
    }
}

TEST_F(SysmanDeviceFrequencyFixtureXe, GivenValidFrequencyHandleWhenCallingzesFrequencyOcGetIccMaxThenVerifyTestCallFail) {
    auto handles = getFreqHandles(handleComponentCount);
    for (auto handle : handles) {
        double iccMax = 0.0;
        EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesFrequencyOcGetIccMax(handle, &iccMax));
    }
}

TEST_F(SysmanDeviceFrequencyFixtureXe, GivenValidFrequencyHandleWhenCallingzesFrequencyOcSetIccMaxThenVerifyTestCallFail) {
    auto handles = getFreqHandles(handleComponentCount);
    for (auto handle : handles) {
        double iccMax = 0.0;
        EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesFrequencyOcSetIccMax(handle, iccMax));
    }
}

TEST_F(SysmanDeviceFrequencyFixtureXe, GivenValidFrequencyHandleWhenCallingzesFrequencyOcGetTjMaxThenVerifyTestCallFail) {
    auto handles = getFreqHandles(handleComponentCount);
    for (auto handle : handles) {
        double tjMax = 0.0;
        EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesFrequencyOcGetTjMax(handle, &tjMax));
    }
}

TEST_F(SysmanDeviceFrequencyFixtureXe, GivenValidFrequencyHandleWhenCallingzesFrequencyOcSetTjMaxThenVerifyTestCallFail) {
    auto handles = getFreqHandles(handleComponentCount);
    for (auto handle : handles) {
        double tjMax = 0.0;
        EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesFrequencyOcSetTjMax(handle, tjMax));
    }
}

} // namespace ult
} // namespace Sysman
} // namespace L0
