/*
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/sysman/test/unit_tests/sources/linux/mocks/mock_sysman_product_helper.h"
#include "level_zero/sysman/test/unit_tests/sources/power/linux/mock_sysfs_power.h"
namespace L0 {
namespace Sysman {
namespace ult {

constexpr uint32_t powerHandleComponentCount = 1u;
using SysmanDevicePowerFixtureHelper = SysmanDevicePowerFixtureI915;

TEST_F(SysmanDevicePowerFixtureHelper, GivenValidPowerHandleWhenGettingPowerEnergyCounterThenValidPowerReadingsRetrieved) {

    auto handles = getPowerHandles(powerHandleComponentCount);
    for (auto handle : handles) {
        zes_power_energy_counter_t energyCounter = {};
        ASSERT_EQ(ZE_RESULT_SUCCESS, zesPowerGetEnergyCounter(handle, &energyCounter));
        EXPECT_EQ(energyCounter.energy, expectedEnergyCounter);
    }
}

constexpr uint32_t powerHandleComponentCountMultiDevice = 3u;
using SysmanDevicePowerMultiDeviceFixtureHelper = SysmanDevicePowerMultiDeviceFixture;

TEST_F(SysmanDevicePowerMultiDeviceFixtureHelper, GivenValidDeviceHandlesAndHwmonInterfaceExistThenSuccessIsReturned) {
    auto subDeviceCount = pLinuxSysmanImp->getSubDeviceCount();
    uint32_t subdeviceId = 0;
    do {
        ze_bool_t onSubdevice = (subDeviceCount == 0) ? false : true;
        PublicLinuxPowerImp *pPowerImp = new PublicLinuxPowerImp(pOsSysman, onSubdevice, subdeviceId, ZES_POWER_DOMAIN_PACKAGE);
        EXPECT_TRUE(pPowerImp->isPowerModuleSupported());
        delete pPowerImp;

    } while (++subdeviceId < subDeviceCount);
}

TEST_F(SysmanDevicePowerMultiDeviceFixtureHelper, GivenInvalidComponentCountWhenEnumeratingPowerDomainsThenValidCountIsReturnedAndVerifySysmanPowerGetCallSucceeds) {
    uint32_t count = 0;
    EXPECT_EQ(zesDeviceEnumPowerDomains(device->toHandle(), &count, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(count, powerHandleComponentCountMultiDevice);

    count = count + 1;
    EXPECT_EQ(zesDeviceEnumPowerDomains(device->toHandle(), &count, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(count, powerHandleComponentCountMultiDevice);
}

TEST_F(SysmanDevicePowerMultiDeviceFixtureHelper, GivenValidPowerPointerWhenGettingCardPowerDomainThenFailureIsReturned) {
    zes_pwr_handle_t phPower = {};
    EXPECT_EQ(zesDeviceGetCardPowerDomain(device->toHandle(), &phPower), ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
}

TEST_F(SysmanDevicePowerMultiDeviceFixtureHelper, GivenValidPowerHandleWhenGettingPowerPropertiesThenCallSucceeds) {
    MockSysmanProductHelper *pMockSysmanProductHelper = new MockSysmanProductHelper();
    pMockSysmanProductHelper->isPowerSetLimitSupportedResult = true;
    std::unique_ptr<SysmanProductHelper> pSysmanProductHelper(static_cast<SysmanProductHelper *>(pMockSysmanProductHelper));
    std::swap(pLinuxSysmanImp->pSysmanProductHelper, pSysmanProductHelper);

    auto handles = getPowerHandles(powerHandleComponentCountMultiDevice);
    for (auto handle : handles) {
        zes_power_properties_t properties = {};
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesPowerGetProperties(handle, &properties));
        if (properties.onSubdevice) {
            EXPECT_FALSE(properties.canControl);
            EXPECT_EQ(properties.defaultLimit, -1);
            EXPECT_EQ(properties.maxLimit, -1);
            EXPECT_EQ(properties.minLimit, -1);
        } else {
            EXPECT_EQ(properties.canControl, true);
            EXPECT_EQ(properties.defaultLimit, (int32_t)(mockDefaultPowerLimitVal / milliFactor));
            EXPECT_EQ(properties.maxLimit, (int32_t)(mockDefaultPowerLimitVal / milliFactor));
            EXPECT_EQ(properties.minLimit, -1);
        }
        EXPECT_EQ(properties.isEnergyThresholdSupported, false);
    }
}

TEST_F(SysmanDevicePowerMultiDeviceFixtureHelper, GivenValidPowerHandleWhenGettingPowerPropertiesAndExtPropertiesThenCallSucceeds) {
    MockSysmanProductHelper *pMockSysmanProductHelper = new MockSysmanProductHelper();
    pMockSysmanProductHelper->isPowerSetLimitSupportedResult = true;
    std::unique_ptr<SysmanProductHelper> pSysmanProductHelper(static_cast<SysmanProductHelper *>(pMockSysmanProductHelper));
    std::swap(pLinuxSysmanImp->pSysmanProductHelper, pSysmanProductHelper);

    auto handles = getPowerHandles(powerHandleComponentCountMultiDevice);
    for (auto handle : handles) {
        ASSERT_NE(nullptr, handle);
        zes_power_properties_t properties = {};
        zes_power_ext_properties_t extProperties = {};
        zes_power_limit_ext_desc_t defaultLimit = {};

        extProperties.defaultLimit = &defaultLimit;
        extProperties.stype = ZES_STRUCTURE_TYPE_POWER_EXT_PROPERTIES;
        properties.pNext = &extProperties;
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesPowerGetProperties(handle, &properties));
        EXPECT_TRUE(defaultLimit.limitValueLocked);
        EXPECT_TRUE(defaultLimit.enabledStateLocked);
        EXPECT_TRUE(defaultLimit.intervalValueLocked);
        EXPECT_EQ(defaultLimit.level, ZES_POWER_LEVEL_UNKNOWN);
        EXPECT_EQ(defaultLimit.source, ZES_POWER_SOURCE_ANY);
        EXPECT_EQ(defaultLimit.limitUnit, ZES_LIMIT_UNIT_POWER);
        EXPECT_EQ(extProperties.domain, ZES_POWER_DOMAIN_PACKAGE);
        if (properties.onSubdevice) {
            EXPECT_FALSE(properties.canControl);
            EXPECT_EQ(defaultLimit.limit, -1);
        } else {
            EXPECT_TRUE(properties.canControl);
            EXPECT_EQ(defaultLimit.limit, static_cast<int32_t>(mockDefaultPowerLimitVal / milliFactor));
            EXPECT_EQ(properties.defaultLimit, (int32_t)(mockDefaultPowerLimitVal / milliFactor));
            EXPECT_EQ(properties.maxLimit, (int32_t)(mockDefaultPowerLimitVal / milliFactor));
            EXPECT_EQ(properties.minLimit, -1);
        }
        EXPECT_EQ(properties.isEnergyThresholdSupported, false);
    }
}

TEST_F(SysmanDevicePowerMultiDeviceFixtureHelper, GivenValidPowerHandleAndExtPropertiesWithNullDescWhenGettingPowerPropertiesThenCallSucceeds) {
    MockSysmanProductHelper *pMockSysmanProductHelper = new MockSysmanProductHelper();
    pMockSysmanProductHelper->isPowerSetLimitSupportedResult = true;
    std::unique_ptr<SysmanProductHelper> pSysmanProductHelper(static_cast<SysmanProductHelper *>(pMockSysmanProductHelper));
    std::swap(pLinuxSysmanImp->pSysmanProductHelper, pSysmanProductHelper);

    auto handles = getPowerHandles(powerHandleComponentCountMultiDevice);
    for (auto handle : handles) {
        ASSERT_NE(nullptr, handle);
        zes_power_properties_t properties = {};
        zes_power_ext_properties_t extProperties = {};

        properties.pNext = &extProperties;
        extProperties.stype = ZES_STRUCTURE_TYPE_POWER_EXT_PROPERTIES;
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesPowerGetProperties(handle, &properties));
        EXPECT_EQ(extProperties.domain, ZES_POWER_DOMAIN_PACKAGE);
        if (properties.onSubdevice) {
            EXPECT_FALSE(properties.canControl);
            EXPECT_EQ(properties.maxLimit, -1);
            EXPECT_EQ(properties.minLimit, -1);
            EXPECT_EQ(properties.defaultLimit, -1);
        } else {
            EXPECT_TRUE(properties.canControl);
            EXPECT_EQ(properties.defaultLimit, (int32_t)(mockDefaultPowerLimitVal / milliFactor));
            EXPECT_EQ(properties.maxLimit, (int32_t)(mockDefaultPowerLimitVal / milliFactor));
            EXPECT_EQ(properties.minLimit, -1);
        }
        EXPECT_EQ(properties.isEnergyThresholdSupported, false);
    }
}

TEST_F(SysmanDevicePowerMultiDeviceFixtureHelper, GivenScanDirectoriesFailAndTelemetrySupportNotAvailableWhenGettingCardPowerThenFailureIsReturned) {

    pSysfsAccess->mockscanDirEntriesResult.push_back(ZE_RESULT_ERROR_NOT_AVAILABLE);

    for (const auto &handle : pSysmanDeviceImp->pPowerHandleContext->handleList) {
        delete handle;
    }
    pSysmanDeviceImp->pPowerHandleContext->handleList.clear();
    pSysmanDeviceImp->pPowerHandleContext->init(pLinuxSysmanImp->getSubDeviceCount());

    zes_pwr_handle_t phPower = {};
    EXPECT_EQ(zesDeviceGetCardPowerDomain(device->toHandle(), &phPower), ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
}

TEST_F(SysmanDevicePowerMultiDeviceFixtureHelper, GivenPowerHandleDomainIsNotCardWhenGettingCardPowerDomainThenErrorIsReturned) {
    uint32_t count = 0;
    EXPECT_EQ(zesDeviceEnumPowerDomains(device->toHandle(), &count, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(count, powerHandleComponentCountMultiDevice);

    auto handle = pSysmanDeviceImp->pPowerHandleContext->handleList.front();
    delete handle;

    pSysmanDeviceImp->pPowerHandleContext->handleList.erase(pSysmanDeviceImp->pPowerHandleContext->handleList.begin());
    zes_pwr_handle_t phPower = {};
    EXPECT_EQ(zesDeviceGetCardPowerDomain(device->toHandle(), &phPower), ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
}

TEST_F(SysmanDevicePowerMultiDeviceFixtureHelper, GivenReadingToSysNodesFailsWhenCallingGetPowerLimitsExtThenPowerLimitCountIsZero) {
    for (const auto &handle : pSysmanDeviceImp->pPowerHandleContext->handleList) {
        delete handle;
    }
    pSysmanDeviceImp->pPowerHandleContext->handleList.clear();
    pSysfsAccess->isSustainedPowerLimitFilePresent = false;
    pSysfsAccess->isCriticalPowerLimitFilePresent = false;
    pSysmanDeviceImp->pPowerHandleContext->init(pLinuxSysmanImp->getSubDeviceCount());

    auto handles = getPowerHandles(powerHandleComponentCountMultiDevice);
    for (auto handle : handles) {
        ASSERT_NE(nullptr, handle);
        uint32_t count = 0;
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesPowerGetLimitsExt(handle, &count, nullptr));
        EXPECT_EQ(count, 0u);
    }
}

TEST_F(SysmanDevicePowerMultiDeviceFixtureHelper, GivenValidPowerHandleWhenGettingPowerEnergyCounterThenValidPowerReadingsRetrieved) {
    auto handles = getPowerHandles(powerHandleComponentCountMultiDevice);

    for (auto handle : handles) {
        ASSERT_NE(nullptr, handle);
        zes_power_properties_t properties = {};
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesPowerGetProperties(handle, &properties));
        zes_power_energy_counter_t energyCounter = {};
        ASSERT_EQ(ZE_RESULT_SUCCESS, zesPowerGetEnergyCounter(handle, &energyCounter));
        if (!properties.onSubdevice) {
            EXPECT_EQ(energyCounter.energy, expectedEnergyCounter);
        } else if (properties.subdeviceId == 0) {
            EXPECT_EQ(energyCounter.energy, expectedEnergyCounterTile0);
        } else if (properties.subdeviceId == 1) {
            EXPECT_EQ(energyCounter.energy, expectedEnergyCounterTile1);
        }
    }
}

} // namespace ult
} // namespace Sysman
} // namespace L0
