/*
 * Copyright (C) 2020-2025 Intel Corporation
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

TEST_F(SysmanDevicePowerFixtureI915, GivenValidPowerPointerWhenGetCardPowerDomainIsCalledThenFailureIsReturned) {
    zes_pwr_handle_t phPower = {};
    EXPECT_EQ(zesDeviceGetCardPowerDomain(device->toHandle(), &phPower), ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
}

TEST_F(SysmanDevicePowerFixtureI915, GivenInvalidPowerPointerWhenGetCardPowerDomainIsCalledThenFailureIsReturned) {
    EXPECT_EQ(zesDeviceGetCardPowerDomain(device->toHandle(), nullptr), ZE_RESULT_ERROR_INVALID_NULL_POINTER);
}

TEST_F(SysmanDevicePowerFixtureI915, GivenPowerHandlesListContainsCardDomainHandleWhenGetCardPowerDomainIsCalledThenCallSucceeds) {
    Power *pPower = new PowerImp(pOsSysman, false, 0, ZES_POWER_DOMAIN_CARD);

    pSysmanDeviceImp->pPowerHandleContext->handleList.push_back(pPower);

    zes_pwr_handle_t phPower = {};
    EXPECT_EQ(zesDeviceGetCardPowerDomain(device->toHandle(), &phPower), ZE_RESULT_SUCCESS);
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

TEST_F(SysmanDevicePowerFixtureI915, GivenPowerHandleWithUnknownPowerDomainWhenIsPowerModuleSupportedIsCalledThenFalseValueIsReturned) {
    uint32_t subdeviceId = 0;
    ze_bool_t onSubdevice = false;
    auto pPowerImp = std::make_unique<PublicLinuxPowerImp>(pOsSysman, onSubdevice, subdeviceId, ZES_POWER_DOMAIN_UNKNOWN);
    pPowerImp->pSysfsAccess = pSysfsAccess;
    EXPECT_FALSE(pPowerImp->isPowerModuleSupported());
}

TEST_F(SysmanDevicePowerFixtureI915, GivenTelemetrySupportAndEnergyCounterNodeExistanceStatesWhenIsPowerModuleSupportedIsCalledForSubdeviceHandleThenCorrectSupportStatusIsReturnedForPackageDomain) {
    // Loop through all combinations of the three boolean flags (false/true) for the file existence
    for (bool isPackageEnergyCounterFilePresent : {false, true}) {
        for (bool isTelemetrySupportAvailable : {false, true}) {
            // Set the file existence flags based on the current combination
            pSysfsAccess->isEnergyCounterFilePresent = isPackageEnergyCounterFilePresent;
            auto pPowerImp = std::make_unique<PublicLinuxPowerImp>(pOsSysman, true, 0, ZES_POWER_DOMAIN_PACKAGE);
            pPowerImp->isTelemetrySupportAvailable = isTelemetrySupportAvailable;
            // The expected result is true if at least one of the path is present
            bool expected = (isTelemetrySupportAvailable || isPackageEnergyCounterFilePresent);

            // Verify if the power module is supported as expected
            EXPECT_EQ(pPowerImp->isPowerModuleSupported(), expected);
        }
    }
}

TEST_F(SysmanDevicePowerFixtureI915, GivenVariousPowerLimitFileExistanceStatesWhenIsPowerModuleSupportedIsCalledForRootDeviceHandleThenCorrectSupportStatusIsReturnedForPackageDomain) {
    // Loop through all combinations of the three boolean flags (false/true) for the file existence
    for (bool isPackageEnergyCounterFilePresent : {false, true}) {
        for (bool isTelemetrySupportAvailable : {false, true}) {
            for (bool isPackagedSustainedPowerLimitFilePresent : {false, true}) {
                for (bool isPackageCriticalPowerLimit2Present : {false, true}) {
                    // Set the file existence flags based on the current combination
                    pSysfsAccess->isEnergyCounterFilePresent = isPackageEnergyCounterFilePresent;
                    pSysfsAccess->isSustainedPowerLimitFilePresent = isPackagedSustainedPowerLimitFilePresent;
                    pSysfsAccess->isCriticalPowerLimitFilePresent = isPackageCriticalPowerLimit2Present;

                    auto pPowerImp = std::make_unique<PublicLinuxPowerImp>(pOsSysman, false, 0, ZES_POWER_DOMAIN_PACKAGE);
                    pPowerImp->isTelemetrySupportAvailable = isTelemetrySupportAvailable;
                    // The expected result is true if at least one of the files is present
                    bool expected = (isTelemetrySupportAvailable || isPackageEnergyCounterFilePresent ||
                                     isPackagedSustainedPowerLimitFilePresent || isPackageCriticalPowerLimit2Present);

                    // Verify if the power module is supported as expected
                    EXPECT_EQ(pPowerImp->isPowerModuleSupported(), expected);
                }
            }
        }
    }
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
        EXPECT_EQ(extProperties.domain, ZES_POWER_DOMAIN_PACKAGE);
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

TEST_F(SysmanDevicePowerFixtureI915, GivenValidPowerHandleWhenGettingPowerPropertiesAndSysfsReadFailsThenDefaultPowerLimitValueIsNotValid) {
    std::unique_ptr<PublicLinuxPowerImp> pLinuxPowerImp(new PublicLinuxPowerImp(pOsSysman, false, 0, ZES_POWER_DOMAIN_PACKAGE));
    EXPECT_TRUE(pLinuxPowerImp->isPowerModuleSupported());
    pSysfsAccess->mockReadValUnsignedLongResult.push_back(ZE_RESULT_ERROR_NOT_AVAILABLE);
    zes_power_properties_t properties{};
    EXPECT_EQ(ZE_RESULT_SUCCESS, pLinuxPowerImp->getProperties(&properties));
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

TEST_F(SysmanDevicePowerFixtureI915, GivenDefaultLimitSysfsNodesNotAvailableWhenGettingPowerPropertiesThenDefaultPowerLimitValueIsNotValid) {
    auto handles = getPowerHandles(powerHandleComponentCount);
    for (auto handle : handles) {
        ASSERT_NE(nullptr, handle);
        zes_power_properties_t properties = {};
        pSysfsAccess->mockReadValUnsignedLongResult.push_back(ZE_RESULT_ERROR_NOT_AVAILABLE);
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesPowerGetProperties(handle, &properties));
        EXPECT_EQ(-1, properties.defaultLimit);
    }
}

TEST_F(SysmanDevicePowerFixtureI915, GivenDefaultLimitSysfsNodesNotAvailableWhenGettingPowerPropertiesExtThenDefaultPowerLimitValueIsNotValid) {
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
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesPowerGetProperties(handle, &properties));
        EXPECT_EQ(-1, extProperties.defaultLimit->limit);
    }
}

TEST_F(SysmanDevicePowerFixtureI915, GivenReadingAndWritingToSustainedPowerLimitNodeReturnsErrorWhenGetOrSetPowerLimitsIsCalledThenProperErrorCodesReturned) {
    auto handles = getPowerHandles(powerHandleComponentCount);

    pSysfsAccess->mockWriteUnsignedResult.push_back(ZE_RESULT_ERROR_NOT_AVAILABLE);
    pSysfsAccess->mockReadValUnsignedLongResult.push_back(ZE_RESULT_ERROR_NOT_AVAILABLE);

    for (auto handle : handles) {
        ASSERT_NE(nullptr, handle);
        zes_power_sustained_limit_t sustainedSet = {};
        zes_power_sustained_limit_t sustainedGet = {};

        EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesPowerGetLimits(handle, &sustainedGet, nullptr, nullptr));
        EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesPowerSetLimits(handle, &sustainedSet, nullptr, nullptr));
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
    }
}

TEST_F(SysmanDevicePowerFixtureI915, GivenReadingToSysNodesFailsWhenCallingGetPowerLimitsExtThenPowerLimitCountIsZero) {
    for (const auto &handle : pSysmanDeviceImp->pPowerHandleContext->handleList) {
        delete handle;
    }
    pSysfsAccess->isSustainedPowerLimitFilePresent = false;
    pSysfsAccess->isCriticalPowerLimitFilePresent = false;
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

TEST_F(SysmanDevicePowerFixtureI915, GivenValidPowerHandleAndPeakPowerLimitFileDoesNotExistWhenCallingGetPowerLimitsExtThenOnlySustainedLimitIsReturned) {
    pSysfsAccess->isCriticalPowerLimitFilePresent = false;
    auto handles = getPowerHandles(powerHandleComponentCount);

    for (auto handle : handles) {
        ASSERT_NE(nullptr, handle);
        zes_power_limit_ext_desc_t allLimits{};
        uint32_t count = 0;

        EXPECT_EQ(ZE_RESULT_SUCCESS, zesPowerGetLimitsExt(handle, &count, nullptr));
        EXPECT_EQ(count, 1u);

        EXPECT_EQ(ZE_RESULT_SUCCESS, zesPowerGetLimitsExt(handle, &count, &allLimits));
        EXPECT_EQ(ZES_POWER_LEVEL_SUSTAINED, allLimits.level);
    }
}

TEST_F(SysmanDevicePowerFixtureI915, GivenValidPowerHandleAndSustainedPowerLimitFileDoesNotExistWhenCallingGetPowerLimitsExtThenOnlyCriticalLimitIsReturned) {
    pSysfsAccess->isSustainedPowerLimitFilePresent = false;
    auto handles = getPowerHandles(powerHandleComponentCount);

    for (auto handle : handles) {
        ASSERT_NE(nullptr, handle);
        zes_power_limit_ext_desc_t allLimits{};
        uint32_t count = 0;

        EXPECT_EQ(ZE_RESULT_SUCCESS, zesPowerGetLimitsExt(handle, &count, nullptr));
        EXPECT_EQ(count, 1u);

        EXPECT_EQ(ZE_RESULT_SUCCESS, zesPowerGetLimitsExt(handle, &count, &allLimits));
        EXPECT_EQ(ZES_POWER_LEVEL_PEAK, allLimits.level);
    }
}

TEST_F(SysmanDevicePowerFixtureI915, GivenValidPowerHandleWhenCallingGetPowerLimitsExtThenProperValuesAreReturned) {
    auto handles = getPowerHandles(powerHandleComponentCount);

    for (auto handle : handles) {
        ASSERT_NE(nullptr, handle);
        uint32_t limitCount = 0;

        EXPECT_EQ(ZE_RESULT_SUCCESS, zesPowerGetLimitsExt(handle, &limitCount, nullptr));
        EXPECT_EQ(limitCount, mockLimitCount);

        std::vector<zes_power_limit_ext_desc_t> allLimits(limitCount);
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesPowerGetLimitsExt(handle, &limitCount, allLimits.data()));
        EXPECT_EQ(ZES_POWER_LEVEL_SUSTAINED, allLimits[0].level);
        EXPECT_EQ(ZES_POWER_LEVEL_PEAK, allLimits[1].level);
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

TEST_F(SysmanDevicePowerFixtureI915, GivenReadingPeakPowerNodeReturnErrorWhenGetPowerLimitsForpeakPowerWhenHwmonInterfaceExistThenProperErrorCodesReturned) {
    auto handles = getPowerHandles(powerHandleComponentCount);
    pSysfsAccess->mockReadValUnsignedLongResult.push_back(ZE_RESULT_ERROR_NOT_AVAILABLE);

    for (auto handle : handles) {
        ASSERT_NE(nullptr, handle);
        zes_power_peak_limit_t peakGet = {};
        EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesPowerGetLimits(handle, nullptr, nullptr, &peakGet));
    }
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

TEST_F(SysmanDevicePowerFixtureI915, GivenValidPowerHandlesWithTelemetrySupportNotAvailableButSysfsReadSucceedsWhenGettingPowerEnergyCounterThenValidPowerReadingsRetrievedFromSysfsNode) {
    zes_power_energy_counter_t energyCounter = {};
    std::unique_ptr<PublicLinuxPowerImp> pLinuxPowerImp(new PublicLinuxPowerImp(pOsSysman, false, 0, ZES_POWER_DOMAIN_PACKAGE));
    pLinuxPowerImp->isTelemetrySupportAvailable = false;
    EXPECT_EQ(ZE_RESULT_SUCCESS, pLinuxPowerImp->getEnergyCounter(&energyCounter));
    EXPECT_EQ(energyCounter.energy, expectedEnergyCounter);
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

} // namespace ult
} // namespace Sysman
} // namespace L0
