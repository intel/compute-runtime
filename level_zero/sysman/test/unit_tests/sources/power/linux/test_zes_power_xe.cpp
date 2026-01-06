/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/sysman/source/api/power/sysman_power_imp.h"
#include "level_zero/sysman/test/unit_tests/sources/linux/mocks/mock_sysman_product_helper.h"
#include "level_zero/sysman/test/unit_tests/sources/power/linux/mock_sysfs_power_xe.h"

namespace L0 {
namespace Sysman {
namespace ult {

constexpr uint32_t xePowerHandleComponentCount = 2u;

TEST_F(SysmanDevicePowerFixtureXe, GivenKmdInterfaceWhenGettingSysFsFilenamesForPowerForXeVersionThenProperPathsAreReturned) {
    auto pSysmanKmdInterface = std::make_unique<SysmanKmdInterfaceXe>(pLinuxSysmanImp->getSysmanProductHelper());
    EXPECT_STREQ("energy1_input", pSysmanKmdInterface->getSysfsFilePath(SysfsName::sysfsNameCardEnergyCounterNode, 0, false).c_str());
    EXPECT_STREQ("energy2_input", pSysmanKmdInterface->getSysfsFilePath(SysfsName::sysfsNamePackageEnergyCounterNode, 0, false).c_str());
    EXPECT_STREQ("power1_max", pSysmanKmdInterface->getSysfsFilePath(SysfsName::sysfsNameCardSustainedPowerLimit, 0, false).c_str());
    EXPECT_STREQ("power1_rated_max", pSysmanKmdInterface->getSysfsFilePath(SysfsName::sysfsNameCardDefaultPowerLimit, 0, false).c_str());
    EXPECT_STREQ("power1_max_interval", pSysmanKmdInterface->getSysfsFilePath(SysfsName::sysfsNameCardSustainedPowerLimitInterval, 0, false).c_str());
    EXPECT_STREQ("power2_max", pSysmanKmdInterface->getSysfsFilePath(SysfsName::sysfsNamePackageSustainedPowerLimit, 0, false).c_str());
    EXPECT_STREQ("power2_rated_max", pSysmanKmdInterface->getSysfsFilePath(SysfsName::sysfsNamePackageDefaultPowerLimit, 0, false).c_str());
    EXPECT_STREQ("power2_max_interval", pSysmanKmdInterface->getSysfsFilePath(SysfsName::sysfsNamePackageSustainedPowerLimitInterval, 0, false).c_str());
}

TEST_F(SysmanDevicePowerFixtureXe, GivenComponentCountZeroWhenEnumeratingPowerDomainsWhenOnlySysfsNodesExistThenValidCountIsReturned) {
    uint32_t count = 0;
    EXPECT_EQ(zesDeviceEnumPowerDomains(device->toHandle(), &count, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(xePowerHandleComponentCount, count);
}

TEST_F(SysmanDevicePowerFixtureXe, GivenPowerHandleForGpuAndMemoryPowerDomainWhenGettingPowerPropertiesExtThenCorrectValuesAreReturned) {
    std::vector<zes_power_domain_t> powerDomains = {ZES_POWER_DOMAIN_GPU, ZES_POWER_DOMAIN_MEMORY};

    for (auto powerDomain : powerDomains) {
        auto pPower = std::make_unique<PowerImp>(pOsSysman, false, 0, powerDomain);
        zes_power_properties_t properties = {};
        zes_power_ext_properties_t extProperties = {};
        zes_power_limit_ext_desc_t defaultLimit = {};

        extProperties.defaultLimit = &defaultLimit;
        extProperties.stype = ZES_STRUCTURE_TYPE_POWER_EXT_PROPERTIES;
        properties.pNext = &extProperties;

        EXPECT_EQ(ZE_RESULT_SUCCESS, pPower->powerGetProperties(&properties));
        EXPECT_EQ(-1, extProperties.defaultLimit->limit);
        EXPECT_EQ(ZES_LIMIT_UNIT_POWER, extProperties.defaultLimit->limitUnit);
        EXPECT_EQ(ZES_POWER_SOURCE_ANY, extProperties.defaultLimit->source);
        EXPECT_EQ(ZES_POWER_LEVEL_UNKNOWN, extProperties.defaultLimit->level);
        EXPECT_TRUE(extProperties.defaultLimit->enabledStateLocked);
        EXPECT_TRUE(extProperties.defaultLimit->intervalValueLocked);
        EXPECT_TRUE(extProperties.defaultLimit->limitValueLocked);
    }
}

TEST_F(SysmanDevicePowerFixtureXe, GivenValidPowerHandleAndGetPropertiesFailsWhenGetPowerPropertiesIsCalledThenFailureIsReturned) {
    std::unique_ptr<XePublicLinuxPowerImp> pLinuxPowerImp = std::make_unique<XePublicLinuxPowerImp>(pOsSysman, false, 0, ZES_POWER_DOMAIN_CARD);
    pLinuxPowerImp->mockGetPropertiesFail = true;
    std::unique_ptr<L0::Sysman::PowerImp> pPowerImp = std::make_unique<L0::Sysman::PowerImp>(pOsSysman, false, 0, ZES_POWER_DOMAIN_CARD);
    L0::Sysman::OsPower *pOsPowerPrev = pPowerImp->pOsPower;
    pPowerImp->pOsPower = pLinuxPowerImp.get();
    zes_power_properties_t properties = {};
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, pPowerImp->powerGetProperties(&properties));
    pPowerImp->pOsPower = pOsPowerPrev;
}

TEST_F(SysmanDevicePowerFixtureXe, GivenValidPowerHandleAndGetPropertiesExtFailsWhenGetPowerPropertiesIsCalledThenFailureIsReturned) {
    std::unique_ptr<XePublicLinuxPowerImp> pLinuxPowerImp = std::make_unique<XePublicLinuxPowerImp>(pOsSysman, false, 0, ZES_POWER_DOMAIN_CARD);
    pLinuxPowerImp->mockGetPropertiesExtFail = true;
    std::unique_ptr<L0::Sysman::PowerImp> pPowerImp = std::make_unique<L0::Sysman::PowerImp>(pOsSysman, false, 0, ZES_POWER_DOMAIN_CARD);
    L0::Sysman::OsPower *pOsPowerPrev = pPowerImp->pOsPower;
    pPowerImp->pOsPower = pLinuxPowerImp.get();
    zes_power_properties_t properties = {};
    zes_power_ext_properties_t extProperties = {};
    zes_power_limit_ext_desc_t defaultLimit = {};
    extProperties.defaultLimit = &defaultLimit;
    extProperties.stype = ZES_STRUCTURE_TYPE_POWER_EXT_PROPERTIES;
    properties.pNext = &extProperties;
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, pPowerImp->powerGetProperties(&properties));
    pPowerImp->pOsPower = pOsPowerPrev;
}

TEST_F(SysmanDevicePowerFixtureXe, GivenDefaultLimitSysfsNodeNotAvailableWhenGettingPowerPropertiesThenDefaultPowerLimitValueIsNotValid) {
    auto handles = getPowerHandles();
    EXPECT_EQ(xePowerHandleComponentCount, handles.size());

    for (auto handle : handles) {
        ASSERT_NE(nullptr, handle);
        zes_power_properties_t properties = {};
        pSysfsAccess->defaultReadResult = ZE_RESULT_ERROR_NOT_AVAILABLE;
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesPowerGetProperties(handle, &properties));
        EXPECT_EQ(-1, properties.defaultLimit);
    }
}

TEST_F(SysmanDevicePowerFixtureXe, GivenDefaultLimitSysfsNodesNotAvailableWhenGettingPowerPropertiesExtThenDefaultPowerLimitValueIsNotValid) {
    auto handles = getPowerHandles();
    EXPECT_EQ(xePowerHandleComponentCount, handles.size());

    for (auto handle : handles) {
        ASSERT_NE(nullptr, handle);
        zes_power_properties_t properties = {};
        zes_power_ext_properties_t extProperties = {};
        zes_power_limit_ext_desc_t defaultLimit = {};

        extProperties.defaultLimit = &defaultLimit;
        extProperties.stype = ZES_STRUCTURE_TYPE_POWER_EXT_PROPERTIES;
        properties.pNext = &extProperties;

        pSysfsAccess->defaultReadResult = ZE_RESULT_ERROR_NOT_AVAILABLE;
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesPowerGetProperties(handle, &properties));
        EXPECT_EQ(-1, extProperties.defaultLimit->limit);
    }
}

TEST_F(SysmanDevicePowerFixtureXe, GivenValidPowerHandleWhenGettingPowerPropertiesAndExtPropertiesThenCallSucceeds) {
    MockSysmanProductHelper *pMockSysmanProductHelper = new MockSysmanProductHelper();
    pMockSysmanProductHelper->isPowerSetLimitSupportedResult = true;
    std::unique_ptr<SysmanProductHelper> pSysmanProductHelper(static_cast<SysmanProductHelper *>(pMockSysmanProductHelper));
    std::swap(pLinuxSysmanImp->pSysmanProductHelper, pSysmanProductHelper);

    auto handles = getPowerHandles();
    EXPECT_EQ(xePowerHandleComponentCount, handles.size());

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
        EXPECT_EQ(properties.defaultLimit, (int32_t)(xeMockDefaultPowerLimitVal / milliFactor));
        EXPECT_EQ(properties.maxLimit, (int32_t)(xeMockDefaultPowerLimitVal / milliFactor));
        EXPECT_EQ(properties.minLimit, -1);
        EXPECT_TRUE(defaultLimit.limitValueLocked);
        EXPECT_TRUE(defaultLimit.enabledStateLocked);
        EXPECT_TRUE(defaultLimit.intervalValueLocked);
        EXPECT_EQ(ZES_POWER_SOURCE_ANY, defaultLimit.source);
        EXPECT_EQ(ZES_LIMIT_UNIT_POWER, defaultLimit.limitUnit);
        EXPECT_EQ(defaultLimit.limit, (int32_t)(xeMockDefaultPowerLimitVal / milliFactor));
    }
}

TEST_F(SysmanDevicePowerFixtureXe, GivenPowerLimitSysfsNodesNotPresentWhenCallingGetPowerLimitsExtThenPowerLimitCountIsZero) {
    pSysfsAccess->isCardSustainedPowerLimitFilePresent = false;
    pSysfsAccess->isCardBurstPowerLimitFilePresent = false;
    pSysfsAccess->isCardCriticalPowerLimitFilePresent = false;
    pSysfsAccess->isPackageBurstPowerLimitFilePresent = false;
    pSysfsAccess->isPackageSustainedPowerLimitFilePresent = false;
    pSysfsAccess->isPackageCriticalPowerLimitFilePresent = false;

    auto handles = getPowerHandles();
    EXPECT_EQ(xePowerHandleComponentCount, handles.size());

    for (auto handle : handles) {
        ASSERT_NE(nullptr, handle);
        uint32_t count = 0;
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesPowerGetLimitsExt(handle, &count, nullptr));
        EXPECT_EQ(0u, count);
    }
}

} // namespace ult
} // namespace Sysman
} // namespace L0