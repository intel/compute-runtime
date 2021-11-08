/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/test/unit_tests/sources/sysman/linux/mock_sysman_fixture.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "mock_sysfs_frequency.h"

#include <cmath>

extern bool sysmanUltsEnable;

using ::testing::Invoke;

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
    std::unique_ptr<Mock<FrequencySysfsAccess>> pSysfsAccess;
    SysfsAccess *pSysfsAccessOld = nullptr;
    std::vector<ze_device_handle_t> deviceHandles;

    void SetUp() override {
        if (!sysmanUltsEnable) {
            GTEST_SKIP();
        }
        SysmanDeviceFixture::SetUp();
        pSysfsAccessOld = pLinuxSysmanImp->pSysfsAccess;
        pSysfsAccess = std::make_unique<NiceMock<Mock<FrequencySysfsAccess>>>();
        pLinuxSysmanImp->pSysfsAccess = pSysfsAccess.get();

        pSysfsAccess->setVal(minFreqFile, minFreq);
        pSysfsAccess->setVal(maxFreqFile, maxFreq);
        pSysfsAccess->setVal(requestFreqFile, request);
        pSysfsAccess->setVal(tdpFreqFile, tdp);
        pSysfsAccess->setVal(actualFreqFile, actual);
        pSysfsAccess->setVal(efficientFreqFile, efficient);
        pSysfsAccess->setVal(maxValFreqFile, maxVal);
        pSysfsAccess->setVal(minValFreqFile, minVal);
        ON_CALL(*pSysfsAccess.get(), read(_, _))
            .WillByDefault(::testing::Invoke(pSysfsAccess.get(), &Mock<FrequencySysfsAccess>::getVal));
        ON_CALL(*pSysfsAccess.get(), write(_, _))
            .WillByDefault(::testing::Invoke(pSysfsAccess.get(), &Mock<FrequencySysfsAccess>::setVal));
        ON_CALL(*pSysfsAccess.get(), directoryExists(_))
            .WillByDefault(::testing::Invoke(pSysfsAccess.get(), &Mock<FrequencySysfsAccess>::mockDirectoryExistsSuccess));

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
        pSysmanDeviceImp->pFrequencyHandleContext->init(deviceHandles);
    }

    void TearDown() override {
        if (!sysmanUltsEnable) {
            GTEST_SKIP();
        }
        SysmanDeviceFixture::TearDown();
        pLinuxSysmanImp->pSysfsAccess = pSysfsAccessOld;
    }

    double clockValue(const double calculatedClock) {
        // i915 specific. frequency step is a fraction
        // However, the i915 represents all clock
        // rates as integer values. So clocks are
        // rounded to the nearest integer.
        uint32_t actualClock = static_cast<uint32_t>(calculatedClock + 0.5);
        return static_cast<double>(actualClock);
    }

    std::vector<zes_freq_handle_t> get_freq_handles(uint32_t count) {
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

    auto handles = get_freq_handles(count);
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
    auto handles = get_freq_handles(handleComponentCount);
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
    auto handles = get_freq_handles(handleComponentCount);
    for (auto handle : handles) {
        EXPECT_NE(handle, nullptr);
        uint32_t count = 0;
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesFrequencyGetAvailableClocks(handle, &count, nullptr));
        EXPECT_EQ(numClocks, count);
    }
}

TEST_F(SysmanDeviceFrequencyFixture, GivenValidFrequencyHandleAndZeroCountWhenCountIsMoreThanNumClocksThenCallSucceeds) {
    auto handles = get_freq_handles(handleComponentCount);
    for (auto handle : handles) {
        EXPECT_NE(handle, nullptr);
        uint32_t count = 80;
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesFrequencyGetAvailableClocks(handle, &count, nullptr));
        EXPECT_EQ(numClocks, count);
    }
}

TEST_F(SysmanDeviceFrequencyFixture, GivenValidFrequencyHandleAndZeroCountWhenCountIsLessThanNumClocksThenCallSucceeds) {
    auto handles = get_freq_handles(handleComponentCount);
    for (auto handle : handles) {
        EXPECT_NE(handle, nullptr);
        uint32_t count = 20;
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesFrequencyGetAvailableClocks(handle, &count, nullptr));
    }
}

TEST_F(SysmanDeviceFrequencyFixture, GivenValidFrequencyHandleAndCorrectCountWhenCallingzesFrequencyGetAvailableClocksThenCallSucceeds) {
    auto handles = get_freq_handles(handleComponentCount);
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
    ON_CALL(*pSysfsAccess.get(), read(_, _))
        .WillByDefault(::testing::Invoke(pSysfsAccess.get(), &Mock<FrequencySysfsAccess>::getValReturnErrorNotAvailable));
    EXPECT_EQ(ZE_RESULT_SUCCESS, pFrequencyImp->frequencyGetRange(&limit));
    EXPECT_EQ(-1, limit.max);
    EXPECT_EQ(-1, limit.min);

    ON_CALL(*pSysfsAccess.get(), read(_, _))
        .WillByDefault(::testing::Invoke(pSysfsAccess.get(), &Mock<FrequencySysfsAccess>::getValReturnErrorUnknown));
    EXPECT_EQ(ZE_RESULT_SUCCESS, pFrequencyImp->frequencyGetRange(&limit));
    EXPECT_EQ(-1, limit.max);
    EXPECT_EQ(-1, limit.min);
}

TEST_F(SysmanDeviceFrequencyFixture, GivenValidFrequencyHandleWhenCallingzesFrequencyGetRangeThenVerifyzesFrequencyGetRangeTestCallSucceeds) {
    auto handles = get_freq_handles(handleComponentCount);
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
    ON_CALL(*pSysfsAccess.get(), write(_, _))
        .WillByDefault(::testing::Invoke(pSysfsAccess.get(), &Mock<FrequencySysfsAccess>::setValMinReturnErrorNotAvailable));
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, pFrequencyImp->frequencySetRange(&limits));

    ON_CALL(*pSysfsAccess.get(), write(_, _))
        .WillByDefault(::testing::Invoke(pSysfsAccess.get(), &Mock<FrequencySysfsAccess>::setValMinReturnErrorUnknown));
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, pFrequencyImp->frequencySetRange(&limits));
}

TEST_F(SysmanDeviceFrequencyFixture, GivenValidFrequencyLimitsWhenCallingFrequencySetRangeForFailures2ThenAPIExitsGracefully) {
    auto pFrequencyImp = std::make_unique<FrequencyImp>(pOsSysman, device->toHandle(), ZES_FREQ_DOMAIN_GPU);
    zes_freq_range_t limits = {};

    // Verify that Max must be within range.
    limits.min = 900.0;
    limits.max = maxFreq;
    ON_CALL(*pSysfsAccess.get(), write(_, _))
        .WillByDefault(::testing::Invoke(pSysfsAccess.get(), &Mock<FrequencySysfsAccess>::setValMaxReturnErrorNotAvailable));
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, pFrequencyImp->frequencySetRange(&limits));

    ON_CALL(*pSysfsAccess.get(), write(_, _))
        .WillByDefault(::testing::Invoke(pSysfsAccess.get(), &Mock<FrequencySysfsAccess>::setValMaxReturnErrorUnknown));
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, pFrequencyImp->frequencySetRange(&limits));
}

TEST_F(SysmanDeviceFrequencyFixture, GivenValidFrequencyHandleWhenCallingzesFrequencySetRangeThenVerifyzesFrequencySetRangeTest1CallSucceeds) {
    auto handles = get_freq_handles(handleComponentCount);
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
    }
}

TEST_F(SysmanDeviceFrequencyFixture, GivenValidFrequencyHandleWhenCallingzesFrequencySetRangeThenVerifyzesFrequencySetRangeTest2CallSucceeds) {
    auto handles = get_freq_handles(handleComponentCount);
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
    auto handles = get_freq_handles(handleComponentCount);
    for (auto handle : handles) {
        const double testRequestValue = 450.0;
        const double testTdpValue = 1200.0;
        const double testEfficientValue = 400.0;
        const double testActualValue = 550.0;
        zes_freq_state_t state;

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
    ON_CALL(*pSysfsAccess.get(), read(_, _))
        .WillByDefault(::testing::Invoke(pSysfsAccess.get(), &Mock<FrequencySysfsAccess>::getValLegacy));
    ON_CALL(*pSysfsAccess.get(), write(_, _))
        .WillByDefault(::testing::Invoke(pSysfsAccess.get(), &Mock<FrequencySysfsAccess>::setValLegacy));
    ON_CALL(*pSysfsAccess.get(), directoryExists(_))
        .WillByDefault(::testing::Invoke(pSysfsAccess.get(), &Mock<FrequencySysfsAccess>::mockDirectoryExistsFailure));
    for (auto handle : pSysmanDeviceImp->pFrequencyHandleContext->handleList) {
        delete handle;
    }
    pSysmanDeviceImp->pFrequencyHandleContext->handleList.clear();
    pSysmanDeviceImp->pFrequencyHandleContext->init(deviceHandles);

    auto handles = get_freq_handles(handleComponentCount);
    for (auto handle : handles) {
        const double testRequestValue = 400.0;
        const double testTdpValue = 1100.0;
        const double testEfficientValue = 300.0;
        const double testActualValue = 550.0;
        zes_freq_state_t state;

        pSysfsAccess->setValLegacy(requestFreqFileLegacy, testRequestValue);
        pSysfsAccess->setValLegacy(tdpFreqFileLegacy, testTdpValue);
        pSysfsAccess->setValLegacy(actualFreqFileLegacy, testActualValue);
        pSysfsAccess->setValLegacy(efficientFreqFileLegacy, testEfficientValue);
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

TEST_F(SysmanDeviceFrequencyFixture, GivenValidFrequencyHandleWhenCallingzesFrequencyGetRangeWithLegacyPathThenVerifyzesFrequencyGetRangeTestCallSucceeds) {
    ON_CALL(*pSysfsAccess.get(), read(_, _))
        .WillByDefault(::testing::Invoke(pSysfsAccess.get(), &Mock<FrequencySysfsAccess>::getValLegacy));
    ON_CALL(*pSysfsAccess.get(), write(_, _))
        .WillByDefault(::testing::Invoke(pSysfsAccess.get(), &Mock<FrequencySysfsAccess>::setValLegacy));
    ON_CALL(*pSysfsAccess.get(), directoryExists(_))
        .WillByDefault(::testing::Invoke(pSysfsAccess.get(), &Mock<FrequencySysfsAccess>::mockDirectoryExistsFailure));
    for (auto handle : pSysmanDeviceImp->pFrequencyHandleContext->handleList) {
        delete handle;
    }
    pSysmanDeviceImp->pFrequencyHandleContext->handleList.clear();
    pSysmanDeviceImp->pFrequencyHandleContext->init(deviceHandles);
    auto handles = get_freq_handles(handleComponentCount);
    double minFreqLegacy = 400.0;
    double maxFreqLegacy = 1200.0;
    pSysfsAccess->setValLegacy(minFreqFileLegacy, minFreqLegacy);
    pSysfsAccess->setValLegacy(maxFreqFileLegacy, maxFreqLegacy);
    for (auto handle : handles) {
        zes_freq_range_t limits;
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesFrequencyGetRange(handle, &limits));
        EXPECT_DOUBLE_EQ(minFreqLegacy, limits.min);
        EXPECT_DOUBLE_EQ(maxFreqLegacy, limits.max);
    }
}

TEST_F(SysmanDeviceFrequencyFixture, GivenValidStatePointerWhenValidatingfrequencyGetStateWhenOneOfTheFrequencyStateThenNegativeValueIsReturned) {
    auto pFrequencyImp = std::make_unique<FrequencyImp>(pOsSysman, device->toHandle(), ZES_FREQ_DOMAIN_GPU);
    zes_freq_state_t state = {};
    ON_CALL(*pSysfsAccess.get(), read(_, _))
        .WillByDefault(::testing::Invoke(pSysfsAccess.get(), &Mock<FrequencySysfsAccess>::getValRequestReturnErrorNotAvailable));
    EXPECT_EQ(ZE_RESULT_SUCCESS, pFrequencyImp->frequencyGetState(&state));
    EXPECT_EQ(-1, state.request);

    ON_CALL(*pSysfsAccess.get(), read(_, _))
        .WillByDefault(::testing::Invoke(pSysfsAccess.get(), &Mock<FrequencySysfsAccess>::getValRequestReturnErrorUnknown));
    EXPECT_EQ(ZE_RESULT_SUCCESS, pFrequencyImp->frequencyGetState(&state));
    EXPECT_EQ(-1, state.request);

    ON_CALL(*pSysfsAccess.get(), read(_, _))
        .WillByDefault(::testing::Invoke(pSysfsAccess.get(), &Mock<FrequencySysfsAccess>::getValTdpReturnErrorNotAvailable));
    EXPECT_EQ(ZE_RESULT_SUCCESS, pFrequencyImp->frequencyGetState(&state));
    EXPECT_EQ(-1, state.tdp);

    ON_CALL(*pSysfsAccess.get(), read(_, _))
        .WillByDefault(::testing::Invoke(pSysfsAccess.get(), &Mock<FrequencySysfsAccess>::getValTdpReturnErrorUnknown));
    EXPECT_EQ(ZE_RESULT_SUCCESS, pFrequencyImp->frequencyGetState(&state));
    EXPECT_EQ(-1, state.tdp);

    ON_CALL(*pSysfsAccess.get(), read(_, _))
        .WillByDefault(::testing::Invoke(pSysfsAccess.get(), &Mock<FrequencySysfsAccess>::getValEfficientReturnErrorNotAvailable));
    EXPECT_EQ(ZE_RESULT_SUCCESS, pFrequencyImp->frequencyGetState(&state));
    EXPECT_EQ(-1, state.efficient);

    ON_CALL(*pSysfsAccess.get(), read(_, _))
        .WillByDefault(::testing::Invoke(pSysfsAccess.get(), &Mock<FrequencySysfsAccess>::getValEfficientReturnErrorUnknown));
    EXPECT_EQ(ZE_RESULT_SUCCESS, pFrequencyImp->frequencyGetState(&state));
    EXPECT_EQ(-1, state.efficient);

    ON_CALL(*pSysfsAccess.get(), read(_, _))
        .WillByDefault(::testing::Invoke(pSysfsAccess.get(), &Mock<FrequencySysfsAccess>::getValActualReturnErrorNotAvailable));
    EXPECT_EQ(ZE_RESULT_SUCCESS, pFrequencyImp->frequencyGetState(&state));
    EXPECT_EQ(-1, state.actual);

    ON_CALL(*pSysfsAccess.get(), read(_, _))
        .WillByDefault(::testing::Invoke(pSysfsAccess.get(), &Mock<FrequencySysfsAccess>::getValActualReturnErrorUnknown));
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
    ON_CALL(*pSysfsAccess.get(), read(_, _))
        .WillByDefault(::testing::Invoke(pSysfsAccess.get(), &Mock<FrequencySysfsAccess>::getValReturnErrorNotAvailable));
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, linuxFrequencyImp.getMin(min));

    ON_CALL(*pSysfsAccess.get(), read(_, _))
        .WillByDefault(::testing::Invoke(pSysfsAccess.get(), &Mock<FrequencySysfsAccess>::getValReturnErrorUnknown));
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, linuxFrequencyImp.getMin(min));
}

TEST_F(SysmanDeviceFrequencyFixture, GivengetMinValFunctionReturnsErrorWhenValidatinggetMinValFailuresThenAPIReturnsErrorAccordingly) {
    PublicLinuxFrequencyImp linuxFrequencyImp(pOsSysman, 0, 0, ZES_FREQ_DOMAIN_GPU);
    double val = 0;
    ON_CALL(*pSysfsAccess.get(), read(_, _))
        .WillByDefault(::testing::Invoke(pSysfsAccess.get(), &Mock<FrequencySysfsAccess>::getMinValReturnErrorNotAvailable));
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, linuxFrequencyImp.getMinVal(val));

    ON_CALL(*pSysfsAccess.get(), read(_, _))
        .WillByDefault(::testing::Invoke(pSysfsAccess.get(), &Mock<FrequencySysfsAccess>::getMinValReturnErrorUnknown));
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, linuxFrequencyImp.getMinVal(val));
}

TEST_F(SysmanDeviceFrequencyFixture, GivengetMaxValFunctionReturnsErrorWhenValidatinggetMaxValFailuresThenAPIReturnsErrorAccordingly) {
    PublicLinuxFrequencyImp linuxFrequencyImp(pOsSysman, 0, 0, ZES_FREQ_DOMAIN_GPU);
    double val = 0;
    ON_CALL(*pSysfsAccess.get(), read(_, _))
        .WillByDefault(::testing::Invoke(pSysfsAccess.get(), &Mock<FrequencySysfsAccess>::getMaxValReturnErrorNotAvailable));
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, linuxFrequencyImp.getMaxVal(val));

    ON_CALL(*pSysfsAccess.get(), read(_, _))
        .WillByDefault(::testing::Invoke(pSysfsAccess.get(), &Mock<FrequencySysfsAccess>::getMaxValReturnErrorUnknown));
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, linuxFrequencyImp.getMaxVal(val));
}

TEST_F(SysmanDeviceFrequencyFixture, GivengetMaxValFunctionReturnsErrorWhenValidatingosFrequencyGetPropertiesThenAPIBehavesAsExpected) {
    zes_freq_properties_t properties = {};
    PublicLinuxFrequencyImp linuxFrequencyImp(pOsSysman, 0, 0, ZES_FREQ_DOMAIN_GPU);
    ON_CALL(*pSysfsAccess.get(), read(_, _))
        .WillByDefault(::testing::Invoke(pSysfsAccess.get(), &Mock<FrequencySysfsAccess>::getMaxValReturnErrorNotAvailable));
    EXPECT_EQ(ZE_RESULT_SUCCESS, linuxFrequencyImp.osFrequencyGetProperties(properties));
    EXPECT_EQ(0, properties.canControl);
}

TEST_F(SysmanDeviceFrequencyFixture, GivengetMinValFunctionReturnsErrorWhenValidatingosFrequencyGetPropertiesThenAPIBehavesAsExpected) {
    zes_freq_properties_t properties = {};
    PublicLinuxFrequencyImp linuxFrequencyImp(pOsSysman, 0, 0, ZES_FREQ_DOMAIN_GPU);
    ON_CALL(*pSysfsAccess.get(), read(_, _))
        .WillByDefault(::testing::Invoke(pSysfsAccess.get(), &Mock<FrequencySysfsAccess>::getMinValReturnErrorNotAvailable));
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
    auto handles = get_freq_handles(handleComponentCount);
    for (auto handle : handles) {
        const double startingMax = 600.0;
        const double newMin = 900.0;
        zes_freq_range_t limits;

        pSysfsAccess->setVal(maxFreqFile, startingMax);
        // If the new Min value is greater than the old Max
        // value, the new Max must be set before the new Min
        limits.min = newMin;
        limits.max = maxFreq;
        ON_CALL(*pSysfsAccess.get(), read(_, _))
            .WillByDefault(::testing::Invoke(pSysfsAccess.get(), &Mock<FrequencySysfsAccess>::setValMaxReturnErrorNotAvailable));
        EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesFrequencySetRange(handle, &limits));
    }
}

TEST_F(SysmanDeviceFrequencyFixture, GivenValidFrequencyHandleWhenCallingzesFrequencySetRangeAndIfsetMaxFailsThenVerifyzesFrequencySetRangeTestCallFail) {
    auto handles = get_freq_handles(handleComponentCount);
    for (auto handle : handles) {
        const double startingMax = 600.0;
        const double newMin = 900.0;
        zes_freq_range_t limits;

        pSysfsAccess->setVal(maxFreqFile, startingMax);
        // If the new Min value is greater than the old Max
        // value, the new Max must be set before the new Min
        limits.min = newMin;
        limits.max = maxFreq;
        ON_CALL(*pSysfsAccess.get(), write(_, _))
            .WillByDefault(::testing::Invoke(pSysfsAccess.get(), &Mock<FrequencySysfsAccess>::setValMaxReturnErrorNotAvailable));
        EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesFrequencySetRange(handle, &limits));
    }
}

TEST_F(SysmanDeviceFrequencyFixture, GivenValidFrequencyHandleWhenCallingzesFrequencyOcGetFrequencyTargetThenVerifyTestCallFail) {
    auto handles = get_freq_handles(handleComponentCount);
    for (auto handle : handles) {
        double freqTarget = 0.0;
        EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesFrequencyOcGetFrequencyTarget(handle, &freqTarget));
    }
}

TEST_F(SysmanDeviceFrequencyFixture, GivenValidFrequencyHandleWhenCallingzesFrequencyOcSetFrequencyTargetThenVerifyTestCallFail) {
    auto handles = get_freq_handles(handleComponentCount);
    for (auto handle : handles) {
        double freqTarget = 0.0;
        EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesFrequencyOcSetFrequencyTarget(handle, freqTarget));
    }
}

TEST_F(SysmanDeviceFrequencyFixture, GivenValidFrequencyHandleWhenCallingzesFrequencyOcGetVoltageTargetThenVerifyTestCallFail) {
    auto handles = get_freq_handles(handleComponentCount);
    for (auto handle : handles) {
        double voltTarget = 0.0, voltOffset = 0.0;
        EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesFrequencyOcGetVoltageTarget(handle, &voltTarget, &voltOffset));
    }
}

TEST_F(SysmanDeviceFrequencyFixture, GivenValidFrequencyHandleWhenCallingzesFrequencyOcSetVoltageTargetThenVerifyTestCallFail) {
    auto handles = get_freq_handles(handleComponentCount);
    for (auto handle : handles) {
        double voltTarget = 0.0, voltOffset = 0.0;
        EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesFrequencyOcSetVoltageTarget(handle, voltTarget, voltOffset));
    }
}

TEST_F(SysmanDeviceFrequencyFixture, GivenValidFrequencyHandleWhenCallingzesFrequencyOcSetModeThenVerifyTestCallFail) {
    auto handles = get_freq_handles(handleComponentCount);
    for (auto handle : handles) {
        zes_oc_mode_t mode = ZES_OC_MODE_OFF;
        EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesFrequencyOcSetMode(handle, mode));
    }
}

TEST_F(SysmanDeviceFrequencyFixture, GivenValidFrequencyHandleWhenCallingzesFrequencyOcGetModeThenVerifyTestCallFail) {
    auto handles = get_freq_handles(handleComponentCount);
    for (auto handle : handles) {
        zes_oc_mode_t mode = ZES_OC_MODE_OFF;
        EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesFrequencyOcGetMode(handle, &mode));
    }
}

TEST_F(SysmanDeviceFrequencyFixture, GivenValidFrequencyHandleWhenCallingzesFrequencyOcGetCapabilitiesThenVerifyTestCallFail) {
    auto handles = get_freq_handles(handleComponentCount);
    for (auto handle : handles) {
        zes_oc_capabilities_t caps = {};
        EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesFrequencyOcGetCapabilities(handle, &caps));
    }
}

TEST_F(SysmanDeviceFrequencyFixture, GivenValidFrequencyHandleWhenCallingzesFrequencyOcGetIccMaxThenVerifyTestCallFail) {
    auto handles = get_freq_handles(handleComponentCount);
    for (auto handle : handles) {
        double iccMax = 0.0;
        EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesFrequencyOcGetIccMax(handle, &iccMax));
    }
}

TEST_F(SysmanDeviceFrequencyFixture, GivenValidFrequencyHandleWhenCallingzesFrequencyOcSetIccMaxThenVerifyTestCallFail) {
    auto handles = get_freq_handles(handleComponentCount);
    for (auto handle : handles) {
        double iccMax = 0.0;
        EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesFrequencyOcSetIccMax(handle, iccMax));
    }
}

TEST_F(SysmanDeviceFrequencyFixture, GivenValidFrequencyHandleWhenCallingzesFrequencyOcGetTjMaxThenVerifyTestCallFail) {
    auto handles = get_freq_handles(handleComponentCount);
    for (auto handle : handles) {
        double tjMax = 0.0;
        EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesFrequencyOcGetTjMax(handle, &tjMax));
    }
}

TEST_F(SysmanDeviceFrequencyFixture, GivenValidFrequencyHandleWhenCallingzesFrequencyOcSetTjMaxThenVerifyTestCallFail) {
    auto handles = get_freq_handles(handleComponentCount);
    for (auto handle : handles) {
        double tjMax = 0.0;
        EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesFrequencyOcSetTjMax(handle, tjMax));
    }
}

TEST_F(SysmanMultiDeviceFixture, GivenValidDevicePointerWhenGettingFrequencyPropertiesThenValidSchedPropertiesRetrieved) {
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

} // namespace ult
} // namespace L0
