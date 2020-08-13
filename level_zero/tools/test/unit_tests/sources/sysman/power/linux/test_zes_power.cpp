/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/sysman/power/linux/os_power_imp.h"
#include "level_zero/tools/test/unit_tests/sources/sysman/linux/mock_sysman_fixture.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "mock_sysfs_power.h"
#include "sysman/power/power_imp.h"

namespace L0 {
namespace ult {

constexpr uint64_t convertJouleToMicroJoule = 1000000u;
constexpr uint32_t powerHandleComponentCount = 1u;
class SysmanDevicePowerFixture : public SysmanDeviceFixture {
  protected:
    std::unique_ptr<Mock<PowerPmt>> pPmt;
    PlatformMonitoringTech *pPmtOld = nullptr;
    void SetUp() override {
        SysmanDeviceFixture::SetUp();
        pPmtOld = pLinuxSysmanImp->pPmt;
        pPmt = std::make_unique<NiceMock<Mock<PowerPmt>>>();
        pPmt->init(deviceName, pFsAccess);
        pLinuxSysmanImp->pPmt = pPmt.get();
        pSysmanDeviceImp->pPowerHandleContext->init();
    }
    void TearDown() override {
        SysmanDeviceFixture::TearDown();
        pLinuxSysmanImp->pPmt = pPmtOld;
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
        zes_power_energy_counter_t energyCounter;
        uint64_t expectedEnergyCounter = convertJouleToMicroJoule * setEnergyCounter;
        ASSERT_EQ(ZE_RESULT_SUCCESS, zesPowerGetEnergyCounter(handle, &energyCounter));
        EXPECT_EQ(energyCounter.energy, expectedEnergyCounter);
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

TEST_F(SysmanDevicePowerFixture, GivenValidPowerHandleWhenGettingPowerLimitsThenUnsupportedFeatureErrorIsReturned) {
    auto handles = get_power_handles(powerHandleComponentCount);
    for (auto handle : handles) {
        zes_power_sustained_limit_t sustained;
        zes_power_burst_limit_t burst;
        zes_power_peak_limit_t peak;
        EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesPowerGetLimits(handle, &sustained, &burst, &peak));
    }
}

TEST_F(SysmanDevicePowerFixture, GivenValidPowerHandleWhenSettingPowerLimitsThenUnsupportedFeatureErrorIsReturned) {
    auto handles = get_power_handles(powerHandleComponentCount);
    for (auto handle : handles) {
        zes_power_sustained_limit_t sustained;
        zes_power_burst_limit_t burst;
        zes_power_peak_limit_t peak;
        EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesPowerSetLimits(handle, &sustained, &burst, &peak));
    }
}

} // namespace ult
} // namespace L0
