/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/sysman/test/unit_tests/sources/linux/mocks/mock_sysman_product_helper.h"
#include "level_zero/sysman/test/unit_tests/sources/power/linux/mock_sysfs_power_xe.h"

namespace L0 {
namespace Sysman {
namespace ult {

static constexpr uint32_t powerHandleComponentCount = 2u;
static constexpr uint32_t singleLimitCount = 1u;
static constexpr uint32_t maxLimitCountSupported = 2u;

TEST_F(SysmanDevicePowerFixtureXe, GivenKmdInterfaceWhenGettingSysFsFilenamesForPowerForXeVersionThenProperPathsAreReturned) {
    auto pSysmanKmdInterface = pLinuxSysmanImp->getSysmanKmdInterface();
    EXPECT_STREQ("energy1_input", pSysmanKmdInterface->getSysfsFilePath(SysfsName::sysfsNameEnergyCounterNode, 0, false).c_str());
    EXPECT_STREQ("power1_rated_max", pSysmanKmdInterface->getSysfsFilePath(SysfsName::sysfsNameDefaultPowerLimit, 0, false).c_str());
    EXPECT_STREQ("power1_max", pSysmanKmdInterface->getSysfsFilePath(SysfsName::sysfsNameSustainedPowerLimit, 0, false).c_str());
    EXPECT_STREQ("power1_max_interval", pSysmanKmdInterface->getSysfsFilePath(SysfsName::sysfsNameSustainedPowerLimitInterval, 0, false).c_str());
}

HWTEST2_F(SysmanDevicePowerFixtureXe, GivenHwmonDirectoriesArePresentAndAtLeastOneOfThePowerLimitFilesExistsThenPowerModuleForCardPowerDomainIsSupported, IsPVC) {
    auto pPowerImp = std::make_unique<PublicLinuxPowerImp>(pOsSysman, false, 0, ZES_POWER_DOMAIN_CARD);

    // Loop through all combinations of the three boolean flags (true/false) for the file existence
    for (bool isCardEnergyCounterFilePresent : {false, true}) {
        for (bool isSustainedPowerLimitFilePresent : {false, true}) {
            for (bool isCriticalPowerLimitFilePresent : {false, true}) {
                // Set the file existence flags based on the current combination
                pSysfsAccess->isCardEnergyCounterFilePresent = isCardEnergyCounterFilePresent;
                pSysfsAccess->isSustainedPowerLimitFilePresent = isSustainedPowerLimitFilePresent;
                pSysfsAccess->isCriticalPowerLimitFilePresent = isCriticalPowerLimitFilePresent;

                // The expected result is true if at least one of the files is present
                bool expected = isCardEnergyCounterFilePresent || isSustainedPowerLimitFilePresent || isCriticalPowerLimitFilePresent;

                // Verify if the power module is supported as expected
                EXPECT_EQ(pPowerImp->isPowerModuleSupported(), expected);
            }
        }
    }
}

HWTEST2_F(SysmanDevicePowerFixtureXe, GivenHwmonDirectoriesArePresentAndAtLeastOneOfThePowerLimitFilesExistsThenPowerModuleForCardPowerDomainIsSupported, IsNotPVC) {
    auto pPowerImp = std::make_unique<PublicLinuxPowerImp>(pOsSysman, false, 0, ZES_POWER_DOMAIN_CARD);

    pSysfsAccess->isCriticalPowerLimitFilePresent = false;
    // Loop through all combinations of the three boolean flags (true/false) for the file existence
    for (bool isCardEnergyCounterFilePresent : {false, true}) {
        for (bool isSustainedPowerLimitFilePresent : {false, true}) {
            // Set the file existence flags based on the current combination
            pSysfsAccess->isCardEnergyCounterFilePresent = isCardEnergyCounterFilePresent;
            pSysfsAccess->isSustainedPowerLimitFilePresent = isSustainedPowerLimitFilePresent;

            // The expected result is true if at least one of the files is present
            bool expected = isCardEnergyCounterFilePresent || isSustainedPowerLimitFilePresent;

            // Verify if the power module is supported as expected
            EXPECT_EQ(pPowerImp->isPowerModuleSupported(), expected);
        }
    }
}

HWTEST2_F(SysmanDevicePowerFixtureXe, GivenHwmonDirectoriesArePresentAndAtLeastOneOfThePowerLimitFilesExistsThenPowerModuleForPackagePowerDomainIsSupported, IsPVC) {
    auto pPowerImp = std::make_unique<PublicLinuxPowerImp>(pOsSysman, false, 0, ZES_POWER_DOMAIN_PACKAGE);

    // Loop through all combinations of the three boolean flags (true/false) for the file existence
    for (bool isPackageEnergyCounterFilePresent : {false, true}) {
        for (bool isPackagedSustainedPowerLimitFilePresent : {false, true}) {
            for (bool isPackageCriticalPowerLimit2Present : {false, true}) {
                // Set the file existence flags based on the current combination
                pSysfsAccess->isPackageEnergyCounterFilePresent = isPackageEnergyCounterFilePresent;
                pSysfsAccess->isPackagedSustainedPowerLimitFilePresent = isPackagedSustainedPowerLimitFilePresent;
                pSysfsAccess->isPackageCriticalPowerLimit2Present = isPackageCriticalPowerLimit2Present;

                // The expected result is true if at least one of the files is present
                bool expected = isPackageEnergyCounterFilePresent || isPackagedSustainedPowerLimitFilePresent || isPackageCriticalPowerLimit2Present;

                // Verify if the power module is supported as expected
                EXPECT_EQ(pPowerImp->isPowerModuleSupported(), expected);
            }
        }
    }
}

HWTEST2_F(SysmanDevicePowerFixtureXe, GivenHwmonDirectoriesArePresentAndAtLeastOneOfThePowerLimitFilesExistsThenPowerModuleForPackagePowerDomainIsSupported, IsNotPVC) {
    auto pPowerImp = std::make_unique<PublicLinuxPowerImp>(pOsSysman, false, 0, ZES_POWER_DOMAIN_PACKAGE);

    // Loop through all combinations of the three boolean flags (true/false) for the file existence
    for (bool isPackageEnergyCounterFilePresent : {false, true}) {
        for (bool isPackagedSustainedPowerLimitFilePresent : {false, true}) {
            for (bool isPackageCriticalPowerLimit1Present : {false, true}) {
                // Set the file existence flags based on the current combination
                pSysfsAccess->isPackageEnergyCounterFilePresent = isPackageEnergyCounterFilePresent;
                pSysfsAccess->isPackagedSustainedPowerLimitFilePresent = isPackagedSustainedPowerLimitFilePresent;
                pSysfsAccess->isPackageCriticalPowerLimit1Present = isPackageCriticalPowerLimit1Present;

                // The expected result is true if at least one of the files is present
                bool expected = isPackageEnergyCounterFilePresent || isPackagedSustainedPowerLimitFilePresent || isPackageCriticalPowerLimit1Present;

                // Verify if the power module is supported as expected
                EXPECT_EQ(pPowerImp->isPowerModuleSupported(), expected);
            }
        }
    }
}

TEST_F(SysmanDevicePowerFixtureXe, GivenComponentCountZeroWhenEnumeratingPowerDomainsWhenhwmonInterfaceExistsThenValidCountIsReturnedAndVerifySysmanPowerGetCallSucceeds) {
    uint32_t count = 0;
    EXPECT_EQ(zesDeviceEnumPowerDomains(device->toHandle(), &count, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(count, powerHandleComponentCount);
}

TEST_F(SysmanDevicePowerFixtureXe, GivenValidPowerHandlesForPackageDomainWhenGetCardPowerDomainIsCalledThenErrorIsReturned) {

    auto handles = getPowerHandles(powerHandleComponentCount);
    pSysmanDeviceImp->pPowerHandleContext->handleList.erase(std::remove_if(pSysmanDeviceImp->pPowerHandleContext->handleList.begin(),
                                                                           pSysmanDeviceImp->pPowerHandleContext->handleList.end(), [](L0::Sysman::Power *powerHandle) {
                                                                               if (powerHandle->isCardPower) {
                                                                                   delete powerHandle;
                                                                                   return true;
                                                                               }
                                                                               return false;
                                                                           }),
                                                            pSysmanDeviceImp->pPowerHandleContext->handleList.end());
    zes_pwr_handle_t phPower = {};
    EXPECT_EQ(zesDeviceGetCardPowerDomain(device->toHandle(), &phPower), ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
}

TEST_F(SysmanDevicePowerFixtureXe, GivenPowerHandleWithPackageDomainThenPowerModuleIsSupported) {
    ze_bool_t onSubdevice = false;
    uint32_t subdeviceId = 0;
    auto pPowerImp = std::make_unique<PublicLinuxPowerImp>(pOsSysman, onSubdevice, subdeviceId, ZES_POWER_DOMAIN_PACKAGE);
    pPowerImp->pSysfsAccess = pSysfsAccess;
    EXPECT_TRUE(pPowerImp->isPowerModuleSupported());
}

TEST_F(SysmanDevicePowerFixtureXe, GivenPowerHandleWithUnknownPowerDomainThenPowerModuleIsNotSupported) {
    ze_bool_t onSubdevice = false;
    uint32_t subdeviceId = 0;
    auto pPowerImp = std::make_unique<PublicLinuxPowerImp>(pOsSysman, onSubdevice, subdeviceId, ZES_POWER_DOMAIN_UNKNOWN);
    pPowerImp->pSysfsAccess = pSysfsAccess;
    EXPECT_FALSE(pPowerImp->isPowerModuleSupported());
}

TEST_F(SysmanDevicePowerFixtureXe, GivenPowerHandleWithPackageDomainAndHwmonExistsAndSustainedPowerLimitFileIsNotPresentThenPowerModuleIsSupported) {
    ze_bool_t onSubdevice = false;
    uint32_t subdeviceId = 0;
    pSysfsAccess->mockReadValUnsignedLongResult.push_back(ZE_RESULT_ERROR_NOT_AVAILABLE);
    auto pPowerImp = std::make_unique<PublicLinuxPowerImp>(pOsSysman, onSubdevice, subdeviceId, ZES_POWER_DOMAIN_PACKAGE);
    pPowerImp->pSysfsAccess = pSysfsAccess;
    EXPECT_TRUE(pPowerImp->isPowerModuleSupported());
}

TEST_F(SysmanDevicePowerFixtureXe, GivenPowerHandleWithPackageDomainAndHwmonExistsAndCriticalPowerLimitFileIsNotPresentThenPowerModuleIsSupported) {
    ze_bool_t onSubdevice = false;
    uint32_t subdeviceId = 0;
    pSysfsAccess->mockReadValUnsignedLongResult.push_back(ZE_RESULT_SUCCESS);
    pSysfsAccess->mockReadValUnsignedLongResult.push_back(ZE_RESULT_ERROR_NOT_AVAILABLE);
    auto pPowerImp = std::make_unique<PublicLinuxPowerImp>(pOsSysman, onSubdevice, subdeviceId, ZES_POWER_DOMAIN_PACKAGE);
    pPowerImp->pSysfsAccess = pSysfsAccess;
    EXPECT_TRUE(pPowerImp->isPowerModuleSupported());
}

TEST_F(SysmanDevicePowerFixtureXe, GivenHwmonDirectoriesDoesNotExistWhenGettingPowerHandlesThenNoHandlesAreReturned) {
    pSysfsAccess->mockScanDirEntriesResult = ZE_RESULT_ERROR_NOT_AVAILABLE;

    uint32_t count = 0;
    EXPECT_EQ(zesDeviceEnumPowerDomains(device->toHandle(), &count, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(count, 0u);
}

TEST_F(SysmanDevicePowerFixtureXe, GivenHwmonDirectoriesDoNotContainNameFileWhenGettingPowerHandlesThenNoHandlesAreReturned) {
    pSysfsAccess->mockReadResult = ZE_RESULT_ERROR_NOT_AVAILABLE;

    uint32_t count = 0;
    EXPECT_EQ(zesDeviceEnumPowerDomains(device->toHandle(), &count, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(count, 0u);
}

TEST_F(SysmanDevicePowerFixtureXe, GivenEnergyCounterNodeIsNotAvailableWhenGettingPowerHandlesThenNoHandlesAreReturned) {
    pSysmanKmdInterface->isEnergyNodeAvailable = false;

    uint32_t count = 0;
    EXPECT_EQ(zesDeviceEnumPowerDomains(device->toHandle(), &count, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(count, 0u);
}

TEST_F(SysmanDevicePowerFixtureXe, GivenValidPowerHandleWhenGettingPowerPropertiesWhenhwmonInterfaceExistsThenCallSucceeds) {
    MockSysmanProductHelper *pMockSysmanProductHelper = new MockSysmanProductHelper();
    pMockSysmanProductHelper->isPowerSetLimitSupportedResult = true;
    std::unique_ptr<SysmanProductHelper> pSysmanProductHelper(static_cast<SysmanProductHelper *>(pMockSysmanProductHelper));
    std::swap(pLinuxSysmanImp->pSysmanProductHelper, pSysmanProductHelper);

    for (const auto &handle : pSysmanDeviceImp->pPowerHandleContext->handleList) {
        delete handle;
    }
    pSysmanDeviceImp->pPowerHandleContext->handleList.clear();
    pSysmanDeviceImp->pPowerHandleContext->init(pLinuxSysmanImp->getSubDeviceCount());
    auto handles = getPowerHandles(powerHandleComponentCount);

    for (auto handle : handles) {
        ASSERT_NE(nullptr, handle);
        zes_power_properties_t properties = {};
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesPowerGetProperties(handle, &properties));
        EXPECT_FALSE(properties.onSubdevice);
        EXPECT_EQ(properties.subdeviceId, 0u);
        EXPECT_EQ(properties.canControl, true);
        EXPECT_EQ(properties.isEnergyThresholdSupported, false);
        EXPECT_EQ(properties.defaultLimit, static_cast<int32_t>(mockDefaultPowerLimitVal / milliFactor));
        EXPECT_EQ(properties.maxLimit, static_cast<int32_t>(mockDefaultPowerLimitVal / milliFactor));
        EXPECT_EQ(properties.minLimit, -1);
    }
}

TEST_F(SysmanDevicePowerFixtureXe, GivenValidPowerHandleWhenGettingPowerPropertiesAndExtPropertiesThenCallSucceedsForCardDomain) {
    MockSysmanProductHelper *pMockSysmanProductHelper = new MockSysmanProductHelper();
    pMockSysmanProductHelper->isPowerSetLimitSupportedResult = true;
    std::unique_ptr<SysmanProductHelper> pSysmanProductHelper(static_cast<SysmanProductHelper *>(pMockSysmanProductHelper));
    std::swap(pLinuxSysmanImp->pSysmanProductHelper, pSysmanProductHelper);

    for (const auto &handle : pSysmanDeviceImp->pPowerHandleContext->handleList) {
        delete handle;
    }
    pSysmanDeviceImp->pPowerHandleContext->handleList.clear();
    pSysmanDeviceImp->pPowerHandleContext->init(pLinuxSysmanImp->getSubDeviceCount());
    auto handles = getPowerHandles(powerHandleComponentCount);
    std::vector<zes_power_domain_t> mockPowerDomains = {ZES_POWER_DOMAIN_CARD, ZES_POWER_DOMAIN_PACKAGE};
    uint32_t count = 0;

    for (auto handle : handles) {
        ASSERT_NE(nullptr, handle);
        zes_power_properties_t properties = {};
        zes_power_ext_properties_t extProperties = {};
        zes_power_limit_ext_desc_t defaultLimit = {};

        extProperties.defaultLimit = &defaultLimit;
        extProperties.stype = ZES_STRUCTURE_TYPE_POWER_EXT_PROPERTIES;
        properties.pNext = &extProperties;
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesPowerGetProperties(handle, &properties));
        EXPECT_FALSE(properties.onSubdevice);
        EXPECT_EQ(properties.subdeviceId, 0u);
        EXPECT_EQ(properties.canControl, true);
        EXPECT_EQ(properties.isEnergyThresholdSupported, false);
        EXPECT_EQ(properties.defaultLimit, (int32_t)(mockDefaultPowerLimitVal / milliFactor));
        EXPECT_EQ(properties.maxLimit, (int32_t)(mockDefaultPowerLimitVal / milliFactor));
        EXPECT_EQ(properties.minLimit, -1);
        EXPECT_EQ(extProperties.domain, mockPowerDomains[count++]);
        EXPECT_TRUE(defaultLimit.limitValueLocked);
        EXPECT_TRUE(defaultLimit.enabledStateLocked);
        EXPECT_TRUE(defaultLimit.intervalValueLocked);
        EXPECT_EQ(ZES_POWER_SOURCE_ANY, defaultLimit.source);
        EXPECT_EQ(ZES_LIMIT_UNIT_POWER, defaultLimit.limitUnit);
        EXPECT_EQ(defaultLimit.limit, (int32_t)(mockDefaultPowerLimitVal / milliFactor));
    }
}

TEST_F(SysmanDevicePowerFixtureXe, GivenValidPowerHandleWithNoStypeForExtPropertiesWhenGettingPowerPropertiesAndExtPropertiesThenCallSucceeds) {
    MockSysmanProductHelper *pMockSysmanProductHelper = new MockSysmanProductHelper();
    pMockSysmanProductHelper->isPowerSetLimitSupportedResult = true;
    std::unique_ptr<SysmanProductHelper> pSysmanProductHelper(static_cast<SysmanProductHelper *>(pMockSysmanProductHelper));
    std::swap(pLinuxSysmanImp->pSysmanProductHelper, pSysmanProductHelper);

    for (const auto &handle : pSysmanDeviceImp->pPowerHandleContext->handleList) {
        delete handle;
    }
    pSysmanDeviceImp->pPowerHandleContext->handleList.clear();
    pSysmanDeviceImp->pPowerHandleContext->init(pLinuxSysmanImp->getSubDeviceCount());
    auto handles = getPowerHandles(powerHandleComponentCount);
    for (auto handle : handles) {
        ASSERT_NE(nullptr, handle);
        zes_power_properties_t properties = {};
        zes_power_ext_properties_t extProperties = {};
        zes_power_limit_ext_desc_t defaultLimit = {};

        extProperties.defaultLimit = &defaultLimit;
        properties.pNext = &extProperties;
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesPowerGetProperties(handle, &properties));
        EXPECT_FALSE(properties.onSubdevice);
        EXPECT_EQ(properties.subdeviceId, 0u);
        EXPECT_EQ(properties.canControl, true);
        EXPECT_EQ(properties.isEnergyThresholdSupported, false);
        EXPECT_EQ(properties.defaultLimit, (int32_t)(mockDefaultPowerLimitVal / milliFactor));
        EXPECT_EQ(properties.maxLimit, (int32_t)(mockDefaultPowerLimitVal / milliFactor));
        EXPECT_EQ(properties.minLimit, -1);
    }
}

TEST_F(SysmanDevicePowerFixtureXe, GivenValidPowerHandleWithNoDefaultLimitStructureForExtPropertiesWhenGettingPowerPropertiesAndExtPropertiesThenCallSucceeds) {
    MockSysmanProductHelper *pMockSysmanProductHelper = new MockSysmanProductHelper();
    pMockSysmanProductHelper->isPowerSetLimitSupportedResult = true;
    std::unique_ptr<SysmanProductHelper> pSysmanProductHelper(static_cast<SysmanProductHelper *>(pMockSysmanProductHelper));
    std::swap(pLinuxSysmanImp->pSysmanProductHelper, pSysmanProductHelper);

    for (const auto &handle : pSysmanDeviceImp->pPowerHandleContext->handleList) {
        delete handle;
    }
    pSysmanDeviceImp->pPowerHandleContext->handleList.clear();
    pSysmanDeviceImp->pPowerHandleContext->init(pLinuxSysmanImp->getSubDeviceCount());
    auto handles = getPowerHandles(powerHandleComponentCount);
    for (auto handle : handles) {
        ASSERT_NE(nullptr, handle);
        zes_power_properties_t properties = {};
        zes_power_ext_properties_t extProperties = {};

        properties.pNext = &extProperties;
        extProperties.stype = ZES_STRUCTURE_TYPE_POWER_EXT_PROPERTIES;
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesPowerGetProperties(handle, &properties));
        EXPECT_FALSE(properties.onSubdevice);
        EXPECT_EQ(properties.subdeviceId, 0u);
        EXPECT_EQ(properties.canControl, true);
        EXPECT_EQ(properties.isEnergyThresholdSupported, false);
        EXPECT_EQ(properties.defaultLimit, (int32_t)(mockDefaultPowerLimitVal / milliFactor));
        EXPECT_EQ(properties.maxLimit, (int32_t)(mockDefaultPowerLimitVal / milliFactor));
        EXPECT_EQ(properties.minLimit, -1);
    }
}

TEST_F(SysmanDevicePowerFixtureXe, GivenInvalidComponentCountWhenEnumeratingPowerDomainsThenValidCountIsReturnedAndVerifySysmanPowerGetCallSucceeds) {
    uint32_t count = 0;
    EXPECT_EQ(zesDeviceEnumPowerDomains(device->toHandle(), &count, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(count, powerHandleComponentCount);

    count = count + 1;
    EXPECT_EQ(zesDeviceEnumPowerDomains(device->toHandle(), &count, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(count, powerHandleComponentCount);
}

HWTEST2_F(SysmanMultiDevicePowerFixtureXe, GivenInvalidComponentCountWhenEnumeratingPowerDomainsThenValidCountIsReturnedAndVerifySysmanPowerGetCallSucceeds, IsPVC) {
    uint32_t count = 0;
    EXPECT_EQ(zesDeviceEnumPowerDomains(device->toHandle(), &count, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(count, powerHandleComponentCount);

    count = count + 1;
    EXPECT_EQ(zesDeviceEnumPowerDomains(device->toHandle(), &count, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(count, powerHandleComponentCount);
}

TEST_F(SysmanMultiDevicePowerFixtureXe, GivenValidPowerHandleWhenGettingPowerPropertiesThenCallSucceeds) {
    MockSysmanProductHelper *pMockSysmanProductHelper = new MockSysmanProductHelper();
    pMockSysmanProductHelper->isPowerSetLimitSupportedResult = true;
    std::unique_ptr<SysmanProductHelper> pSysmanProductHelper(static_cast<SysmanProductHelper *>(pMockSysmanProductHelper));
    std::swap(pLinuxSysmanImp->pSysmanProductHelper, pSysmanProductHelper);

    for (const auto &handle : pSysmanDeviceImp->pPowerHandleContext->handleList) {
        delete handle;
    }
    pSysmanDeviceImp->pPowerHandleContext->handleList.clear();
    pSysmanDeviceImp->pPowerHandleContext->init(pLinuxSysmanImp->getSubDeviceCount());
    auto handles = getPowerHandles(powerHandleComponentCount);
    for (auto handle : handles) {
        if (handle) {
            zes_power_properties_t properties = {};
            zes_power_ext_properties_t extProperties = {};
            zes_power_limit_ext_desc_t defaultLimit = {};

            extProperties.defaultLimit = &defaultLimit;
            extProperties.stype = ZES_STRUCTURE_TYPE_POWER_EXT_PROPERTIES;
            properties.pNext = &extProperties;
            EXPECT_EQ(ZE_RESULT_SUCCESS, zesPowerGetProperties(handle, &properties));
            EXPECT_EQ(properties.minLimit, -1);
            EXPECT_EQ(properties.isEnergyThresholdSupported, false);
            EXPECT_TRUE(defaultLimit.limitValueLocked);
            EXPECT_TRUE(defaultLimit.enabledStateLocked);
            EXPECT_TRUE(defaultLimit.intervalValueLocked);
            EXPECT_EQ(ZES_POWER_SOURCE_ANY, defaultLimit.source);
            EXPECT_EQ(ZES_LIMIT_UNIT_POWER, defaultLimit.limitUnit);
            if (properties.onSubdevice) {
                EXPECT_EQ(properties.canControl, false);
                EXPECT_EQ(properties.defaultLimit, -1);
                EXPECT_EQ(properties.maxLimit, -1);
                EXPECT_EQ(defaultLimit.limit, -1);
            } else {
                EXPECT_EQ(properties.canControl, true);
                EXPECT_EQ(properties.defaultLimit, (int32_t)(mockDefaultPowerLimitVal / milliFactor));
                EXPECT_EQ(properties.maxLimit, (int32_t)(mockDefaultPowerLimitVal / milliFactor));
                EXPECT_EQ(defaultLimit.limit, (int32_t)(mockDefaultPowerLimitVal / milliFactor));
            }
        }
    }
}

HWTEST2_F(SysmanMultiDevicePowerFixtureXe, GivenSetPowerLimitsWhenGettingPowerLimitsThenLimitsSetEarlierAreRetrieved, IsPVC) {
    auto handles = getPowerHandles(powerHandleComponentCount);
    for (auto handle : handles) {
        if (handle) {
            zes_power_properties_t properties = {};
            EXPECT_EQ(ZE_RESULT_SUCCESS, zesPowerGetProperties(handle, &properties));

            zes_power_sustained_limit_t sustainedSet = {};
            zes_power_sustained_limit_t sustainedGet = {};
            sustainedSet.power = 300000;
            if (!properties.onSubdevice) {
                EXPECT_EQ(ZE_RESULT_SUCCESS, zesPowerSetLimits(handle, &sustainedSet, nullptr, nullptr));
                EXPECT_EQ(ZE_RESULT_SUCCESS, zesPowerGetLimits(handle, &sustainedGet, nullptr, nullptr));
                EXPECT_EQ(sustainedGet.power, sustainedSet.power);
            } else {
                EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesPowerSetLimits(handle, &sustainedSet, nullptr, nullptr));
                EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesPowerGetLimits(handle, &sustainedGet, nullptr, nullptr));
            }

            zes_power_burst_limit_t burstGet = {};
            if (!properties.onSubdevice) {
                EXPECT_EQ(ZE_RESULT_SUCCESS, zesPowerGetLimits(handle, nullptr, &burstGet, nullptr));
                EXPECT_EQ(burstGet.enabled, false);
                EXPECT_EQ(burstGet.power, -1);
            } else {
                EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesPowerGetLimits(handle, nullptr, &burstGet, nullptr));
            }

            zes_power_peak_limit_t peakSet = {};
            zes_power_peak_limit_t peakGet = {};
            peakSet.powerAC = 300000;
            if (!properties.onSubdevice) {
                EXPECT_EQ(ZE_RESULT_SUCCESS, zesPowerSetLimits(handle, nullptr, nullptr, &peakSet));
                EXPECT_EQ(ZE_RESULT_SUCCESS, zesPowerGetLimits(handle, nullptr, nullptr, &peakGet));
                EXPECT_EQ(peakGet.powerAC, peakSet.powerAC);
                EXPECT_EQ(peakGet.powerDC, -1);
            } else {
                EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesPowerGetLimits(handle, nullptr, nullptr, &peakGet));
            }
        }
    }
}

HWTEST2_F(SysmanMultiDevicePowerFixtureXe, GivenSetPowerLimitsWhenGettingPowerLimitsThenLimitsSetEarlierAreRetrieved, IsBMG) {
    auto handles = getPowerHandles(powerHandleComponentCount);
    for (auto handle : handles) {
        if (handle) {
            auto phPower = Power::fromHandle(handle);
            zes_power_properties_t properties = {};
            EXPECT_EQ(ZE_RESULT_SUCCESS, zesPowerGetProperties(handle, &properties));

            zes_power_sustained_limit_t sustainedSet = {};
            zes_power_sustained_limit_t sustainedGet = {};
            sustainedSet.power = 300000;
            if (!properties.onSubdevice) {
                EXPECT_EQ(ZE_RESULT_SUCCESS, zesPowerSetLimits(handle, &sustainedSet, nullptr, nullptr));
                EXPECT_EQ(ZE_RESULT_SUCCESS, zesPowerGetLimits(handle, &sustainedGet, nullptr, nullptr));
                EXPECT_EQ(sustainedGet.power, sustainedSet.power);
            } else {
                EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesPowerSetLimits(handle, &sustainedSet, nullptr, nullptr));
                EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesPowerGetLimits(handle, &sustainedGet, nullptr, nullptr));
            }

            zes_power_burst_limit_t burstGet = {};
            if (!properties.onSubdevice) {
                EXPECT_EQ(ZE_RESULT_SUCCESS, zesPowerGetLimits(handle, nullptr, &burstGet, nullptr));
                EXPECT_EQ(burstGet.enabled, false);
                EXPECT_EQ(burstGet.power, -1);
            } else {
                EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesPowerGetLimits(handle, nullptr, &burstGet, nullptr));
            }

            zes_power_peak_limit_t peakSet = {};
            zes_power_peak_limit_t peakGet = {};
            peakSet.powerAC = 300000;
            if (!properties.onSubdevice) {
                if (!phPower->isCardPower) {
                    EXPECT_EQ(ZE_RESULT_SUCCESS, zesPowerSetLimits(handle, nullptr, nullptr, &peakSet));
                    EXPECT_EQ(ZE_RESULT_SUCCESS, zesPowerGetLimits(handle, nullptr, nullptr, &peakGet));
                    EXPECT_EQ(peakGet.powerAC, peakSet.powerAC);
                    EXPECT_EQ(peakGet.powerDC, -1);
                } else {
                    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesPowerSetLimits(handle, nullptr, nullptr, &peakSet));
                    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesPowerGetLimits(handle, nullptr, nullptr, &peakGet));
                }
            } else {
                EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesPowerGetLimits(handle, nullptr, nullptr, &peakGet));
            }
        }
    }
}

HWTEST2_F(SysmanMultiDevicePowerFixtureXe, GivenValidPowerHandlesWhenCallingSetAndGetPowerLimitExtThenLimitsSetEarlierAreRetrieved, IsPVC) {
    auto handles = getPowerHandles(powerHandleComponentCount);
    for (auto handle : handles) {
        if (handle) {
            uint32_t limitCount = 0;
            const int32_t testLimit = 300000;
            const int32_t testInterval = 10;

            zes_power_properties_t properties = {};
            EXPECT_EQ(ZE_RESULT_SUCCESS, zesPowerGetProperties(handle, &properties));

            EXPECT_EQ(ZE_RESULT_SUCCESS, zesPowerGetLimitsExt(handle, &limitCount, nullptr));
            EXPECT_EQ(limitCount, maxLimitCountSupported);

            limitCount++;
            EXPECT_EQ(ZE_RESULT_SUCCESS, zesPowerGetLimitsExt(handle, &limitCount, nullptr));
            EXPECT_EQ(limitCount, maxLimitCountSupported);

            std::vector<zes_power_limit_ext_desc_t> allLimits(limitCount);
            EXPECT_EQ(ZE_RESULT_SUCCESS, zesPowerGetLimitsExt(handle, &limitCount, allLimits.data()));
            EXPECT_EQ(limitCount, maxLimitCountSupported);

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
                    EXPECT_EQ(ZES_POWER_SOURCE_ANY, allLimits[i].source);
                    EXPECT_EQ(ZES_LIMIT_UNIT_CURRENT, allLimits[i].limitUnit);
                    allLimits[i].limit = testLimit;
                }
            }
            if (!properties.onSubdevice) {
                EXPECT_EQ(ZE_RESULT_SUCCESS, zesPowerSetLimitsExt(handle, &limitCount, allLimits.data()));
                EXPECT_EQ(ZE_RESULT_SUCCESS, zesPowerGetLimitsExt(handle, &limitCount, allLimits.data()));
                for (uint32_t i = 0; i < limitCount; i++) {
                    if (allLimits[i].level == ZES_POWER_LEVEL_SUSTAINED) {
                        EXPECT_EQ(testInterval, allLimits[i].interval);
                    } else if (allLimits[i].level == ZES_POWER_LEVEL_PEAK) {
                        EXPECT_EQ(0, allLimits[i].interval);
                    }
                    EXPECT_EQ(testLimit, allLimits[i].limit);
                }
            } else {
                EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesPowerSetLimitsExt(handle, &limitCount, allLimits.data()));
            }
        }
    }
}

HWTEST2_F(SysmanDevicePowerFixtureXe, GivenValidPowerHandlesWhenCallingSetAndGetPowerLimitExtThenLimitsSetEarlierAreRetrieved, IsBMG) {
    auto handles = getPowerHandles(powerHandleComponentCount);
    for (auto handle : handles) {
        if (handle) {
            auto phPower = Power::fromHandle(handle);
            uint32_t limitCount = 0;
            const int32_t testLimit = 300000;
            const int32_t testInterval = 10;

            zes_power_properties_t properties = {};
            EXPECT_EQ(ZE_RESULT_SUCCESS, zesPowerGetProperties(handle, &properties));

            EXPECT_EQ(ZE_RESULT_SUCCESS, zesPowerGetLimitsExt(handle, &limitCount, nullptr));
            EXPECT_EQ(limitCount, phPower->isCardPower ? singleLimitCount : maxLimitCountSupported);

            limitCount++;
            EXPECT_EQ(ZE_RESULT_SUCCESS, zesPowerGetLimitsExt(handle, &limitCount, nullptr));
            EXPECT_EQ(limitCount, phPower->isCardPower ? singleLimitCount : maxLimitCountSupported);

            std::vector<zes_power_limit_ext_desc_t> allLimits(limitCount);
            EXPECT_EQ(ZE_RESULT_SUCCESS, zesPowerGetLimitsExt(handle, &limitCount, allLimits.data()));
            EXPECT_EQ(limitCount, phPower->isCardPower ? singleLimitCount : maxLimitCountSupported);

            for (uint32_t i = 0; i < limitCount; i++) {
                if (allLimits[i].level == ZES_POWER_LEVEL_SUSTAINED) {
                    EXPECT_FALSE(allLimits[i].limitValueLocked);
                    EXPECT_TRUE(allLimits[i].enabledStateLocked);
                    EXPECT_FALSE(allLimits[i].intervalValueLocked);
                    EXPECT_EQ(ZES_POWER_SOURCE_ANY, allLimits[i].source);
                    EXPECT_EQ(ZES_LIMIT_UNIT_POWER, allLimits[i].limitUnit);
                    allLimits[i].limit = testLimit;
                    allLimits[i].interval = testInterval;
                } else if (!phPower->isCardPower && allLimits[i].level == ZES_POWER_LEVEL_PEAK) {
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
                } else if (allLimits[i].level == ZES_POWER_LEVEL_PEAK) {
                    EXPECT_EQ(0, allLimits[i].interval);
                }
                EXPECT_EQ(testLimit, allLimits[i].limit);
            }
        }
    }
}

} // namespace ult
} // namespace Sysman
} // namespace L0