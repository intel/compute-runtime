/*
 * Copyright (C) 2020-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/sysman/test/unit_tests/sources/linux/mocks/mock_sysman_product_helper.h"
#include "level_zero/sysman/test/unit_tests/sources/power/linux/mock_sysfs_power.h"

namespace L0 {
namespace Sysman {
namespace ult {

constexpr uint32_t i915PowerHandleComponentCount = 1u;

TEST_F(SysmanDevicePowerFixtureI915, GivenComponentCountZeroWhenEnumeratingPowerDomainsThenValidCountIsReturnedAndVerifySysmanPowerGetCallSucceeds) {
    uint32_t count = 0;
    EXPECT_EQ(zesDeviceEnumPowerDomains(device->toHandle(), &count, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(count, i915PowerHandleComponentCount);
}

TEST_F(SysmanDevicePowerFixtureI915, GivenInvalidComponentCountWhenEnumeratingPowerDomainsThenValidCountIsReturnedAndVerifySysmanPowerGetCallSucceeds) {
    uint32_t count = 0;
    EXPECT_EQ(zesDeviceEnumPowerDomains(device->toHandle(), &count, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(count, i915PowerHandleComponentCount);

    count = count + 1;
    EXPECT_EQ(zesDeviceEnumPowerDomains(device->toHandle(), &count, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(count, i915PowerHandleComponentCount);
}

TEST_F(SysmanDevicePowerFixtureI915, GivenComponentCountZeroWhenEnumeratingPowerDomainsThenValidPowerHandlesIsReturned) {
    uint32_t count = 0;
    EXPECT_EQ(zesDeviceEnumPowerDomains(device->toHandle(), &count, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(count, i915PowerHandleComponentCount);

    std::vector<zes_pwr_handle_t> handles(count, nullptr);
    EXPECT_EQ(zesDeviceEnumPowerDomains(device->toHandle(), &count, handles.data()), ZE_RESULT_SUCCESS);
    for (auto handle : handles) {
        EXPECT_NE(handle, nullptr);
    }
}

TEST_F(SysmanDevicePowerFixtureI915, GivenValidPowerPointerWhenGettingCardPowerDomainAndThenCallSucceeds) {
    zes_pwr_handle_t phPower = {};
    EXPECT_EQ(zesDeviceGetCardPowerDomain(device->toHandle(), &phPower), ZE_RESULT_SUCCESS);
}

TEST_F(SysmanDevicePowerFixtureI915, GivenInvalidPowerPointerWhenGettingCardPowerDomainAndThenReturnsFailure) {
    EXPECT_EQ(zesDeviceGetCardPowerDomain(device->toHandle(), nullptr), ZE_RESULT_ERROR_INVALID_NULL_POINTER);
}

TEST_F(SysmanDevicePowerFixtureI915, GivenUninitializedPowerHandlesAndWhenGettingCardPowerDomainThenReturnsFailure) {
    auto handles = getPowerHandles(0);
    for (const auto &handle : pSysmanDeviceImp->pPowerHandleContext->handleList) {
        delete handle;
    }
    pSysmanDeviceImp->pPowerHandleContext->handleList.clear();

    zes_pwr_handle_t phPower = {};
    EXPECT_EQ(zesDeviceGetCardPowerDomain(device->toHandle(), &phPower), ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
}

TEST_F(SysmanDevicePowerFixtureI915, GivenScanDirectoriesFailAndPmtIsNullWhenGettingCardPowerThenReturnsFailure) {
    pSysfsAccess->mockscanDirEntriesResult.push_back(ZE_RESULT_ERROR_NOT_AVAILABLE);

    auto handles = getPowerHandles(0);
    for (const auto &handle : pSysmanDeviceImp->pPowerHandleContext->handleList) {
        delete handle;
    }
    pSysmanDeviceImp->pPowerHandleContext->handleList.clear();

    zes_pwr_handle_t phPower = {};
    EXPECT_EQ(zesDeviceGetCardPowerDomain(device->toHandle(), &phPower), ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
}

TEST_F(SysmanDevicePowerFixtureI915, GivenHwmonDirectoriesDoesNotExistWhenGettingPowerHandlesThenNoHandlesAreReturned) {
    pSysfsAccess->mockscanDirEntriesResult.clear();
    pSysfsAccess->mockscanDirEntriesResult.push_back(ZE_RESULT_ERROR_NOT_AVAILABLE);
    pSysfsAccess->mockscanDirEntriesResult.push_back(ZE_RESULT_ERROR_NOT_AVAILABLE);

    uint32_t count = 0;
    EXPECT_EQ(zesDeviceEnumPowerDomains(device->toHandle(), &count, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(count, 0u);
}

TEST_F(SysmanDevicePowerFixtureI915, GivenHwmonDirectoriesDoesNotContainNameFileWhenGettingPowerHandlesThenNoHandlesAreReturned) {
    pSysfsAccess->mockReadResult = ZE_RESULT_ERROR_NOT_AVAILABLE;

    uint32_t count = 0;
    EXPECT_EQ(zesDeviceEnumPowerDomains(device->toHandle(), &count, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(count, 0u);
}

TEST_F(SysmanDevicePowerFixtureI915, GivenEnergyCounterNodeIsNotAvailableWhenGettingPowerHandlesThenNoHandlesAreReturned) {
    pSysmanKmdInterface->isEnergyNodeAvailable = false;

    uint32_t count = 0;
    EXPECT_EQ(zesDeviceEnumPowerDomains(device->toHandle(), &count, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(count, 0u);
}

TEST_F(SysmanDevicePowerFixtureI915, GivenValidPowerHandleAndPowerDomainIsCardWhenGettingPowerPropertiessThenCallSucceeds) {
    MockSysmanProductHelper *pMockSysmanProductHelper = new MockSysmanProductHelper();
    pMockSysmanProductHelper->isPowerSetLimitSupportedResult = true;
    std::unique_ptr<SysmanProductHelper> pSysmanProductHelper(static_cast<SysmanProductHelper *>(pMockSysmanProductHelper));
    std::swap(pLinuxSysmanImp->pSysmanProductHelper, pSysmanProductHelper);

    auto handles = getPowerHandles(i915PowerHandleComponentCount);
    auto handle = handles[0];
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

TEST_F(SysmanDevicePowerFixtureI915, GivenValidPowerHandleWhenGettingPowerPropertiesAndExtPropertiesThenCallSucceedsForCardDomain) {
    MockSysmanProductHelper *pMockSysmanProductHelper = new MockSysmanProductHelper();
    pMockSysmanProductHelper->isPowerSetLimitSupportedResult = true;
    std::unique_ptr<SysmanProductHelper> pSysmanProductHelper(static_cast<SysmanProductHelper *>(pMockSysmanProductHelper));
    std::swap(pLinuxSysmanImp->pSysmanProductHelper, pSysmanProductHelper);

    auto handles = getPowerHandles(i915PowerHandleComponentCount);
    auto handle = handles[0];
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
    EXPECT_EQ(extProperties.domain, ZES_POWER_DOMAIN_CARD);
    EXPECT_TRUE(defaultLimit.limitValueLocked);
    EXPECT_TRUE(defaultLimit.enabledStateLocked);
    EXPECT_TRUE(defaultLimit.intervalValueLocked);
    EXPECT_EQ(ZES_POWER_SOURCE_ANY, defaultLimit.source);
    EXPECT_EQ(ZES_LIMIT_UNIT_POWER, defaultLimit.limitUnit);
    EXPECT_EQ(defaultLimit.limit, (int32_t)(mockDefaultPowerLimitVal / milliFactor));
}

TEST_F(SysmanDevicePowerFixtureI915, GivenValidPowerHandleWithNoStypeForExtPropertiesWhenGettingPowerPropertiesAndExtPropertiesThenCallSucceeds) {
    MockSysmanProductHelper *pMockSysmanProductHelper = new MockSysmanProductHelper();
    pMockSysmanProductHelper->isPowerSetLimitSupportedResult = true;
    std::unique_ptr<SysmanProductHelper> pSysmanProductHelper(static_cast<SysmanProductHelper *>(pMockSysmanProductHelper));
    std::swap(pLinuxSysmanImp->pSysmanProductHelper, pSysmanProductHelper);

    auto handles = getPowerHandles(i915PowerHandleComponentCount);
    auto handle = handles[0];
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

TEST_F(SysmanDevicePowerFixtureI915, GivenValidPowerHandleAndPowerSetLimitIsUnsupportedWhenCallingZesPowerGetPropertiesThenCanControlIsFalse) {
    std::unique_ptr<SysmanProductHelper> pSysmanProductHelper = std::make_unique<MockSysmanProductHelper>();
    std::swap(pLinuxSysmanImp->pSysmanProductHelper, pSysmanProductHelper);

    auto handles = getPowerHandles(i915PowerHandleComponentCount);
    auto handle = handles[0];
    ASSERT_NE(nullptr, handle);
    zes_power_properties_t properties{};
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesPowerGetProperties(handle, &properties));
    EXPECT_FALSE(properties.canControl);
}

TEST_F(SysmanDevicePowerFixtureI915, GivenValidPowerHandleWithUnknownPowerDomainWhenGetDefaultLimitIsCalledThenProperErrorCodeIsReturned) {
    std::unique_ptr<PublicLinuxPowerImp> pLinuxPowerImp(new PublicLinuxPowerImp(pOsSysman, false, 0, ZES_POWER_DOMAIN_UNKNOWN));
    zes_power_properties_t properties{};
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, pLinuxPowerImp->getProperties(&properties));
    EXPECT_EQ(properties.defaultLimit, -1);
}

TEST_F(SysmanDevicePowerFixtureI915, GivenValidPowerHandleWhenGettingPowerPropertiesAndSysfsReadFailsThenFailureIsReturned) {
    std::unique_ptr<PublicLinuxPowerImp> pLinuxPowerImp(new PublicLinuxPowerImp(pOsSysman, false, 0, ZES_POWER_DOMAIN_CARD));
    EXPECT_TRUE(pLinuxPowerImp->isPowerModuleSupported());
    pSysfsAccess->mockReadValUnsignedLongResult.push_back(ZE_RESULT_ERROR_NOT_AVAILABLE);
    zes_power_properties_t properties{};
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, pLinuxPowerImp->getProperties(&properties));
    EXPECT_EQ(properties.defaultLimit, -1);
}

TEST_F(SysmanDevicePowerFixtureI915, GivenValidPowerHandleAndHandleCountZeroWhenCallingReInitThenValidCountIsReturnedAndVerifyzesDeviceEnumPowerHandleSucceeds) {
    VariableBackup<L0::Sysman::SysFsAccessInterface *> sysfsBackup(&pLinuxSysmanImp->pSysfsAccess);
    pLinuxSysmanImp->pSysfsAccess = pSysfsAccess;

    uint32_t count = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEnumPowerDomains(device->toHandle(), &count, NULL));
    EXPECT_EQ(count, i915PowerHandleComponentCount);

    for (auto handle : pSysmanDeviceImp->pPowerHandleContext->handleList) {
        delete handle;
    }
    pSysmanDeviceImp->pPowerHandleContext->handleList.clear();

    pLinuxSysmanImp->reInitSysmanDeviceResources();

    count = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEnumPowerDomains(device->toHandle(), &count, NULL));
    EXPECT_EQ(count, i915PowerHandleComponentCount);
}

HWTEST2_F(SysmanDevicePowerFixtureI915, GivenSetPowerLimitsWhenGettingPowerLimitsThenLimitsSetEarlierAreRetrieved, IsPVC) {
    auto handles = getPowerHandles(i915PowerHandleComponentCount);
    auto handle = handles[0];
    ASSERT_NE(nullptr, handle);
    zes_power_sustained_limit_t sustainedSet = {};
    zes_power_sustained_limit_t sustainedGet = {};
    sustainedSet.enabled = 1;
    sustainedSet.interval = 10;
    sustainedSet.power = 300000;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesPowerSetLimits(handle, &sustainedSet, nullptr, nullptr));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesPowerGetLimits(handle, &sustainedGet, nullptr, nullptr));
    EXPECT_EQ(sustainedGet.power, sustainedSet.power);
    zes_power_burst_limit_t burstGet = {};
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesPowerSetLimits(handle, nullptr, nullptr, nullptr));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesPowerGetLimits(handle, nullptr, &burstGet, nullptr));
    EXPECT_EQ(burstGet.enabled, false);
    EXPECT_EQ(burstGet.power, -1);

    zes_power_peak_limit_t peakSet = {};
    zes_power_peak_limit_t peakGet = {};
    peakSet.powerAC = 300000;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesPowerSetLimits(handle, nullptr, nullptr, &peakSet));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesPowerGetLimits(handle, nullptr, nullptr, &peakGet));
    EXPECT_EQ(peakGet.powerAC, peakSet.powerAC);
    EXPECT_EQ(peakGet.powerDC, -1);
}

HWTEST2_F(SysmanDevicePowerFixtureI915, GivenSetPowerLimitsWhenGettingPowerLimitsThenLimitsSetEarlierAreRetrieved, IsDG1) {
    auto handles = getPowerHandles(i915PowerHandleComponentCount);
    auto handle = handles[0];
    ASSERT_NE(nullptr, handle);
    zes_power_sustained_limit_t sustainedSet = {};
    zes_power_sustained_limit_t sustainedGet = {};
    sustainedSet.enabled = 1;
    sustainedSet.interval = 10;
    sustainedSet.power = 300000;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesPowerSetLimits(handle, &sustainedSet, nullptr, nullptr));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesPowerGetLimits(handle, &sustainedGet, nullptr, nullptr));
    EXPECT_EQ(sustainedGet.power, sustainedSet.power);
    zes_power_burst_limit_t burstGet = {};
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesPowerSetLimits(handle, nullptr, nullptr, nullptr));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesPowerGetLimits(handle, nullptr, &burstGet, nullptr));
    EXPECT_EQ(burstGet.enabled, false);
    EXPECT_EQ(burstGet.power, -1);

    zes_power_peak_limit_t peakSet = {};
    zes_power_peak_limit_t peakGet = {};
    peakSet.powerAC = 300000;
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesPowerSetLimits(handle, nullptr, nullptr, &peakSet));
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesPowerGetLimits(handle, nullptr, nullptr, &peakGet));
}

TEST_F(SysmanDevicePowerFixtureI915, GivenDefaultLimitSysfsNodesNotAvailableWhenGettingPowerPropertiesThenApiCallReturnsFailure) {
    auto handles = getPowerHandles(i915PowerHandleComponentCount);
    auto handle = handles[0];
    ASSERT_NE(nullptr, handle);
    zes_power_properties_t properties = {};
    pSysfsAccess->mockReadValUnsignedLongResult.push_back(ZE_RESULT_ERROR_NOT_AVAILABLE);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesPowerGetProperties(handle, &properties));
}

TEST_F(SysmanDevicePowerFixtureI915, GivenGetPropertiesExtCallFailsWhenGettingPowerPropertiesThenApiCallReturnsFailure) {
    auto handles = getPowerHandles(i915PowerHandleComponentCount);
    auto handle = handles[0];
    ASSERT_NE(nullptr, handle);
    zes_power_properties_t properties = {};
    zes_power_ext_properties_t extProperties = {};
    zes_power_limit_ext_desc_t defaultLimit = {};

    extProperties.defaultLimit = &defaultLimit;
    extProperties.stype = ZES_STRUCTURE_TYPE_POWER_EXT_PROPERTIES;
    properties.pNext = &extProperties;
    pSysfsAccess->mockReadValUnsignedLongResult.push_back(ZE_RESULT_SUCCESS);
    pSysfsAccess->mockReadValUnsignedLongResult.push_back(ZE_RESULT_ERROR_NOT_AVAILABLE);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesPowerGetProperties(handle, &properties));
}

TEST_F(SysmanDevicePowerFixtureI915, GivenDefaultLimitSysfsNodesNotAvailableWhenGettingPowerPropertiesExtThenApiCallReturnsFailure) {
    auto handles = getPowerHandles(i915PowerHandleComponentCount);
    for (auto handle : handles) {
        ASSERT_NE(nullptr, handle);
        zes_power_properties_t properties = {};
        zes_power_ext_properties_t extProperties = {};
        zes_power_limit_ext_desc_t defaultLimit = {};

        extProperties.defaultLimit = &defaultLimit;
        extProperties.stype = ZES_STRUCTURE_TYPE_POWER_EXT_PROPERTIES;
        properties.pNext = &extProperties;

        pSysfsAccess->mockReadValUnsignedLongResult.push_back(ZE_RESULT_ERROR_NOT_AVAILABLE);
        EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesPowerGetProperties(handle, &properties));
    }
}

HWTEST2_F(SysmanDevicePowerFixtureI915, GivenValidPowerHandlesWhenCallingSetAndGetPowerLimitExtThenLimitsSetEarlierAreRetrieved, IsPVC) {
    auto handles = getPowerHandles(i915PowerHandleComponentCount);
    auto handle = handles[0];
    ASSERT_NE(nullptr, handle);

    uint32_t limitCount = 0;
    const int32_t testLimit = 300000;
    const int32_t testInterval = 10;

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesPowerGetLimitsExt(handle, &limitCount, nullptr));
    EXPECT_EQ(limitCount, maxLimitCountSupported);

    limitCount++;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesPowerGetLimitsExt(handle, &limitCount, nullptr));
    EXPECT_EQ(limitCount, maxLimitCountSupported);

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
            EXPECT_EQ(ZES_POWER_SOURCE_ANY, allLimits[i].source);
            EXPECT_EQ(ZES_LIMIT_UNIT_CURRENT, allLimits[i].limitUnit);
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

HWTEST2_F(SysmanDevicePowerFixtureI915, GivenValidPowerHandlesWhenCallingSetAndGetPowerLimitExtThenLimitsSetEarlierAreRetrieved, IsDG1) {
    auto handles = getPowerHandles(i915PowerHandleComponentCount);
    auto handle = handles[0];
    ASSERT_NE(nullptr, handle);

    uint32_t limitCount = 0;
    const int32_t testLimit = 300000;
    const int32_t testInterval = 10;

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesPowerGetLimitsExt(handle, &limitCount, nullptr));
    EXPECT_EQ(limitCount, singleLimitCount);

    limitCount++;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesPowerGetLimitsExt(handle, &limitCount, nullptr));
    EXPECT_EQ(limitCount, singleLimitCount);

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
        }
    }

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesPowerSetLimitsExt(handle, &limitCount, allLimits.data()));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesPowerGetLimitsExt(handle, &limitCount, allLimits.data()));

    for (uint32_t i = 0; i < limitCount; i++) {
        if (allLimits[i].level == ZES_POWER_LEVEL_SUSTAINED) {
            EXPECT_EQ(testInterval, allLimits[i].interval);
        }
        EXPECT_EQ(testLimit, allLimits[i].limit);
    }
}

TEST_F(SysmanDevicePowerFixtureI915, GivenReadingSustainedPowerLimitNodeFailsWhenSetOrGetPowerLimitsIsCalledForSustainedPowerLimitThenProperErrorCodesReturned) {
    auto handles = getPowerHandles(i915PowerHandleComponentCount);

    pSysfsAccess->mockWriteUnsignedResult.push_back(ZE_RESULT_ERROR_NOT_AVAILABLE);
    pSysfsAccess->mockReadValUnsignedLongResult.push_back(ZE_RESULT_ERROR_NOT_AVAILABLE);

    for (auto handle : handles) {
        ASSERT_NE(nullptr, handle);
        zes_power_sustained_limit_t sustainedSet = {};
        zes_power_sustained_limit_t sustainedGet = {};

        EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesPowerSetLimits(handle, &sustainedSet, nullptr, nullptr));
        EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesPowerGetLimits(handle, &sustainedGet, nullptr, nullptr));
    }
}

TEST_F(SysmanDevicePowerFixtureI915, GivenValidPowerHandleAndWritingToSustainedLimitSysNodeFailsWhenCallingSetPowerLimitsExtThenProperErrorCodeIsReturned) {
    auto handles = getPowerHandles(i915PowerHandleComponentCount);

    auto handle = handles[0];
    ASSERT_NE(nullptr, handle);
    uint32_t count = maxLimitCountSupported;
    std::vector<zes_power_limit_ext_desc_t> allLimits(maxLimitCountSupported);
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesPowerGetLimitsExt(handle, &count, allLimits.data()));
    pSysfsAccess->mockWriteUnsignedResult.push_back(ZE_RESULT_ERROR_NOT_AVAILABLE);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesPowerSetLimitsExt(handle, &count, allLimits.data()));
}

TEST_F(SysmanDevicePowerFixtureI915, GivenValidPowerHandleAndWritingToSustainedLimitIntervalSysNodeFailsWhenCallingSetPowerLimitsExtThenProperErrorCodeIsReturned) {
    auto handles = getPowerHandles(i915PowerHandleComponentCount);
    pSysfsAccess->mockWriteResult = ZE_RESULT_ERROR_NOT_AVAILABLE;
    auto handle = handles[0];
    ASSERT_NE(nullptr, handle);
    uint32_t count = maxLimitCountSupported;
    std::vector<zes_power_limit_ext_desc_t> allLimits(maxLimitCountSupported);
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesPowerGetLimitsExt(handle, &count, allLimits.data()));
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesPowerSetLimitsExt(handle, &count, allLimits.data()));
}

TEST_F(SysmanDevicePowerFixtureI915, GivenValidPowerHandleAndReadingFromSustainedLimitSysNodesFailsWhenCallingGetPowerLimitsExtThenProperErrorCodesReturned) {
    auto handles = getPowerHandles(i915PowerHandleComponentCount);
    auto handle = handles[0];
    ASSERT_NE(nullptr, handle);
    uint32_t count = maxLimitCountSupported;
    std::vector<zes_power_limit_ext_desc_t> allLimits(maxLimitCountSupported);
    pSysfsAccess->mockReadValUnsignedLongResult.push_back(ZE_RESULT_ERROR_NOT_AVAILABLE);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesPowerGetLimitsExt(handle, &count, allLimits.data()));
    pSysfsAccess->mockReadValUnsignedLongResult.push_back(ZE_RESULT_SUCCESS);
    count = maxLimitCountSupported;
    pSysfsAccess->mockReadIntResult = ZE_RESULT_ERROR_NOT_AVAILABLE;
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesPowerGetLimitsExt(handle, &count, allLimits.data()));
}

TEST_F(SysmanDevicePowerFixtureI915, GivenReadingFromSysNodesFailsWhenCallingGetPowerLimitsExtThenPowerLimitCountIsZero) {
    pSysfsAccess->isSustainedPowerLimitFilePresent = false;
    pSysfsAccess->isCriticalPowerLimitFilePresent = false;

    auto handles = getPowerHandles(i915PowerHandleComponentCount);
    auto handle = handles[0];
    ASSERT_NE(nullptr, handle);
    uint32_t count = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesPowerGetLimitsExt(handle, &count, nullptr));
    EXPECT_EQ(count, 0u);
}

HWTEST2_F(SysmanDevicePowerFixtureI915, GivenValidPowerHandleAndReadingToPeakLimitSysNodesFailsWhenCallingGetPowerLimitsExtThenProperErrorCodesReturned, IsPVC) {
    auto handles = getPowerHandles(i915PowerHandleComponentCount);
    pSysfsAccess->mockReadPeakResult = ZE_RESULT_ERROR_NOT_AVAILABLE;
    auto handle = handles[0];
    ASSERT_NE(nullptr, handle);
    uint32_t count = maxLimitCountSupported;
    std::vector<zes_power_limit_ext_desc_t> allLimits(maxLimitCountSupported);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesPowerGetLimitsExt(handle, &count, allLimits.data()));
}

TEST_F(SysmanDevicePowerFixtureI915, GivenValidPowerHandleWhenSettingBurstPowerLimitThenProperErrorCodesReturned) {
    auto handles = getPowerHandles(i915PowerHandleComponentCount);
    for (auto handle : handles) {
        ASSERT_NE(nullptr, handle);
        zes_power_limit_ext_desc_t allLimits{};
        uint32_t count = 1;
        allLimits.level = ZES_POWER_LEVEL_BURST;
        EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesPowerSetLimitsExt(handle, &count, &allLimits));
    }
}

HWTEST2_F(SysmanDevicePowerFixtureI915, GivenValidPowerHandleWhenCallingGetPowerLimitsExtThenOnlySustainedLimitIsReturned, IsDG1) {
    auto handles = getPowerHandles(i915PowerHandleComponentCount);
    auto handle = handles[0];
    ASSERT_NE(nullptr, handle);
    zes_power_limit_ext_desc_t allLimits{};
    uint32_t count = 0;

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesPowerGetLimitsExt(handle, &count, nullptr));
    EXPECT_EQ(count, singleLimitCount);

    count = 1;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesPowerGetLimitsExt(handle, &count, &allLimits));
    EXPECT_EQ(count, 1u);
    ASSERT_EQ(ZES_POWER_LEVEL_SUSTAINED, allLimits.level);
    EXPECT_EQ(false, allLimits.limitValueLocked);
    EXPECT_EQ(true, allLimits.enabledStateLocked);
    EXPECT_EQ(false, allLimits.intervalValueLocked);
    EXPECT_EQ(ZES_POWER_SOURCE_ANY, allLimits.source);
    EXPECT_EQ(ZES_LIMIT_UNIT_POWER, allLimits.limitUnit);
}

HWTEST2_F(SysmanDevicePowerFixtureI915, GivenValidPowerHandleAndWritingToPeakLimitSysNodesFailsWhenCallingSetPowerLimitsExtThenProperErrorCodesReturned, IsPVC) {
    auto handles = getPowerHandles(i915PowerHandleComponentCount);

    pSysfsAccess->mockWritePeakLimitResult = ZE_RESULT_ERROR_NOT_AVAILABLE;

    auto handle = handles[0];
    ASSERT_NE(nullptr, handle);
    uint32_t count = maxLimitCountSupported;
    std::vector<zes_power_limit_ext_desc_t> allLimits(maxLimitCountSupported);
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesPowerGetLimitsExt(handle, &count, allLimits.data()));
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesPowerSetLimitsExt(handle, &count, allLimits.data()));
}

TEST_F(SysmanDevicePowerFixtureI915, GivenReadingPeakPowerLimitNodeFailsWhenSetOrGetPowerLimitsIsCalledForPeakPowerLimitThenProperErrorCodesReturned) {
    auto handles = getPowerHandles(i915PowerHandleComponentCount);

    pSysfsAccess->mockWriteUnsignedResult.push_back(ZE_RESULT_ERROR_NOT_AVAILABLE);
    pSysfsAccess->mockReadValUnsignedLongResult.push_back(ZE_RESULT_ERROR_NOT_AVAILABLE);

    for (auto handle : handles) {
        ASSERT_NE(nullptr, handle);
        zes_power_peak_limit_t peakSet = {};
        zes_power_peak_limit_t peakGet = {};

        EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesPowerSetLimits(handle, nullptr, nullptr, &peakSet));
        EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesPowerGetLimits(handle, nullptr, nullptr, &peakGet));
    }
}

TEST_F(SysmanDevicePowerFixtureI915, GivenReadingSustainedPowerNodeReturnErrorWhenGetPowerLimitsForSustainedPowerThenProperErrorCodesReturned) {
    auto handles = getPowerHandles(i915PowerHandleComponentCount);

    pSysfsAccess->mockReadValUnsignedLongResult.push_back(ZE_RESULT_ERROR_NOT_AVAILABLE);
    for (auto handle : handles) {
        ASSERT_NE(nullptr, handle);
        zes_power_sustained_limit_t sustainedGet = {};
        EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesPowerGetLimits(handle, &sustainedGet, nullptr, nullptr));
    }
}

TEST_F(SysmanDevicePowerFixtureI915, GivenReadingpeakPowerNodeReturnErrorWhenGetPowerLimitsForpeakPowerThenProperErrorCodesReturned) {
    auto handles = getPowerHandles(i915PowerHandleComponentCount);
    pSysfsAccess->mockReadValUnsignedLongResult.push_back(ZE_RESULT_ERROR_NOT_AVAILABLE);

    for (auto handle : handles) {
        ASSERT_NE(nullptr, handle);
        zes_power_peak_limit_t peakGet = {};
        EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesPowerGetLimits(handle, nullptr, nullptr, &peakGet));
    }
}

HWTEST2_F(SysmanDevicePowerFixtureI915, GivenValidPowerHandleAndPermissionsThenFirstDisableSustainedPowerLimitAndThenEnableItAndCheckSuccesIsReturned, IsXeHpOrXeHpcOrXeHpgCore) {
    auto handles = getPowerHandles(i915PowerHandleComponentCount);
    ASSERT_NE(nullptr, handles[0]);
    zes_power_sustained_limit_t sustainedSet = {};
    zes_power_sustained_limit_t sustainedGet = {};
    sustainedSet.enabled = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesPowerSetLimits(handles[0], &sustainedSet, nullptr, nullptr));
    sustainedSet.power = 300000;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesPowerSetLimits(handles[0], &sustainedSet, nullptr, nullptr));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesPowerGetLimits(handles[0], &sustainedGet, nullptr, nullptr));
    EXPECT_EQ(sustainedGet.power, sustainedSet.power);
}

TEST_F(SysmanDevicePowerFixtureI915, GivenwritingSustainedPowerNodeReturnErrorWhenSetPowerLimitsForSustainedPowerThenProperErrorCodesReturned) {
    auto handles = getPowerHandles(i915PowerHandleComponentCount);
    ASSERT_NE(nullptr, handles[0]);

    pSysfsAccess->mockWriteUnsignedResult.push_back(ZE_RESULT_ERROR_NOT_AVAILABLE);
    zes_power_sustained_limit_t sustainedSet = {};
    sustainedSet.enabled = 1;
    sustainedSet.interval = 10;
    sustainedSet.power = 300000;
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesPowerSetLimits(handles[0], &sustainedSet, nullptr, nullptr));
}

TEST_F(SysmanDevicePowerFixtureI915, GivenwritingSustainedPowerIntervalNodeFailsWhenSetPowerLimitsForSustainedPowerIntervalThenProperErrorCodesReturned) {
    auto handles = getPowerHandles(i915PowerHandleComponentCount);
    ASSERT_NE(nullptr, handles[0]);

    pSysfsAccess->mockWriteUnsignedResult.push_back(ZE_RESULT_ERROR_NOT_AVAILABLE);
    zes_power_sustained_limit_t sustainedSet = {};
    sustainedSet.enabled = 1;
    sustainedSet.interval = 10;
    sustainedSet.power = 300000;
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesPowerSetLimits(handles[0], &sustainedSet, nullptr, nullptr));
}

HWTEST2_F(SysmanDevicePowerFixtureI915, GivenValidPowerHandleWhenWritingToSustainedPowerEnableNodeWithoutPermissionsThenValidErrorIsReturned, IsXeHpOrXeHpcOrXeHpgCore) {
    auto handles = getPowerHandles(i915PowerHandleComponentCount);
    ASSERT_NE(nullptr, handles[0]);

    pSysfsAccess->mockWriteUnsignedResult.push_back(ZE_RESULT_ERROR_INSUFFICIENT_PERMISSIONS);
    zes_power_sustained_limit_t sustainedSet = {};
    sustainedSet.enabled = 0;
    EXPECT_EQ(ZE_RESULT_ERROR_INSUFFICIENT_PERMISSIONS, zesPowerSetLimits(handles[0], &sustainedSet, nullptr, nullptr));
}

TEST_F(SysmanDevicePowerFixtureI915, GivenHwmonDirectoriesArePresentAndPowerLimitFilesDontExistThenPowerModuleIsNotSupported) {
    pSysfsAccess->isSustainedPowerLimitFilePresent = false;
    pSysfsAccess->isCriticalPowerLimitFilePresent = false;
    pSysfsAccess->isEnergyCounterFilePresent = false;
    auto subDeviceCount = pLinuxSysmanImp->getSubDeviceCount();
    ze_bool_t onSubdevice = (subDeviceCount == 0) ? false : true;
    uint32_t subdeviceId = 0;
    auto pPowerImp = std::make_unique<PublicLinuxPowerImp>(pOsSysman, onSubdevice, subdeviceId, ZES_POWER_DOMAIN_CARD);
    EXPECT_FALSE(pPowerImp->isPowerModuleSupported());
    pPowerImp = std::make_unique<PublicLinuxPowerImp>(pOsSysman, onSubdevice, subdeviceId, ZES_POWER_DOMAIN_PACKAGE);
    EXPECT_FALSE(pPowerImp->isPowerModuleSupported());
}

TEST_F(SysmanDevicePowerFixtureI915, GivenHwmonDirectoriesArePresentAndAtLeastOnePowerLimitFileExistsThenPowerModuleIsSupportedForCardDomain) {
    auto subDeviceCount = pLinuxSysmanImp->getSubDeviceCount();
    ze_bool_t onSubdevice = (subDeviceCount == 0) ? false : true;
    uint32_t subdeviceId = 0;

    auto pPowerImp = std::make_unique<PublicLinuxPowerImp>(pOsSysman, onSubdevice, subdeviceId, ZES_POWER_DOMAIN_CARD);

    pSysfsAccess->isSustainedPowerLimitFilePresent = true;
    pSysfsAccess->isCriticalPowerLimitFilePresent = false;
    EXPECT_TRUE(pPowerImp->isPowerModuleSupported());

    pSysfsAccess->isSustainedPowerLimitFilePresent = false;
    pSysfsAccess->isCriticalPowerLimitFilePresent = true;
    EXPECT_TRUE(pPowerImp->isPowerModuleSupported());

    pPowerImp = std::make_unique<PublicLinuxPowerImp>(pOsSysman, onSubdevice, subdeviceId, ZES_POWER_DOMAIN_CARD);

    pSysfsAccess->isSustainedPowerLimitFilePresent = false;
    pSysfsAccess->isCriticalPowerLimitFilePresent = false;
    pSysfsAccess->isEnergyCounterFilePresent = true;
    EXPECT_TRUE(pPowerImp->isPowerModuleSupported());
}

TEST_F(SysmanDevicePowerFixtureI915, GivenValidPowerHandleWhenGettingPowerPropertiesThenCallSucceeds) {
    auto handles = getPowerHandles(i915PowerHandleComponentCount);

    auto handle = handles[0];
    ASSERT_NE(nullptr, handle);
    zes_power_properties_t properties = {};
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesPowerGetProperties(handle, &properties));
    EXPECT_FALSE(properties.onSubdevice);
    EXPECT_EQ(properties.subdeviceId, 0u);
}

TEST_F(SysmanDevicePowerFixtureI915, GivenValidPowerHandleWhenGettingPowerEnergyThresholdThenUnsupportedFeatureErrorIsReturned) {
    pSysfsAccess->mockscanDirEntriesResult.push_back(ZE_RESULT_ERROR_NOT_AVAILABLE);
    zes_energy_threshold_t threshold;
    auto handles = getPowerHandles(i915PowerHandleComponentCount);
    for (auto handle : handles) {
        ASSERT_NE(nullptr, handle);
        EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesPowerGetEnergyThreshold(handle, &threshold));
    }
}

TEST_F(SysmanDevicePowerFixtureI915, GivenValidPowerHandleWhenSettingPowerEnergyThresholdThenUnsupportedFeatureErrorIsReturned) {
    pSysfsAccess->mockscanDirEntriesResult.push_back(ZE_RESULT_ERROR_NOT_AVAILABLE);
    double threshold = 0;
    auto handles = getPowerHandles(i915PowerHandleComponentCount);
    for (auto handle : handles) {
        EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesPowerSetEnergyThreshold(handle, threshold));
    }
}

} // namespace ult
} // namespace Sysman
} // namespace L0
