/*
 * Copyright (C) 2020-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/sysman/test/unit_tests/sources/power/windows/mock_power.h"
#include "level_zero/sysman/test/unit_tests/sources/windows/mock_sysman_fixture.h"

namespace L0 {
namespace Sysman {
namespace ult {

constexpr uint32_t powerHandleComponentCount = 2u;
class SysmanDevicePowerFixture : public SysmanDeviceFixture {

  protected:
    std::unique_ptr<PowerKmdSysManager> pKmdSysManager;
    L0::Sysman::KmdSysManager *pOriginalKmdSysManager = nullptr;

    void SetUp() override {
        SysmanDeviceFixture::SetUp();
    }

    void init(bool allowSetCalls) {
        pKmdSysManager.reset(new PowerKmdSysManager);

        pKmdSysManager->allowSetCalls = allowSetCalls;

        pOriginalKmdSysManager = pWddmSysmanImp->pKmdSysManager;
        pWddmSysmanImp->pKmdSysManager = pKmdSysManager.get();

        for (auto handle : pSysmanDeviceImp->pPowerHandleContext->handleList) {
            delete handle;
        }

        pSysmanDeviceImp->pPowerHandleContext->handleList.clear();
    }
    void TearDown() override {
        pWddmSysmanImp->pKmdSysManager = pOriginalKmdSysManager;
        SysmanDeviceFixture::TearDown();
    }

    std::vector<zes_pwr_handle_t> getPowerHandles(uint32_t count) {
        std::vector<zes_pwr_handle_t> handles(count, nullptr);
        EXPECT_EQ(zesDeviceEnumPowerDomains(pSysmanDevice->toHandle(), &count, handles.data()), ZE_RESULT_SUCCESS);
        return handles;
    }
};

TEST_F(SysmanDevicePowerFixture, GivenComponentCountZeroWhenEnumeratingPowerDomainThenValidCountIsReturnedAndVerifySysmanPowerGetCallSucceeds) {
    init(true);

    uint32_t count = 0;
    EXPECT_EQ(zesDeviceEnumPowerDomains(pSysmanDevice->toHandle(), &count, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(count, powerHandleComponentCount);
}

TEST_F(SysmanDevicePowerFixture, GivenComponentCountZeroWhenEnumeratingPowerDomainWithOnlyOneDomainSupportedThenValidCountIsReturnedAndVerifySysmanPowerGetCallSucceeds) {
    init(true);

    uint32_t count = 0;
    pKmdSysManager->mockPowerDomainCount = 1;
    EXPECT_EQ(zesDeviceEnumPowerDomains(pSysmanDevice->toHandle(), &count, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(count, pKmdSysManager->mockPowerDomainCount);
}

TEST_F(SysmanDevicePowerFixture, GivenComponentCountZeroWhenEnumeratingPowerDomainWithKmdFailureThenZeroCountIsReturnedAndVerifySysmanPowerGetCallSucceeds) {
    init(true);

    uint32_t count = 0;
    pKmdSysManager->mockPowerFailure[KmdSysman::Requests::Power::NumPowerDomains] = 1;
    EXPECT_EQ(zesDeviceEnumPowerDomains(pSysmanDevice->toHandle(), &count, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(count, 0u);
}

TEST_F(SysmanDevicePowerFixture, GivenComponentCountZeroWhenEnumeratingPowerDomainWithUnexpectedReturnStatusFromKmdForPowerLimitsEnabledThenZesDeviceEnumPowerDomainsCallSucceedsAndProperHandleCountIsReturned) {
    init(true);
    uint32_t count = 0;
    pKmdSysManager->mockRequestMultiple = true;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEnumPowerDomains(pSysmanDevice->toHandle(), &count, nullptr));
    EXPECT_EQ(count, pKmdSysManager->mockPowerDomainCount);
}

TEST_F(SysmanDevicePowerFixture, GivenComponentCountZeroWhenEnumeratingPowerDomainWithUnexpectedReturnSizeFromKmdForPowerLimitsEnabledThenZesDeviceEnumPowerDomainsCallSucceedsAndProperHandleCountIsReturned) {
    init(true);
    uint32_t count = 0;
    pKmdSysManager->mockRequestMultiple = true;
    pKmdSysManager->requestMultipleSizeDiff = true;
    pKmdSysManager->mockRequestMultipleResult = ZE_RESULT_SUCCESS;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEnumPowerDomains(pSysmanDevice->toHandle(), &count, nullptr));
    EXPECT_EQ(count, pKmdSysManager->mockPowerDomainCount);
}

TEST_F(SysmanDevicePowerFixture, GivenComponentCountZeroWhenEnumeratingPowerDomainWithPowerLimitsNotEnabledThenZesDeviceEnumPowerDomainsCallSucceedsAndProperHandleCountIsReturned) {
    init(true);

    pKmdSysManager->mockPowerFailure[KmdSysman::Requests::Power::PowerLimit1Enabled] = 1;
    pKmdSysManager->mockPowerFailure[KmdSysman::Requests::Power::PowerLimit2Enabled] = 1;
    pKmdSysManager->mockPowerFailure[KmdSysman::Requests::Power::PowerLimit4Enabled] = 1;

    uint32_t count = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEnumPowerDomains(pSysmanDevice->toHandle(), &count, nullptr));
    EXPECT_EQ(powerHandleComponentCount, count);
}

TEST_F(SysmanDevicePowerFixture, GivenComponentCountZeroWhenEnumeratingPowerDomainWithNoPowerLimitsSupportAvailableThenZesDeviceEnumPowerDomainsCallSucceedsAndProperHandleCountIsReturned) {
    init(true);

    pKmdSysManager->mockPowerLimit1Enabled = 0;
    pKmdSysManager->mockPowerLimit2Enabled = 0;
    pKmdSysManager->mockPowerLimit4Enabled = 0;

    uint32_t count = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEnumPowerDomains(pSysmanDevice->toHandle(), &count, nullptr));
    EXPECT_EQ(powerHandleComponentCount, count);
}

TEST_F(SysmanDevicePowerFixture, GivenComponentCountZeroWhenEnumeratingPowerDomainWithUnexpectedValueFromKmdForDomainsSupportedThenValidCountIsReturnedAndVerifySysmanPowerGetCallSucceeds) {
    init(true);
    uint32_t count = 0;
    pKmdSysManager->mockPowerDomainCount = 3;
    EXPECT_EQ(zesDeviceEnumPowerDomains(pSysmanDevice->toHandle(), &count, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(count, 0u);
}

TEST_F(SysmanDevicePowerFixture, GivenPowerDomainsAreEnumeratedWhenCallingIsPowerInitCompletedThenVerifyPowerInitializationIsCompleted) {
    init(true);

    uint32_t count = 0;
    EXPECT_EQ(zesDeviceEnumPowerDomains(pSysmanDevice->toHandle(), &count, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(count, powerHandleComponentCount);

    EXPECT_EQ(true, pSysmanDeviceImp->pPowerHandleContext->isPowerInitDone());
}

TEST_F(SysmanDevicePowerFixture, GivenInvalidComponentCountWhenEnumeratingPowerDomainThenValidCountIsReturnedAndVerifySysmanPowerGetCallSucceeds) {
    init(true);

    uint32_t count = 0;
    EXPECT_EQ(zesDeviceEnumPowerDomains(pSysmanDevice->toHandle(), &count, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(count, powerHandleComponentCount);

    count = count + 1;
    EXPECT_EQ(zesDeviceEnumPowerDomains(pSysmanDevice->toHandle(), &count, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(count, powerHandleComponentCount);
}

TEST_F(SysmanDevicePowerFixture, GivenComponentCountZeroWhenEnumeratingPowerDomainThenValidPowerHandlesIsReturned) {
    init(true);

    uint32_t count = 0;
    EXPECT_EQ(zesDeviceEnumPowerDomains(pSysmanDevice->toHandle(), &count, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(count, powerHandleComponentCount);

    std::vector<zes_pwr_handle_t> handles(count, nullptr);
    EXPECT_EQ(zesDeviceEnumPowerDomains(pSysmanDevice->toHandle(), &count, handles.data()), ZE_RESULT_SUCCESS);
    for (auto handle : handles) {
        EXPECT_NE(handle, nullptr);
    }
}

TEST_F(SysmanDevicePowerFixture, GivenValidPowerHandleWhenGettingPowerPropertiesAndExtPropertiesThenCallSucceeds) {
    // Setting allow set calls or not
    init(true);

    auto handles = getPowerHandles(powerHandleComponentCount);

    for (auto handle : handles) {

        zes_power_properties_t properties = {};
        zes_power_ext_properties_t extProperties = {};
        zes_power_limit_ext_desc_t defaultLimit = {};

        extProperties.defaultLimit = &defaultLimit;
        extProperties.stype = ZES_STRUCTURE_TYPE_POWER_EXT_PROPERTIES;
        properties.pNext = &extProperties;

        ze_result_t result = zesPowerGetProperties(handle, &properties);

        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        EXPECT_FALSE(properties.onSubdevice);
        EXPECT_EQ(properties.subdeviceId, 0u);
        EXPECT_TRUE(properties.canControl);
        EXPECT_TRUE(properties.isEnergyThresholdSupported);
        EXPECT_EQ(static_cast<uint32_t>(properties.maxLimit), pKmdSysManager->mockMaxPowerLimit);
        EXPECT_EQ(static_cast<uint32_t>(properties.minLimit), pKmdSysManager->mockMinPowerLimit);
        EXPECT_EQ(static_cast<uint32_t>(properties.defaultLimit), pKmdSysManager->mockTpdDefault);
        EXPECT_TRUE(defaultLimit.limitValueLocked);
        EXPECT_TRUE(defaultLimit.enabledStateLocked);
        EXPECT_EQ(ZES_POWER_SOURCE_ANY, defaultLimit.source);
        EXPECT_EQ(ZES_LIMIT_UNIT_POWER, defaultLimit.limitUnit);
        EXPECT_EQ(defaultLimit.limit, static_cast<int32_t>(pKmdSysManager->mockTpdDefault));
    }
}

TEST_F(SysmanDevicePowerFixture, GivenValidPowerHandleWhenGettingPowerPropertiesAndExtPropertiesWithUnsetDefaultLimitThenCallSucceeds) {
    // Setting allow set calls or not
    init(true);

    auto handles = getPowerHandles(powerHandleComponentCount);

    for (auto handle : handles) {

        zes_power_properties_t properties = {};
        zes_power_ext_properties_t extProperties = {};

        extProperties.stype = ZES_STRUCTURE_TYPE_POWER_EXT_PROPERTIES;
        properties.pNext = &extProperties;

        ze_result_t result = zesPowerGetProperties(handle, &properties);

        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        EXPECT_FALSE(properties.onSubdevice);
        EXPECT_EQ(properties.subdeviceId, 0u);
        EXPECT_TRUE(properties.canControl);
        EXPECT_TRUE(properties.isEnergyThresholdSupported);
        EXPECT_EQ(static_cast<uint32_t>(properties.maxLimit), pKmdSysManager->mockMaxPowerLimit);
        EXPECT_EQ(static_cast<uint32_t>(properties.minLimit), pKmdSysManager->mockMinPowerLimit);
        EXPECT_EQ(static_cast<uint32_t>(properties.defaultLimit), pKmdSysManager->mockTpdDefault);
    }
}

TEST_F(SysmanDevicePowerFixture, GivenValidPowerHandleWhenGettingPowerPropertiesAndDefaultPowerIsNotAvailableThenCallSucceedsAndDefaultLimitIsInvalid) {
    // Setting allow set calls or not
    init(true);

    auto handles = getPowerHandles(powerHandleComponentCount);

    for (auto handle : handles) {
        zes_power_properties_t properties = {};
        zes_power_ext_properties_t extProperties = {};
        zes_power_limit_ext_desc_t defaultLimit = {};

        extProperties.defaultLimit = &defaultLimit;
        extProperties.stype = ZES_STRUCTURE_TYPE_POWER_EXT_PROPERTIES;
        properties.pNext = &extProperties;

        pKmdSysManager->mockPowerFailure[KmdSysman::Requests::Power::TdpDefault] = 1;
        ze_result_t result = zesPowerGetProperties(handle, &properties);
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        EXPECT_EQ(defaultLimit.limit, -1);
    }
}

TEST_F(SysmanDevicePowerFixture, DISABLED_GivenValidPowerHandleWhenGettingPowerPropertiesAllowSetToFalseThenCallSucceeds) {
    // Setting allow set calls or not
    init(false);

    auto handles = getPowerHandles(powerHandleComponentCount);

    for (auto handle : handles) {
        zes_power_properties_t properties;

        ze_result_t result = zesPowerGetProperties(handle, &properties);

        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        EXPECT_FALSE(properties.onSubdevice);
        EXPECT_EQ(properties.subdeviceId, 0u);
        EXPECT_FALSE(properties.canControl);
        EXPECT_FALSE(properties.isEnergyThresholdSupported);
        EXPECT_EQ(static_cast<uint32_t>(properties.maxLimit), pKmdSysManager->mockMaxPowerLimit);
        EXPECT_EQ(static_cast<uint32_t>(properties.minLimit), pKmdSysManager->mockMinPowerLimit);
        EXPECT_EQ(static_cast<uint32_t>(properties.defaultLimit), pKmdSysManager->mockTpdDefault);
    }
}

TEST_F(SysmanDevicePowerFixture, GivenValidPowerHandleWhenGettingPowerPropertiesAndExtPropertiesAndRequestMultipleFailesThenCallFails) {
    // Setting allow set calls or not
    init(true);

    auto handles = getPowerHandles(powerHandleComponentCount);
    for (auto handle : handles) {
        zes_power_properties_t properties = {};
        zes_power_ext_properties_t extProperties = {};
        extProperties.stype = ZES_STRUCTURE_TYPE_POWER_EXT_PROPERTIES;
        properties.pNext = &extProperties;

        // Mock failure of individual requests in the request multiple call
        std::vector<uint32_t> requestId = {KmdSysman::Requests::Power::EnergyThresholdSupported, KmdSysman::Requests::Power::TdpDefault, KmdSysman::Requests::Power::MinPowerLimitDefault, KmdSysman::Requests::Power::MaxPowerLimitDefault};
        for (auto it = requestId.begin(); it != requestId.end(); it++) {
            pKmdSysManager->mockPowerFailure[*it] = 1;
            EXPECT_EQ(ZE_RESULT_SUCCESS, zesPowerGetProperties(handle, &properties));
            pKmdSysManager->mockPowerFailure[*it] = 0;
        }

        pKmdSysManager->mockRequestMultiple = true;
        EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, zesPowerGetProperties(handle, &properties));
        pKmdSysManager->requestMultipleSizeDiff = true;
        pKmdSysManager->mockRequestMultipleResult = ZE_RESULT_SUCCESS;
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesPowerGetProperties(handle, &properties));
        pKmdSysManager->mockRequestMultiple = false;
        pKmdSysManager->requestMultipleSizeDiff = false;
        pKmdSysManager->mockRequestMultipleResult = ZE_RESULT_ERROR_NOT_AVAILABLE;
    }
}

HWTEST2_F(SysmanDevicePowerFixture, GivenValidPowerHandleWhenGettingPowerEnergyCounterThenValidPowerReadingsRetrieved, IsAtMostDg2) {
    // Setting allow set calls or not
    init(true);

    auto handles = getPowerHandles(powerHandleComponentCount);

    for (auto handle : handles) {
        zes_power_energy_counter_t energyCounter;

        ze_result_t result = zesPowerGetEnergyCounter(handle, &energyCounter);

        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        EXPECT_EQ(energyCounter.energy, pKmdSysManager->mockEnergyCounter64Bit);
        EXPECT_GT(energyCounter.timestamp, 0u);
    }
}

HWTEST2_F(SysmanDevicePowerFixture, GivenValidPowerHandleWhenGettingPowerEnergyCounterAndKmdCallFailsThenCallFails, IsAtMostDg2) {
    // Setting allow set calls or not
    init(true);

    auto handles = getPowerHandles(powerHandleComponentCount);
    for (auto handle : handles) {
        pKmdSysManager->mockPowerFailure[KmdSysman::Requests::Power::CurrentEnergyCounter64Bit] = 1;
        zes_power_energy_counter_t energyCounter;
        ze_result_t result = zesPowerGetEnergyCounter(handle, &energyCounter);
        EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, result);
    }
}

TEST_F(SysmanDevicePowerFixture, GivenValidPowerHandleWhenGettingPowerLimitsAllowSetToFalseThenCallSucceedsWithValidPowerReadingsRetrieved) {
    // Setting allow set calls or not
    init(false);

    auto handles = getPowerHandles(powerHandleComponentCount);

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

    auto handles = getPowerHandles(powerHandleComponentCount);

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

    auto handles = getPowerHandles(powerHandleComponentCount);

    for (auto handle : handles) {
        double energyThreshold = 2000;

        ze_result_t result = zesPowerSetEnergyThreshold(handle, energyThreshold);

        EXPECT_NE(ZE_RESULT_SUCCESS, result);
    }
}

TEST_F(SysmanDevicePowerFixture, GivenValidPowerHandleWhenSettingEnergyThresholdAllowSetToTrueThenCallSucceeds) {
    // Setting allow set calls or not
    init(true);

    auto handles = getPowerHandles(powerHandleComponentCount);

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

    auto handles = getPowerHandles(powerHandleComponentCount);

    for (auto handle : handles) {
        zes_power_sustained_limit_t sustained;
        zes_power_burst_limit_t burst;
        zes_power_peak_limit_t peak;

        uint32_t powerIncrement = 1500;
        uint32_t timeIncrement = 12000;
        uint32_t acPeakPower = 56000;
        uint32_t dcPeakPower = 44100;

        ze_result_t result = zesPowerGetLimits(handle, &sustained, &burst, &peak);

        EXPECT_EQ(ZE_RESULT_SUCCESS, result);

        sustained.power += powerIncrement;
        sustained.interval += timeIncrement;
        burst.power += powerIncrement;
        peak.powerAC = acPeakPower;
        peak.powerDC = dcPeakPower;

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

TEST_F(SysmanDevicePowerFixture, GivenValidPowerHandlesWhenCallingSetAndGetPowerLimitExtThenCallSucceeds) {
    // Setting allow set calls or not
    init(true);
    auto handles = getPowerHandles(powerHandleComponentCount);
    for (auto handle : handles) {

        uint32_t limitCount = 0;
        const int32_t testLimit = 3000000;
        const int32_t testInterval = 10;

        EXPECT_EQ(ZE_RESULT_SUCCESS, zesPowerGetLimitsExt(handle, &limitCount, nullptr));
        EXPECT_EQ(limitCount, mockLimitCount);

        std::vector<zes_power_limit_ext_desc_t> allLimits(limitCount);
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesPowerGetLimitsExt(handle, &limitCount, allLimits.data()));
        for (uint32_t i = 0; i < limitCount; i++) {
            if (allLimits[i].level == ZES_POWER_LEVEL_SUSTAINED) {
                EXPECT_FALSE(allLimits[i].limitValueLocked);
                EXPECT_TRUE(allLimits[i].enabledStateLocked);
                EXPECT_FALSE(allLimits[i].intervalValueLocked);
                EXPECT_EQ(ZES_POWER_SOURCE_ANY, allLimits[i].source);
                EXPECT_EQ(ZES_LIMIT_UNIT_POWER, allLimits[i].limitUnit);
                allLimits[i].limit = testLimit;
                allLimits[i].interval = testInterval;
            } else if (allLimits[i].level == ZES_POWER_LEVEL_PEAK) {
                EXPECT_FALSE(allLimits[i].limitValueLocked);
                EXPECT_TRUE(allLimits[i].enabledStateLocked);
                EXPECT_TRUE(allLimits[i].intervalValueLocked);
                EXPECT_EQ(ZES_LIMIT_UNIT_POWER, allLimits[i].limitUnit);
                allLimits[i].limit = testLimit;
                if (allLimits[i].source == ZES_POWER_SOURCE_MAINS) {
                    EXPECT_EQ(ZES_POWER_SOURCE_MAINS, allLimits[i].source);
                } else {
                    EXPECT_EQ(ZES_POWER_SOURCE_BATTERY, allLimits[i].source);
                }
            } else if (allLimits[i].level == ZES_POWER_LEVEL_BURST) {
                EXPECT_FALSE(allLimits[i].limitValueLocked);
                EXPECT_TRUE(allLimits[i].enabledStateLocked);
                EXPECT_TRUE(allLimits[i].intervalValueLocked);
                EXPECT_EQ(ZES_POWER_SOURCE_ANY, allLimits[i].source);
                EXPECT_EQ(ZES_LIMIT_UNIT_POWER, allLimits[i].limitUnit);
                allLimits[i].limit = testLimit;
            }
        }
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesPowerSetLimitsExt(handle, &limitCount, allLimits.data()));
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesPowerGetLimitsExt(handle, &limitCount, allLimits.data()));
        for (uint32_t i = 0; i < limitCount; i++) {
            if (allLimits[i].level == ZES_POWER_LEVEL_SUSTAINED) {
                EXPECT_EQ(testInterval, allLimits[i].interval);
            } else if (allLimits[i].level == ZES_POWER_LEVEL_PEAK || allLimits[i].level == ZES_POWER_LEVEL_BURST) {
                EXPECT_EQ(0, allLimits[i].interval);
            }
            EXPECT_EQ(testLimit, allLimits[i].limit);
        }
    }
}

TEST_F(SysmanDevicePowerFixture, GivenValidPowerHandleWhenCallingGetAndSetPowerLimitsExtThenProperValuesAreReturnedCoveringMutlipleBranches) {
    // Setting allow set calls or not
    init(true);
    auto handles = getPowerHandles(powerHandleComponentCount);
    for (auto handle : handles) {

        uint32_t limitCount = 0;

        EXPECT_EQ(ZE_RESULT_SUCCESS, zesPowerGetLimitsExt(handle, &limitCount, nullptr));
        EXPECT_EQ(limitCount, mockLimitCount);

        std::vector<zes_power_limit_ext_desc_t> allLimits(limitCount);
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesPowerGetLimitsExt(handle, &limitCount, allLimits.data()));

        std::vector<uint32_t> requestId = {KmdSysman::Requests::Power::PowerLimit1Enabled, KmdSysman::Requests::Power::CurrentPowerLimit1, KmdSysman::Requests::Power::CurrentPowerLimit1Tau, KmdSysman::Requests::Power::PowerLimit2Enabled, KmdSysman::Requests::Power::CurrentPowerLimit2, KmdSysman::Requests::Power::PowerLimit4Enabled, KmdSysman::Requests::Power::CurrentPowerLimit4Ac, KmdSysman::Requests::Power::CurrentPowerLimit4Dc};
        for (auto it = requestId.begin(); it != requestId.end(); it++) {
            pKmdSysManager->mockPowerFailure[*it] = 1;
            uint32_t count = limitCount;
            EXPECT_EQ(ZE_RESULT_SUCCESS, zesPowerGetLimitsExt(handle, &count, allLimits.data()));
            pKmdSysManager->mockPowerFailure[*it] = 0;
        }

        EXPECT_EQ(ZE_RESULT_SUCCESS, zesPowerGetLimitsExt(handle, &limitCount, allLimits.data()));
        requestId = {KmdSysman::Requests::Power::CurrentPowerLimit1, KmdSysman::Requests::Power::CurrentPowerLimit1Tau, KmdSysman::Requests::Power::CurrentPowerLimit2, KmdSysman::Requests::Power::CurrentPowerLimit4Ac, KmdSysman::Requests::Power::CurrentPowerLimit4Dc};
        for (auto it = requestId.begin(); it != requestId.end(); it++) {
            pKmdSysManager->mockPowerFailure[*it] = 1;
            EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, zesPowerSetLimitsExt(handle, &limitCount, allLimits.data()));
            pKmdSysManager->mockPowerFailure[*it] = 0;
        }

        uint32_t count = mockLimitCount;
        pKmdSysManager->mockRequestMultiple = true;
        EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, zesPowerGetLimitsExt(handle, &count, allLimits.data()));
        pKmdSysManager->requestMultipleSizeDiff = true;
        count = mockLimitCount;
        EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, zesPowerGetLimitsExt(handle, &count, allLimits.data()));
        pKmdSysManager->mockRequestMultiple = false;
        pKmdSysManager->requestMultipleSizeDiff = false;

        pKmdSysManager->mockPowerLimit2Enabled = 0;
        count = mockLimitCount;
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesPowerGetLimitsExt(handle, &count, allLimits.data()));
        pKmdSysManager->mockPowerLimit2Enabled = 1;

        pKmdSysManager->mockPowerLimit1Enabled = 0;
        count = mockLimitCount;
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesPowerGetLimitsExt(handle, &count, allLimits.data()));
        pKmdSysManager->mockPowerLimit1Enabled = 1;

        pKmdSysManager->mockPowerLimit4Enabled = 0;
        count = mockLimitCount;
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesPowerGetLimitsExt(handle, &count, allLimits.data()));
        pKmdSysManager->mockPowerLimit4Enabled = 1;

        allLimits[0].level = ZES_POWER_LEVEL_UNKNOWN;
        count = mockLimitCount;
        EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesPowerSetLimitsExt(handle, &count, allLimits.data()));
    }
}

TEST_F(SysmanDevicePowerFixture, GivenValidPowerHandleWhenCallingGetPowerLimitsExtThenProperValuesAreReturned) {
    // Setting allow set calls or not
    init(false);
    auto handles = getPowerHandles(powerHandleComponentCount);

    for (auto handle : handles) {
        zes_power_limit_ext_desc_t allLimits{};
        uint32_t count = 0;

        EXPECT_EQ(ZE_RESULT_SUCCESS, zesPowerGetLimitsExt(handle, &count, nullptr));
        EXPECT_EQ(count, mockLimitCount);

        count = 1;
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesPowerGetLimitsExt(handle, &count, &allLimits));
        EXPECT_EQ(count, 1u);
        EXPECT_EQ(false, allLimits.limitValueLocked);
        EXPECT_EQ(true, allLimits.enabledStateLocked);
        EXPECT_EQ(false, allLimits.intervalValueLocked);
        EXPECT_EQ(ZES_POWER_SOURCE_ANY, allLimits.source);
        EXPECT_EQ(ZES_LIMIT_UNIT_POWER, allLimits.limitUnit);
        EXPECT_EQ(ZES_POWER_LEVEL_SUSTAINED, allLimits.level);
    }
}

} // namespace ult
} // namespace Sysman
} // namespace L0
