/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/test/unit_tests/sources/sysman/linux/mock_sysman_fixture.h"

#include "mock_sysfs_power.h"

using ::testing::DoDefault;
using ::testing::Matcher;
using ::testing::Return;

namespace L0 {
namespace ult {
constexpr uint32_t powerHandleComponentCount = 1u;
class SysmanDevicePowerFixture : public SysmanDeviceFixture {

  protected:
    std::unique_ptr<Mock<PowerSysfsAccess>> pSysfsAccess;
    SysfsAccess *pSysfsAccessOld = nullptr;
    std::vector<ze_device_handle_t> deviceHandles;

    void SetUp() override {
        SysmanDeviceFixture::SetUp();
        pSysfsAccessOld = pLinuxSysmanImp->pSysfsAccess;
        pSysfsAccess = std::make_unique<NiceMock<Mock<PowerSysfsAccess>>>();
        pLinuxSysmanImp->pSysfsAccess = pSysfsAccess.get();

        ON_CALL(*pSysfsAccess.get(), read(_, Matcher<std::string &>(_)))
            .WillByDefault(::testing::Invoke(pSysfsAccess.get(), &Mock<PowerSysfsAccess>::getValString));
        ON_CALL(*pSysfsAccess.get(), read(_, Matcher<uint64_t &>(_)))
            .WillByDefault(::testing::Invoke(pSysfsAccess.get(), &Mock<PowerSysfsAccess>::getValUnsignedLong));
        ON_CALL(*pSysfsAccess.get(), write(_, _))
            .WillByDefault(::testing::Invoke(pSysfsAccess.get(), &Mock<PowerSysfsAccess>::setVal));
        ON_CALL(*pSysfsAccess.get(), scanDirEntries(_, _))
            .WillByDefault(::testing::Invoke(pSysfsAccess.get(), &Mock<PowerSysfsAccess>::getscanDirEntries));

        pSysmanDeviceImp->pPowerHandleContext->init();
    }

    void TearDown() override {
        SysmanDeviceFixture::TearDown();
        pLinuxSysmanImp->pSysfsAccess = pSysfsAccessOld;
    }

    std::vector<zes_pwr_handle_t> get_power_handles(uint32_t count) {
        std::vector<zes_pwr_handle_t> handles(count, nullptr);
        EXPECT_EQ(zesDeviceEnumPowerDomains(device->toHandle(), &count, handles.data()), ZE_RESULT_SUCCESS);
        return handles;
    }
};

TEST_F(SysmanDevicePowerFixture, GivenComponentCountZeroWhenEnumeratingPowerDomainsThenValidCountIsReturnedAndVerifySysmanPowerGetCallSucceeds) {
    uint32_t count = 0;
    EXPECT_EQ(zesDeviceEnumPowerDomains(device->toHandle(), &count, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(count, powerHandleComponentCount);
}

TEST_F(SysmanDevicePowerFixture, GivenInvalidComponentCountWhenEnumeratingPowerDomainsThenValidCountIsReturnedAndVerifySysmanPowerGetCallSucceeds) {
    uint32_t count = 0;
    EXPECT_EQ(zesDeviceEnumPowerDomains(device->toHandle(), &count, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(count, powerHandleComponentCount);

    count = count + 1;
    EXPECT_EQ(zesDeviceEnumPowerDomains(device->toHandle(), &count, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(count, powerHandleComponentCount);
}

TEST_F(SysmanDevicePowerFixture, GivenComponentCountZeroWhenEnumeratingPowerDomainsThenValidPowerHandlesIsReturned) {
    uint32_t count = 0;
    EXPECT_EQ(zesDeviceEnumPowerDomains(device->toHandle(), &count, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(count, powerHandleComponentCount);

    std::vector<zes_pwr_handle_t> handles(count, nullptr);
    EXPECT_EQ(zesDeviceEnumPowerDomains(device->toHandle(), &count, handles.data()), ZE_RESULT_SUCCESS);
    for (auto handle : handles) {
        EXPECT_NE(handle, nullptr);
    }
}

TEST_F(SysmanDevicePowerFixture, GivenValidPowerHandleWhenGettingPowerPropertiesThenCallSucceeds) {
    auto handles = get_power_handles(powerHandleComponentCount);

    for (auto handle : handles) {
        zes_power_properties_t properties;
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesPowerGetProperties(handle, &properties));
        EXPECT_FALSE(properties.onSubdevice);
        EXPECT_EQ(properties.subdeviceId, 0u);
    }
}

TEST_F(SysmanDevicePowerFixture, GivenValidPowerHandleWhenGettingPowerEnergyCounterThenValidPowerReadingsRetrieved) {
    auto handles = get_power_handles(powerHandleComponentCount);

    for (auto handle : handles) {
        zes_power_energy_counter_t energyCounter = {};
        ASSERT_EQ(ZE_RESULT_SUCCESS, zesPowerGetEnergyCounter(handle, &energyCounter));
        EXPECT_EQ(energyCounter.energy, expectedEnergyCounter);
    }
}

TEST_F(SysmanDevicePowerFixture, GivenValidPowerHandleWhenGettingPowerEnergyCounterFailedThenValidErrorCodeReturned) {
    auto handles = get_power_handles(powerHandleComponentCount);

    ON_CALL(*pSysfsAccess.get(), read(_, Matcher<uint64_t &>(_)))
        .WillByDefault(::testing::Invoke(pSysfsAccess.get(), &Mock<PowerSysfsAccess>::getValUnsignedLongReturnErrorForEnergyCounter));
    for (auto handle : handles) {
        zes_power_energy_counter_t energyCounter = {};
        ASSERT_EQ(ZE_RESULT_ERROR_INSUFFICIENT_PERMISSIONS, zesPowerGetEnergyCounter(handle, &energyCounter));
    }
}

TEST_F(SysmanDevicePowerFixture, GivenValidPowerHandleWhenGettingPowerEnergyThresholdThenUnsupportedFeatureErrorIsReturned) {
    zes_energy_threshold_t threshold;
    auto handles = get_power_handles(powerHandleComponentCount);
    for (auto handle : handles) {
        EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesPowerGetEnergyThreshold(handle, &threshold));
    }
}

TEST_F(SysmanDevicePowerFixture, GivenValidPowerHandleWhenSettingPowerEnergyThresholdThenUnsupportedFeatureErrorIsReturned) {
    double threshold = 0;
    auto handles = get_power_handles(powerHandleComponentCount);
    for (auto handle : handles) {
        EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesPowerSetEnergyThreshold(handle, threshold));
    }
}

TEST_F(SysmanDevicePowerFixture, GivenSetPowerLimitsWhenGettingPowerLimitsThenLimitsSetEarlierAreRetrieved) {
    auto handles = get_power_handles(powerHandleComponentCount);
    for (auto handle : handles) {
        zes_power_sustained_limit_t sustainedSet = {};
        zes_power_sustained_limit_t sustainedGet = {};
        sustainedSet.interval = 10;
        sustainedSet.power = 300000;
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesPowerSetLimits(handle, &sustainedSet, nullptr, nullptr));
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesPowerGetLimits(handle, &sustainedGet, nullptr, nullptr));
        EXPECT_EQ(sustainedGet.power, sustainedSet.power);
        EXPECT_EQ(sustainedGet.interval, sustainedSet.interval);

        zes_power_burst_limit_t burstSet = {};
        zes_power_burst_limit_t burstGet = {};
        burstSet.enabled = 1;
        burstSet.power = 375000;
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesPowerSetLimits(handle, nullptr, &burstSet, nullptr));
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesPowerGetLimits(handle, nullptr, &burstGet, nullptr));
        EXPECT_EQ(burstSet.enabled, burstGet.enabled);
        EXPECT_EQ(burstSet.power, burstGet.power);

        burstSet.enabled = 0;
        burstGet.enabled = 0;
        burstGet.power = 0;
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesPowerSetLimits(handle, nullptr, &burstSet, nullptr));
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesPowerGetLimits(handle, nullptr, &burstGet, nullptr));
        EXPECT_EQ(burstSet.enabled, burstGet.enabled);
        EXPECT_EQ(burstGet.power, 0);

        zes_power_peak_limit_t peakGet = {};
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesPowerGetLimits(handle, nullptr, nullptr, &peakGet));
        EXPECT_EQ(peakGet.powerAC, -1);
        EXPECT_EQ(peakGet.powerDC, -1);
    }
}

TEST_F(SysmanDevicePowerFixture, GivenGetPowerLimitsReturnErrorWhenGettingPowerLimitsForBurstPowerLimitThenProperErrorCodesReturned) {
    auto handles = get_power_handles(powerHandleComponentCount);

    ON_CALL(*pSysfsAccess.get(), read(_, Matcher<uint64_t &>(_)))
        .WillByDefault(::testing::Invoke(pSysfsAccess.get(), &Mock<PowerSysfsAccess>::getValUnsignedLongReturnErrorForBurstPowerLimit));

    for (auto handle : handles) {
        zes_power_burst_limit_t burstSet = {};
        zes_power_burst_limit_t burstGet = {};
        burstSet.enabled = 1;
        burstSet.power = 375000;
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesPowerSetLimits(handle, nullptr, &burstSet, nullptr));
        EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesPowerGetLimits(handle, nullptr, &burstGet, nullptr));
    }
}

TEST_F(SysmanDevicePowerFixture, GivenSetPowerLimitsReturnErrorWhenSettingPowerLimitsForBurstPowerLimitThenProperErrorCodesReturned) {
    auto handles = get_power_handles(powerHandleComponentCount);

    ON_CALL(*pSysfsAccess.get(), write(_, _))
        .WillByDefault(::testing::Invoke(pSysfsAccess.get(), &Mock<PowerSysfsAccess>::setValUnsignedLongReturnErrorForBurstPowerLimit));

    for (auto handle : handles) {
        zes_power_burst_limit_t burstSet = {};
        burstSet.enabled = 1;
        EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesPowerSetLimits(handle, nullptr, &burstSet, nullptr));
    }
}

TEST_F(SysmanDevicePowerFixture, GivenSetPowerLimitsReturnErrorWhenSettingPowerLimitsForBurstPowerLimitEnabledThenProperErrorCodesReturned) {
    auto handles = get_power_handles(powerHandleComponentCount);

    ON_CALL(*pSysfsAccess.get(), write(_, _))
        .WillByDefault(::testing::Invoke(pSysfsAccess.get(), &Mock<PowerSysfsAccess>::setValUnsignedLongReturnErrorForBurstPowerLimitEnabled));
    ON_CALL(*pSysfsAccess.get(), read(_, Matcher<uint64_t &>(_)))
        .WillByDefault(::testing::Invoke(pSysfsAccess.get(), &Mock<PowerSysfsAccess>::getValUnsignedLongReturnErrorForBurstPowerLimitEnabled));

    for (auto handle : handles) {
        zes_power_burst_limit_t burstSet = {};
        zes_power_burst_limit_t burstGet = {};
        burstSet.enabled = 1;
        EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesPowerSetLimits(handle, nullptr, &burstSet, nullptr));
        EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesPowerGetLimits(handle, nullptr, &burstGet, nullptr));
    }
}

TEST_F(SysmanDevicePowerFixture, GivenReadingSustainedPowerLimitNodeReturnErrorWhenSetOrGetPowerLimitsForSustainedPowerLimitEnabledThenProperErrorCodesReturned) {
    auto handles = get_power_handles(powerHandleComponentCount);

    ON_CALL(*pSysfsAccess.get(), read(_, Matcher<uint64_t &>(_)))
        .WillByDefault(::testing::Invoke(pSysfsAccess.get(), &Mock<PowerSysfsAccess>::getValUnsignedLongReturnErrorForSustainedPowerLimitEnabled));

    for (auto handle : handles) {
        zes_power_sustained_limit_t sustainedSet = {};
        zes_power_sustained_limit_t sustainedGet = {};

        EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesPowerSetLimits(handle, &sustainedSet, nullptr, nullptr));
        EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesPowerGetLimits(handle, &sustainedGet, nullptr, nullptr));
    }
}

TEST_F(SysmanDevicePowerFixture, GivenReadingSustainedPowerNodeReturnErrorWhenGetPowerLimitsForSustainedPowerThenProperErrorCodesReturned) {
    auto handles = get_power_handles(powerHandleComponentCount);

    ON_CALL(*pSysfsAccess.get(), read(_, Matcher<uint64_t &>(_)))
        .WillByDefault(::testing::Invoke(pSysfsAccess.get(), &Mock<PowerSysfsAccess>::getValUnsignedLongReturnErrorForSustainedPower));

    for (auto handle : handles) {
        zes_power_sustained_limit_t sustainedGet = {};
        EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesPowerGetLimits(handle, &sustainedGet, nullptr, nullptr));
    }
}

TEST_F(SysmanDevicePowerFixture, GivenReadingSustainedPowerIntervalNodeReturnErrorWhenGetPowerLimitsForSustainedPowerThenProperErrorCodesReturned) {
    auto handles = get_power_handles(powerHandleComponentCount);

    ON_CALL(*pSysfsAccess.get(), read(_, Matcher<uint64_t &>(_)))
        .WillByDefault(::testing::Invoke(pSysfsAccess.get(), &Mock<PowerSysfsAccess>::getValUnsignedLongReturnErrorForSustainedPowerInterval));

    for (auto handle : handles) {
        zes_power_sustained_limit_t sustainedGet = {};
        EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesPowerGetLimits(handle, &sustainedGet, nullptr, nullptr));
    }
}

TEST_F(SysmanDevicePowerFixture, GivenwritingSustainedPowerNodeReturnErrorWhenSetPowerLimitsForSustainedPowerThenProperErrorCodesReturned) {
    auto handles = get_power_handles(powerHandleComponentCount);

    ON_CALL(*pSysfsAccess.get(), write(_, _))
        .WillByDefault(::testing::Invoke(pSysfsAccess.get(), &Mock<PowerSysfsAccess>::setValReturnErrorForSustainedPower));

    zes_power_sustained_limit_t sustainedSet = {};
    sustainedSet.interval = 10;
    sustainedSet.power = 300000;
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesPowerSetLimits(handles[0], &sustainedSet, nullptr, nullptr));
}

TEST_F(SysmanDevicePowerFixture, GivenwritingSustainedPowerIntervalNodeReturnErrorWhenSetPowerLimitsForSustainedPowerIntervalThenProperErrorCodesReturned) {
    auto handles = get_power_handles(powerHandleComponentCount);

    ON_CALL(*pSysfsAccess.get(), write(_, _))
        .WillByDefault(::testing::Invoke(pSysfsAccess.get(), &Mock<PowerSysfsAccess>::setValReturnErrorForSustainedPowerInterval));

    zes_power_sustained_limit_t sustainedSet = {};
    sustainedSet.interval = 10;
    sustainedSet.power = 300000;
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesPowerSetLimits(handles[0], &sustainedSet, nullptr, nullptr));
}

TEST_F(SysmanDevicePowerFixture, GivenGetPowerLimitsWhenPowerLimitsAreDisabledThenAllPowerValuesAreIgnored) {
    auto handles = get_power_handles(powerHandleComponentCount);

    ON_CALL(*pSysfsAccess.get(), read(_, Matcher<uint64_t &>(_)))
        .WillByDefault(::testing::Invoke(pSysfsAccess.get(), &Mock<PowerSysfsAccess>::getValUnsignedLongReturnsPowerLimitEnabledAsDisabled));

    zes_power_sustained_limit_t sustainedGet = {};
    zes_power_burst_limit_t burstGet = {};
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesPowerGetLimits(handles[0], &sustainedGet, nullptr, nullptr));
    EXPECT_EQ(sustainedGet.interval, 0);
    EXPECT_EQ(sustainedGet.power, 0);
    EXPECT_EQ(sustainedGet.enabled, 0);
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesPowerGetLimits(handles[0], nullptr, &burstGet, nullptr));
    EXPECT_EQ(burstGet.enabled, 0);
    EXPECT_EQ(burstGet.power, 0);
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesPowerSetLimits(handles[0], &sustainedGet, nullptr, nullptr));
    zes_power_burst_limit_t burstSet = {};
    burstSet.enabled = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesPowerSetLimits(handles[0], nullptr, &burstSet, nullptr));
}

} // namespace ult
} // namespace L0