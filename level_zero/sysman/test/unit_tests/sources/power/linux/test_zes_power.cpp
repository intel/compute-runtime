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

constexpr uint32_t powerHandleComponentCount = 1u;

TEST_F(SysmanDevicePowerFixtureI915, GivenComponentCountZeroWhenEnumeratingPowerDomainsWhenhwmonInterfaceExistsThenValidCountIsReturnedAndVerifySysmanPowerGetCallSucceeds) {
    uint32_t count = 0;
    EXPECT_EQ(zesDeviceEnumPowerDomains(device->toHandle(), &count, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(count, powerHandleComponentCount);
}

TEST_F(SysmanDevicePowerFixtureI915, GivenInvalidComponentCountWhenEnumeratingPowerDomainsWhenhwmonInterfaceExistsThenValidCountIsReturnedAndVerifySysmanPowerGetCallSucceeds) {
    uint32_t count = 0;
    EXPECT_EQ(zesDeviceEnumPowerDomains(device->toHandle(), &count, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(count, powerHandleComponentCount);

    count = count + 1;
    EXPECT_EQ(zesDeviceEnumPowerDomains(device->toHandle(), &count, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(count, powerHandleComponentCount);
}

TEST_F(SysmanDevicePowerFixtureI915, GivenComponentCountZeroWhenEnumeratingPowerDomainsWhenhwmonInterfaceExistsThenValidPowerHandlesIsReturned) {
    uint32_t count = 0;
    EXPECT_EQ(zesDeviceEnumPowerDomains(device->toHandle(), &count, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(count, powerHandleComponentCount);

    std::vector<zes_pwr_handle_t> handles(count, nullptr);
    EXPECT_EQ(zesDeviceEnumPowerDomains(device->toHandle(), &count, handles.data()), ZE_RESULT_SUCCESS);
    for (auto handle : handles) {
        EXPECT_NE(handle, nullptr);
    }
}

TEST_F(SysmanDevicePowerFixtureI915, GivenValidPowerPointerWhenGettingCardPowerDomainWhenhwmonInterfaceExistsAndThenCallSucceeds) {
    zes_pwr_handle_t phPower = {};
    EXPECT_EQ(zesDeviceGetCardPowerDomain(device->toHandle(), &phPower), ZE_RESULT_SUCCESS);
}

TEST_F(SysmanDevicePowerFixtureI915, GivenInvalidPowerPointerWhenGettingCardPowerDomainAndThenReturnsFailure) {
    EXPECT_EQ(zesDeviceGetCardPowerDomain(device->toHandle(), nullptr), ZE_RESULT_ERROR_INVALID_NULL_POINTER);
}

TEST_F(SysmanDevicePowerFixtureI915, GivenUninitializedPowerHandlesAndWhenGettingCardPowerDomainThenReturnsFailure) {
    for (const auto &handle : pSysmanDeviceImp->pPowerHandleContext->handleList) {
        delete handle;
    }
    pSysmanDeviceImp->pPowerHandleContext->handleList.clear();

    zes_pwr_handle_t phPower = {};
    EXPECT_EQ(zesDeviceGetCardPowerDomain(device->toHandle(), &phPower), ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
}

TEST_F(SysmanDevicePowerFixtureI915, GivenValidPowerHandleWhenGettingPowerPropertiesWhenhwmonInterfaceExistsThenCallSucceeds) {
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

TEST_F(SysmanDevicePowerFixtureI915, GivenValidPowerHandleWhenGettingPowerPropertiesAndExtPropertiesThenCallSucceeds) {
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
}

TEST_F(SysmanDevicePowerFixtureI915, GivenValidPowerHandleWithNoStypeForExtPropertiesWhenGettingPowerPropertiesAndExtPropertiesThenCallSucceeds) {
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

TEST_F(SysmanDevicePowerFixtureI915, GivenValidPowerHandleAndPowerSetLimitSupportedIsUnsupportedWhenCallingzesPowerGetPropertiesThenVerifyCanControlIsSetToFalse) {
    std::unique_ptr<SysmanProductHelper> pSysmanProductHelper = std::make_unique<MockSysmanProductHelper>();
    std::swap(pLinuxSysmanImp->pSysmanProductHelper, pSysmanProductHelper);

    for (const auto &handle : pSysmanDeviceImp->pPowerHandleContext->handleList) {
        delete handle;
    }
    pSysmanDeviceImp->pPowerHandleContext->handleList.clear();
    pSysmanDeviceImp->pPowerHandleContext->init(pLinuxSysmanImp->getSubDeviceCount());
    auto handles = getPowerHandles(powerHandleComponentCount);

    for (auto handle : handles) {
        ASSERT_NE(nullptr, handle);
        zes_power_properties_t properties{};
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesPowerGetProperties(handle, &properties));
        EXPECT_FALSE(properties.canControl);
    }
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
    EXPECT_EQ(count, powerHandleComponentCount);

    for (auto handle : pSysmanDeviceImp->pPowerHandleContext->handleList) {
        delete handle;
    }
    pSysmanDeviceImp->pPowerHandleContext->handleList.clear();

    pLinuxSysmanImp->reInitSysmanDeviceResources();

    count = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEnumPowerDomains(device->toHandle(), &count, NULL));
    EXPECT_EQ(count, powerHandleComponentCount);
}

HWTEST2_F(SysmanDevicePowerFixtureI915, GivenSetPowerLimitsWhenGettingPowerLimitsWhenHwmonInterfaceExistThenLimitsSetEarlierAreRetrieved, IsXeHpOrXeHpcOrXeHpgCore) {
    auto handles = getPowerHandles(powerHandleComponentCount);
    for (auto handle : handles) {
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
}

TEST_F(SysmanDevicePowerFixtureI915, GivenDefaultLimitSysfsNodesNotAvailableWhenGettingPowerPropertiesThenApiCallReturnsFailure) {
    auto handles = getPowerHandles(powerHandleComponentCount);
    for (auto handle : handles) {
        ASSERT_NE(nullptr, handle);
        zes_power_properties_t properties = {};
        pSysfsAccess->mockReadValUnsignedLongResult.push_back(ZE_RESULT_ERROR_NOT_AVAILABLE);
        EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesPowerGetProperties(handle, &properties));
    }
}

TEST_F(SysmanDevicePowerFixtureI915, GivenDefaultLimitSysfsNodesNotAvailableWhenGettingPowerPropertiesExtThenApiCallReturnsFailure) {
    auto handles = getPowerHandles(powerHandleComponentCount);
    for (auto handle : handles) {
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
}

HWTEST2_F(SysmanDevicePowerFixtureI915, GivenValidPowerHandlesWhenCallingSetAndGetPowerLimitExtThenLimitsSetEarlierAreRetrieved, IsPVC) {
    auto handles = getPowerHandles(powerHandleComponentCount);
    for (auto handle : handles) {
        ASSERT_NE(nullptr, handle);

        uint32_t limitCount = 0;
        const int32_t testLimit = 300000;
        const int32_t testInterval = 10;

        EXPECT_EQ(ZE_RESULT_SUCCESS, zesPowerGetLimitsExt(handle, &limitCount, nullptr));
        EXPECT_EQ(limitCount, mockLimitCount);

        limitCount++;
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
}

HWTEST2_F(SysmanDevicePowerFixtureI915, GivenValidPowerHandlesWhenCallingSetAndGetPowerLimitExtThenLimitsSetEarlierAreRetrieved, IsDG1) {
    auto handles = getPowerHandles(powerHandleComponentCount);
    for (auto handle : handles) {
        ASSERT_NE(nullptr, handle);

        uint32_t limitCount = 0;
        const int32_t testLimit = 300000;
        const int32_t testInterval = 10;

        EXPECT_EQ(ZE_RESULT_SUCCESS, zesPowerGetLimitsExt(handle, &limitCount, nullptr));
        EXPECT_EQ(limitCount, mockLimitCount);

        limitCount++;
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

TEST_F(SysmanDevicePowerFixtureI915, GivenReadingSustainedPowerLimitNodeReturnErrorWhenSetOrGetPowerLimitsWhenHwmonInterfaceExistForSustainedPowerLimitEnabledThenProperErrorCodesReturned) {
    auto handles = getPowerHandles(powerHandleComponentCount);

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

TEST_F(SysmanDevicePowerFixtureI915, GivenValidPowerHandleAndWritingToSustainedLimitSysNodesFailsWhenCallingSetPowerLimitsExtThenProperErrorCodesReturned) {
    auto handles = getPowerHandles(powerHandleComponentCount);

    for (auto handle : handles) {
        ASSERT_NE(nullptr, handle);
        uint32_t count = mockLimitCount;
        std::vector<zes_power_limit_ext_desc_t> allLimits(mockLimitCount);
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesPowerGetLimitsExt(handle, &count, allLimits.data()));

        pSysfsAccess->mockWriteUnsignedResult.push_back(ZE_RESULT_ERROR_NOT_AVAILABLE);
        EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesPowerSetLimitsExt(handle, &count, allLimits.data()));
    }
}

TEST_F(SysmanDevicePowerFixtureI915, GivenValidPowerHandleAndWritingToSustainedLimitIntervalSysNodeFailsWhenCallingSetPowerLimitsExtThenProperErrorCodesReturned) {
    auto handles = getPowerHandles(powerHandleComponentCount);
    pSysfsAccess->mockWriteResult = ZE_RESULT_ERROR_NOT_AVAILABLE;
    for (auto handle : handles) {
        ASSERT_NE(nullptr, handle);
        uint32_t count = mockLimitCount;
        std::vector<zes_power_limit_ext_desc_t> allLimits(mockLimitCount);
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesPowerGetLimitsExt(handle, &count, allLimits.data()));
        EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesPowerSetLimitsExt(handle, &count, allLimits.data()));
    }
}

TEST_F(SysmanDevicePowerFixtureI915, GivenValidPowerHandleAndReadingToSustainedLimitSysNodesFailsWhenCallingGetPowerLimitsExtThenProperErrorCodesReturned) {
    auto handles = getPowerHandles(powerHandleComponentCount);

    for (auto handle : handles) {
        ASSERT_NE(nullptr, handle);
        uint32_t count = mockLimitCount;
        std::vector<zes_power_limit_ext_desc_t> allLimits(mockLimitCount);
        pSysfsAccess->mockReadValUnsignedLongResult.push_back(ZE_RESULT_ERROR_NOT_AVAILABLE);
        EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesPowerGetLimitsExt(handle, &count, allLimits.data()));
        pSysfsAccess->mockReadValUnsignedLongResult.push_back(ZE_RESULT_SUCCESS);
        count = mockLimitCount;
        pSysfsAccess->mockReadIntResult = ZE_RESULT_ERROR_NOT_AVAILABLE;
        EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesPowerGetLimitsExt(handle, &count, allLimits.data()));
    }
}

TEST_F(SysmanDevicePowerFixtureI915, GivenReadingToSysNodesFailsWhenCallingGetPowerLimitsExtThenPowerLimitCountIsZero) {
    for (const auto &handle : pSysmanDeviceImp->pPowerHandleContext->handleList) {
        delete handle;
    }
    pSysfsAccess->mockReadValUnsignedLongResult.push_back(ZE_RESULT_ERROR_NOT_AVAILABLE);
    pSysfsAccess->mockReadValUnsignedLongResult.push_back(ZE_RESULT_ERROR_NOT_AVAILABLE);
    pSysmanDeviceImp->pPowerHandleContext->handleList.clear();
    pSysmanDeviceImp->pPowerHandleContext->init(pLinuxSysmanImp->getSubDeviceCount());

    auto handles = getPowerHandles(powerHandleComponentCount);
    for (auto handle : handles) {
        ASSERT_NE(nullptr, handle);
        uint32_t count = 0;
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesPowerGetLimitsExt(handle, &count, nullptr));
        EXPECT_EQ(count, 0u);
    }
}

TEST_F(SysmanDevicePowerFixtureI915, GivenValidPowerHandleAndWritingToPeakLimitSysNodesFailsWhenCallingSetPowerLimitsExtThenProperErrorCodesReturned) {
    auto handles = getPowerHandles(powerHandleComponentCount);

    pSysfsAccess->mockWritePeakLimitResult = ZE_RESULT_ERROR_NOT_AVAILABLE;

    for (auto handle : handles) {
        ASSERT_NE(nullptr, handle);
        uint32_t count = mockLimitCount;
        std::vector<zes_power_limit_ext_desc_t> allLimits(mockLimitCount);
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesPowerGetLimitsExt(handle, &count, allLimits.data()));

        EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesPowerSetLimitsExt(handle, &count, allLimits.data()));
    }
}

TEST_F(SysmanDevicePowerFixtureI915, GivenValidPowerHandleAndReadingToPeakLimitSysNodesFailsWhenCallingGetPowerLimitsExtThenProperErrorCodesReturned) {
    auto handles = getPowerHandles(powerHandleComponentCount);

    pSysfsAccess->mockReadPeakResult = ZE_RESULT_ERROR_NOT_AVAILABLE;

    for (auto handle : handles) {
        ASSERT_NE(nullptr, handle);
        uint32_t count = mockLimitCount;
        std::vector<zes_power_limit_ext_desc_t> allLimits(mockLimitCount);
        EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesPowerGetLimitsExt(handle, &count, allLimits.data()));
    }
}

TEST_F(SysmanDevicePowerFixtureI915, GivenValidPowerHandleWhenSettingBurstPowerLimitThenProperErrorCodesReturned) {
    auto handles = getPowerHandles(powerHandleComponentCount);

    for (auto handle : handles) {
        ASSERT_NE(nullptr, handle);
        zes_power_limit_ext_desc_t allLimits{};
        uint32_t count = 1;
        allLimits.level = ZES_POWER_LEVEL_BURST;

        EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesPowerSetLimitsExt(handle, &count, &allLimits));
    }
}

TEST_F(SysmanDevicePowerFixtureI915, GivenValidPowerHandleWhenCallingGetPowerLimitsExtThenProperValuesAreReturned) {
    auto handles = getPowerHandles(powerHandleComponentCount);

    for (auto handle : handles) {
        ASSERT_NE(nullptr, handle);
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

HWTEST2_F(SysmanDevicePowerFixtureI915, GivenValidPowerHandleAndWritingToPeakLimitSysNodesFailsWhenCallingSetPowerLimitsExtThenProperErrorCodesReturned, IsPVC) {
    auto handles = getPowerHandles(powerHandleComponentCount);

    pSysfsAccess->mockWritePeakLimitResult = ZE_RESULT_ERROR_NOT_AVAILABLE;

    for (auto handle : handles) {
        ASSERT_NE(nullptr, handle);
        uint32_t count = mockLimitCount;
        std::vector<zes_power_limit_ext_desc_t> allLimits(mockLimitCount);
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesPowerGetLimitsExt(handle, &count, allLimits.data()));

        EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesPowerSetLimitsExt(handle, &count, allLimits.data()));
    }
}

HWTEST2_F(SysmanDevicePowerFixtureI915, GivenValidPowerHandleAndWritingToPeakLimitSysNodesFailsWhenCallingSetPowerLimitsExtThenProperErrorCodesReturned, IsDG1) {
    auto handles = getPowerHandles(powerHandleComponentCount);

    pSysfsAccess->mockWritePeakLimitResult = ZE_RESULT_ERROR_NOT_AVAILABLE;

    for (auto handle : handles) {
        ASSERT_NE(nullptr, handle);
        uint32_t count = mockLimitCount;
        std::vector<zes_power_limit_ext_desc_t> allLimits(mockLimitCount);
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesPowerGetLimitsExt(handle, &count, allLimits.data()));

        EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesPowerSetLimitsExt(handle, &count, allLimits.data()));
    }
}

TEST_F(SysmanDevicePowerFixtureI915, GivenReadingPeakPowerLimitNodeReturnErrorWhenSetOrGetPowerLimitsWhenHwmonInterfaceExistForPeakPowerLimitEnabledThenProperErrorCodesReturned) {
    auto handles = getPowerHandles(powerHandleComponentCount);

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

TEST_F(SysmanDevicePowerFixtureI915, GivenReadingSustainedPowerNodeReturnErrorWhenGetPowerLimitsForSustainedPowerWhenHwmonInterfaceExistThenProperErrorCodesReturned) {
    auto handles = getPowerHandles(powerHandleComponentCount);

    pSysfsAccess->mockReadValUnsignedLongResult.push_back(ZE_RESULT_ERROR_NOT_AVAILABLE);
    for (auto handle : handles) {
        ASSERT_NE(nullptr, handle);
        zes_power_sustained_limit_t sustainedGet = {};
        EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesPowerGetLimits(handle, &sustainedGet, nullptr, nullptr));
    }
}

TEST_F(SysmanDevicePowerFixtureI915, GivenReadingpeakPowerNodeReturnErrorWhenGetPowerLimitsForpeakPowerWhenHwmonInterfaceExistThenProperErrorCodesReturned) {
    auto handles = getPowerHandles(powerHandleComponentCount);
    pSysfsAccess->mockReadValUnsignedLongResult.push_back(ZE_RESULT_ERROR_NOT_AVAILABLE);

    for (auto handle : handles) {
        ASSERT_NE(nullptr, handle);
        zes_power_peak_limit_t peakGet = {};
        EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesPowerGetLimits(handle, nullptr, nullptr, &peakGet));
    }
}

HWTEST2_F(SysmanDevicePowerFixtureI915, GivenValidPowerHandleAndPermissionsThenFirstDisableSustainedPowerLimitAndThenEnableItAndCheckSuccesIsReturned, IsXeHpOrXeHpcOrXeHpgCore) {
    auto handles = getPowerHandles(powerHandleComponentCount);
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

TEST_F(SysmanDevicePowerFixtureI915, GivenwritingSustainedPowerNodeReturnErrorWhenSetPowerLimitsForSustainedPowerWhenHwmonInterfaceExistThenProperErrorCodesReturned) {
    auto handles = getPowerHandles(powerHandleComponentCount);
    ASSERT_NE(nullptr, handles[0]);

    pSysfsAccess->mockWriteUnsignedResult.push_back(ZE_RESULT_ERROR_NOT_AVAILABLE);
    zes_power_sustained_limit_t sustainedSet = {};
    sustainedSet.enabled = 1;
    sustainedSet.interval = 10;
    sustainedSet.power = 300000;
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesPowerSetLimits(handles[0], &sustainedSet, nullptr, nullptr));
}

TEST_F(SysmanDevicePowerFixtureI915, GivenwritingSustainedPowerIntervalNodeReturnErrorWhenSetPowerLimitsForSustainedPowerIntervalWhenHwmonInterfaceExistThenProperErrorCodesReturned) {
    auto handles = getPowerHandles(powerHandleComponentCount);
    ASSERT_NE(nullptr, handles[0]);

    pSysfsAccess->mockWriteUnsignedResult.push_back(ZE_RESULT_ERROR_NOT_AVAILABLE);
    zes_power_sustained_limit_t sustainedSet = {};
    sustainedSet.enabled = 1;
    sustainedSet.interval = 10;
    sustainedSet.power = 300000;
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesPowerSetLimits(handles[0], &sustainedSet, nullptr, nullptr));
}

HWTEST2_F(SysmanDevicePowerFixtureI915, GivenValidPowerHandleWhenWritingToSustainedPowerEnableNodeWithoutPermissionsThenValidErrorIsReturned, IsXeHpOrXeHpcOrXeHpgCore) {
    auto handles = getPowerHandles(powerHandleComponentCount);
    ASSERT_NE(nullptr, handles[0]);

    pSysfsAccess->mockWriteUnsignedResult.push_back(ZE_RESULT_ERROR_INSUFFICIENT_PERMISSIONS);
    zes_power_sustained_limit_t sustainedSet = {};
    sustainedSet.enabled = 0;
    EXPECT_EQ(ZE_RESULT_ERROR_INSUFFICIENT_PERMISSIONS, zesPowerSetLimits(handles[0], &sustainedSet, nullptr, nullptr));
}

TEST_F(SysmanDevicePowerFixtureI915, GivenValidPowerHandleWhenGettingPowerPropertiesThenCallSucceeds) {

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
    }
}

TEST_F(SysmanDevicePowerFixtureI915, GivenHwMonDoesNotExistAndTelemDataNotAvailableWhenGettingPowerEnergyCounterThenFailureIsReturned) {
    auto handles = getPowerHandles(powerHandleComponentCount);
    for (auto handle : handles) {
        pSysfsAccess->mockReadValUnsignedLongResult.push_back(ZE_RESULT_ERROR_NOT_AVAILABLE);
        ASSERT_NE(nullptr, handle);
        zes_power_energy_counter_t energyCounter = {};
        EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, zesPowerGetEnergyCounter(handle, &energyCounter));
    }
}

TEST_F(SysmanDevicePowerFixtureI915, GivenValidPowerHandleWhenGettingPowerEnergyThresholdThenUnsupportedFeatureErrorIsReturned) {

    zes_energy_threshold_t threshold;
    auto handles = getPowerHandles(powerHandleComponentCount);
    for (auto handle : handles) {
        ASSERT_NE(nullptr, handle);
        EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesPowerGetEnergyThreshold(handle, &threshold));
    }
}

TEST_F(SysmanDevicePowerFixtureI915, GivenValidPowerHandleWhenSettingPowerEnergyThresholdThenUnsupportedFeatureErrorIsReturned) {

    double threshold = 0;
    auto handles = getPowerHandles(powerHandleComponentCount);
    for (auto handle : handles) {
        EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesPowerSetEnergyThreshold(handle, threshold));
    }
}

TEST_F(SysmanDevicePowerFixtureXe, GivenKmdInterfaceWhenGettingSysFsFilenamesForPowerForXeVersionThenProperPathsAreReturned) {
    auto pSysmanKmdInterface = std::make_unique<SysmanKmdInterfaceXe>(pLinuxSysmanImp->getSysmanProductHelper());
    EXPECT_STREQ("energy1_input", pSysmanKmdInterface->getSysfsFilePath(SysfsName::sysfsNameEnergyCounterNode, 0, false).c_str());
    EXPECT_STREQ("power1_rated_max", pSysmanKmdInterface->getSysfsFilePath(SysfsName::sysfsNameDefaultPowerLimit, 0, false).c_str());
    EXPECT_STREQ("power1_max", pSysmanKmdInterface->getSysfsFilePath(SysfsName::sysfsNameSustainedPowerLimit, 0, false).c_str());
    EXPECT_STREQ("power1_max_interval", pSysmanKmdInterface->getSysfsFilePath(SysfsName::sysfsNameSustainedPowerLimitInterval, 0, false).c_str());
}

} // namespace ult
} // namespace Sysman
} // namespace L0
