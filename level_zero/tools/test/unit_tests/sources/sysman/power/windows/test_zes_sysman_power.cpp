/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/sysman/power/windows/os_power_imp.h"
#include "level_zero/tools/test/unit_tests/sources/sysman/power/windows/mock_power.h"
#include "level_zero/tools/test/unit_tests/sources/sysman/windows/mock_sysman_fixture.h"

extern bool sysmanUltsEnable;

namespace L0 {
namespace ult {

constexpr uint32_t powerHandleComponentCount = 1u;
class SysmanDevicePowerFixture : public SysmanDeviceFixture {

  protected:
    std::unique_ptr<Mock<PowerKmdSysManager>> pKmdSysManager;
    KmdSysManager *pOriginalKmdSysManager = nullptr;
    void SetUp() override {
        if (!sysmanUltsEnable) {
            GTEST_SKIP();
        }
        SysmanDeviceFixture::SetUp();
    }

    void init(bool allowSetCalls) {
        pKmdSysManager.reset(new Mock<PowerKmdSysManager>);

        pKmdSysManager->allowSetCalls = allowSetCalls;

        pOriginalKmdSysManager = pWddmSysmanImp->pKmdSysManager;
        pWddmSysmanImp->pKmdSysManager = pKmdSysManager.get();

        for (auto handle : pSysmanDeviceImp->pPowerHandleContext->handleList) {
            delete handle;
        }

        pSysmanDeviceImp->pPowerHandleContext->handleList.clear();
        uint32_t subDeviceCount = 0;
        std::vector<ze_device_handle_t> deviceHandles;
        Device::fromHandle(device->toHandle())->getSubDevices(&subDeviceCount, nullptr);
        if (subDeviceCount == 0) {
            deviceHandles.resize(1, device->toHandle());
        } else {
            deviceHandles.resize(subDeviceCount, nullptr);
            Device::fromHandle(device->toHandle())->getSubDevices(&subDeviceCount, deviceHandles.data());
        }
    }
    void TearDown() override {
        if (!sysmanUltsEnable) {
            GTEST_SKIP();
        }
        pWddmSysmanImp->pKmdSysManager = pOriginalKmdSysManager;
        SysmanDeviceFixture::TearDown();
    }

    std::vector<zes_pwr_handle_t> get_power_handles(uint32_t count) {
        std::vector<zes_pwr_handle_t> handles(count, nullptr);
        EXPECT_EQ(zesDeviceEnumPowerDomains(device->toHandle(), &count, handles.data()), ZE_RESULT_SUCCESS);
        return handles;
    }
};

TEST_F(SysmanDevicePowerFixture, GivenComponentCountZeroWhenEnumeratingPowerDomainThenValidCountIsReturnedAndVerifySysmanPowerGetCallSucceeds) {
    init(true);

    uint32_t count = 0;
    EXPECT_EQ(zesDeviceEnumPowerDomains(device->toHandle(), &count, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(count, powerHandleComponentCount);
}

TEST_F(SysmanDevicePowerFixture, GivenInvalidComponentCountWhenEnumeratingPowerDomainThenValidCountIsReturnedAndVerifySysmanPowerGetCallSucceeds) {
    init(true);

    uint32_t count = 0;
    EXPECT_EQ(zesDeviceEnumPowerDomains(device->toHandle(), &count, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(count, powerHandleComponentCount);

    count = count + 1;
    EXPECT_EQ(zesDeviceEnumPowerDomains(device->toHandle(), &count, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(count, powerHandleComponentCount);
}

TEST_F(SysmanDevicePowerFixture, GivenComponentCountZeroWhenEnumeratingPowerDomainThenValidPowerHandlesIsReturned) {
    init(true);

    uint32_t count = 0;
    EXPECT_EQ(zesDeviceEnumPowerDomains(device->toHandle(), &count, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(count, powerHandleComponentCount);

    std::vector<zes_pwr_handle_t> handles(count, nullptr);
    EXPECT_EQ(zesDeviceEnumPowerDomains(device->toHandle(), &count, handles.data()), ZE_RESULT_SUCCESS);
    for (auto handle : handles) {
        EXPECT_NE(handle, nullptr);
    }
}

TEST_F(SysmanDevicePowerFixture, DISABLED_GivenValidPowerHandleWhenGettingPowerPropertiesAllowSetToTrueThenCallSucceeds) {
    // Setting allow set calls or not
    init(true);

    auto handles = get_power_handles(powerHandleComponentCount);

    for (auto handle : handles) {
        zes_power_properties_t properties;

        ze_result_t result = zesPowerGetProperties(handle, &properties);

        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        EXPECT_FALSE(properties.onSubdevice);
        EXPECT_EQ(properties.subdeviceId, 0);
        EXPECT_TRUE(properties.canControl);
        EXPECT_TRUE(properties.isEnergyThresholdSupported);
        EXPECT_EQ(properties.maxLimit, pKmdSysManager->mockMaxPowerLimit);
        EXPECT_EQ(properties.minLimit, pKmdSysManager->mockMinPowerLimit);
        EXPECT_EQ(properties.defaultLimit, pKmdSysManager->mockTpdDefault);
    }
}

TEST_F(SysmanDevicePowerFixture, DISABLED_GivenValidPowerHandleWhenGettingPowerPropertiesAllowSetToFalseThenCallSucceeds) {
    // Setting allow set calls or not
    init(false);

    auto handles = get_power_handles(powerHandleComponentCount);

    for (auto handle : handles) {
        zes_power_properties_t properties;

        ze_result_t result = zesPowerGetProperties(handle, &properties);

        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        EXPECT_FALSE(properties.onSubdevice);
        EXPECT_EQ(properties.subdeviceId, 0);
        EXPECT_FALSE(properties.canControl);
        EXPECT_FALSE(properties.isEnergyThresholdSupported);
        EXPECT_EQ(properties.maxLimit, pKmdSysManager->mockMaxPowerLimit);
        EXPECT_EQ(properties.minLimit, pKmdSysManager->mockMinPowerLimit);
        EXPECT_EQ(properties.defaultLimit, pKmdSysManager->mockTpdDefault);
    }
}

TEST_F(SysmanDevicePowerFixture, GivenValidPowerHandleWhenGettingPowerEnergyCounterThenValidPowerReadingsRetrieved) {
    // Setting allow set calls or not
    init(true);

    auto handles = get_power_handles(powerHandleComponentCount);

    for (auto handle : handles) {
        zes_power_energy_counter_t energyCounter;

        ze_result_t result = zesPowerGetEnergyCounter(handle, &energyCounter);

        uint32_t conversionUnit = (1 << pKmdSysManager->mockEnergyUnit);
        double valueConverted = static_cast<double>(pKmdSysManager->mockEnergyCounter) / static_cast<double>(conversionUnit);
        valueConverted *= static_cast<double>(convertJouleToMicroJoule);
        uint64_t mockEnergytoMicroJoules = static_cast<uint64_t>(valueConverted);
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        EXPECT_EQ(energyCounter.energy, mockEnergytoMicroJoules);
        EXPECT_EQ(energyCounter.timestamp, convertTStoMicroSec(pKmdSysManager->mockTimeStamp, pKmdSysManager->mockFrequencyTimeStamp));
    }
}

TEST_F(SysmanDevicePowerFixture, GivenValidPowerHandleWhenGettingPowerLimitsAllowSetToFalseThenCallSucceedsWithValidPowerReadingsRetrieved) {
    // Setting allow set calls or not
    init(false);

    auto handles = get_power_handles(powerHandleComponentCount);

    for (auto handle : handles) {
        zes_power_sustained_limit_t sustained;
        zes_power_burst_limit_t burst;
        zes_power_peak_limit_t peak;

        ze_result_t result = zesPowerGetLimits(handle, &sustained, &burst, &peak);

        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        EXPECT_TRUE(sustained.enabled);
        EXPECT_EQ(sustained.power, pKmdSysManager->mockPowerLimit1);
        EXPECT_EQ(sustained.interval, pKmdSysManager->mockTauPowerLimit1);
        EXPECT_TRUE(burst.enabled);
        EXPECT_EQ(burst.power, pKmdSysManager->mockPowerLimit2);
        EXPECT_EQ(peak.powerAC, pKmdSysManager->mockAcPowerPeak);
        EXPECT_EQ(peak.powerDC, pKmdSysManager->mockDcPowerPeak);
    }
}

TEST_F(SysmanDevicePowerFixture, GivenValidPowerHandleWhenSettingPowerLimitsAllowSetToFalseThenCallFails) {
    // Setting allow set calls or not
    init(false);

    auto handles = get_power_handles(powerHandleComponentCount);

    for (auto handle : handles) {
        zes_power_sustained_limit_t sustained;
        zes_power_burst_limit_t burst;
        zes_power_peak_limit_t peak;

        ze_result_t result = zesPowerGetLimits(handle, &sustained, &burst, &peak);

        EXPECT_EQ(ZE_RESULT_SUCCESS, result);

        sustained.power += 1000;

        result = zesPowerSetLimits(handle, &sustained, &burst, &peak);

        EXPECT_NE(ZE_RESULT_SUCCESS, result);
    }
}

TEST_F(SysmanDevicePowerFixture, GivenValidPowerHandleWhenSettingEnergyThresholdAllowSetToFalseThenCallFails) {
    // Setting allow set calls or not
    init(false);

    auto handles = get_power_handles(powerHandleComponentCount);

    for (auto handle : handles) {
        double energyThreshold = 2000;

        ze_result_t result = zesPowerSetEnergyThreshold(handle, energyThreshold);

        EXPECT_NE(ZE_RESULT_SUCCESS, result);
    }
}

TEST_F(SysmanDevicePowerFixture, GivenValidPowerHandleWhenSettingEnergyThresholdAllowSetToTrueThenCallSucceeds) {
    // Setting allow set calls or not
    init(true);

    auto handles = get_power_handles(powerHandleComponentCount);

    for (auto handle : handles) {
        double energyThreshold = 2000;

        ze_result_t result = zesPowerSetEnergyThreshold(handle, energyThreshold);

        EXPECT_EQ(ZE_RESULT_SUCCESS, result);

        zes_energy_threshold_t newEnergyThreshold;
        result = zesPowerGetEnergyThreshold(handle, &newEnergyThreshold);

        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        EXPECT_EQ(newEnergyThreshold.threshold, energyThreshold);
    }
}

TEST_F(SysmanDevicePowerFixture, GivenValidPowerHandleWhenSettingPowerLimitsAllowSetToTrueThenCallSucceeds) {
    // Setting allow set calls or not
    init(true);

    auto handles = get_power_handles(powerHandleComponentCount);

    for (auto handle : handles) {
        zes_power_sustained_limit_t sustained;
        zes_power_burst_limit_t burst;
        zes_power_peak_limit_t peak;

        uint32_t powerIncrement = 1500;
        uint32_t timeIncrement = 12000;
        uint32_t AcPeakPower = 56000;
        uint32_t DcPeakPower = 44100;

        ze_result_t result = zesPowerGetLimits(handle, &sustained, &burst, &peak);

        EXPECT_EQ(ZE_RESULT_SUCCESS, result);

        sustained.power += powerIncrement;
        sustained.interval += timeIncrement;
        burst.power += powerIncrement;
        peak.powerAC = AcPeakPower;
        peak.powerDC = DcPeakPower;

        result = zesPowerSetLimits(handle, &sustained, &burst, &peak);

        EXPECT_EQ(ZE_RESULT_SUCCESS, result);

        zes_power_sustained_limit_t newSustained;
        zes_power_burst_limit_t newBurst;
        zes_power_peak_limit_t newPeak;

        result = zesPowerGetLimits(handle, &newSustained, &newBurst, &newPeak);

        EXPECT_EQ(ZE_RESULT_SUCCESS, result);

        EXPECT_EQ(newSustained.power, sustained.power);
        EXPECT_EQ(newSustained.interval, sustained.interval);
        EXPECT_EQ(newBurst.power, burst.power);
        EXPECT_EQ(newPeak.powerAC, peak.powerAC);
        EXPECT_EQ(newPeak.powerDC, peak.powerDC);
    }
}

TEST_F(SysmanDevicePowerFixture, GivenValidPowerHandlesWhenCallingSetAndGetPowerLimitExtWhenHwmonInterfaceExistThenUnsupportedFeatureIsReturned) {
    // Setting allow set calls or not
    init(true);

    auto handles = get_power_handles(powerHandleComponentCount);
    for (auto handle : handles) {
        uint32_t limitCount = 0;

        EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesPowerGetLimitsExt(handle, &limitCount, nullptr));
        EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesPowerSetLimitsExt(handle, &limitCount, nullptr));
    }
}

} // namespace ult
} // namespace L0
