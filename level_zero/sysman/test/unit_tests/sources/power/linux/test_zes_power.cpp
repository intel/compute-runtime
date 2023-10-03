/*
 * Copyright (C) 2020-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/sysman/test/unit_tests/sources/power/linux/mock_sysfs_power.h"

namespace L0 {
namespace Sysman {
namespace ult {

constexpr uint64_t convertJouleToMicroJoule = 1000000u;
constexpr uint32_t powerHandleComponentCount = 1u;

ssize_t preadMockPower(int fd, void *buf, size_t count, off_t offset) {
    uint64_t *mockBuf = static_cast<uint64_t *>(buf);
    *mockBuf = setEnergyCounter;
    return count;
}

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

    auto pSysFsAccess = std::make_unique<MockPowerSysfsAccessInterface>();
    std::unique_ptr<PublicLinuxPowerImp> pLinuxPowerImp(new PublicLinuxPowerImp(pOsSysman, false, 0));
    pLinuxPowerImp->pSysfsAccess = pSysFsAccess.get();
    pLinuxPowerImp->pPmt = static_cast<MockPowerPmt *>(pLinuxSysmanImp->getPlatformMonitoringTechAccess(0));
    pLinuxPowerImp->isPowerModuleSupported();

    zes_power_properties_t properties{};
    EXPECT_EQ(ZE_RESULT_SUCCESS, pLinuxPowerImp->getProperties(&properties));
    EXPECT_FALSE(properties.onSubdevice);
    EXPECT_EQ(properties.subdeviceId, 0u);
    EXPECT_EQ(properties.canControl, true);
    EXPECT_EQ(properties.isEnergyThresholdSupported, false);
    EXPECT_EQ(properties.defaultLimit, static_cast<int32_t>(mockDefaultPowerLimitVal / milliFactor));
    EXPECT_EQ(properties.maxLimit, static_cast<int32_t>(mockMaxPowerLimitVal / milliFactor));
    EXPECT_EQ(properties.minLimit, static_cast<int32_t>(mockMinPowerLimitVal / milliFactor));
}

TEST_F(SysmanDevicePowerFixtureI915, GivenValidPowerHandleWhenGettingPowerPropertiesWhenHwmonInterfaceExistThenLimitsReturnsUnknown) {

    auto pSysFsAccess = std::make_unique<MockPowerSysfsAccessInterface>();
    pSysFsAccess->mockReadReturnStatus.push_back(ZE_RESULT_ERROR_NOT_AVAILABLE);
    pSysFsAccess->isRepeated = true;
    std::unique_ptr<PublicLinuxPowerImp> pLinuxPowerImp(new PublicLinuxPowerImp(pOsSysman, false, 0));
    pLinuxPowerImp->pSysfsAccess = pSysFsAccess.get();
    pLinuxPowerImp->isPowerModuleSupported();

    zes_power_properties_t properties{};
    EXPECT_EQ(ZE_RESULT_SUCCESS, pLinuxPowerImp->getProperties(&properties));
    EXPECT_FALSE(properties.onSubdevice);
    EXPECT_EQ(properties.subdeviceId, 0u);
    EXPECT_EQ(properties.defaultLimit, -1);
    EXPECT_EQ(properties.maxLimit, -1);
    EXPECT_EQ(properties.minLimit, -1);
}

TEST_F(SysmanDevicePowerFixtureI915, GivenValidPowerHandleWhenGettingPowerPropertiesWhenHwmonInterfaceExistThenMaxLimitIsUnsupported) {

    pSysfsAccess->mockReadReturnStatus.push_back(ZE_RESULT_ERROR_NOT_AVAILABLE);
    pSysfsAccess->mockReadUnsignedIntValue.push_back(std::numeric_limits<uint32_t>::max());

    pSysfsAccess->mockReadReturnStatus.push_back(ZE_RESULT_ERROR_NOT_AVAILABLE);
    pSysfsAccess->mockReadUnsignedIntValue.push_back(std::numeric_limits<uint32_t>::max());

    pSysfsAccess->mockReadReturnStatus.push_back(ZE_RESULT_SUCCESS);
    pSysfsAccess->mockReadUnsignedIntValue.push_back(std::numeric_limits<uint32_t>::max());

    for (const auto &handle : pSysmanDeviceImp->pPowerHandleContext->handleList) {
        delete handle;
    }
    pSysmanDeviceImp->pPowerHandleContext->handleList.clear();
    pSysmanDeviceImp->pPowerHandleContext->init(pLinuxSysmanImp->getSubDeviceCount());
    auto handles = getPowerHandles(powerHandleComponentCount);
    for (auto &handle : handles) {
        zes_power_properties_t properties{};
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesPowerGetProperties(handle, &properties));
        EXPECT_FALSE(properties.onSubdevice);
        EXPECT_EQ(properties.subdeviceId, 0u);
        EXPECT_EQ(properties.defaultLimit, -1);
        EXPECT_EQ(properties.maxLimit, -1);
        EXPECT_EQ(properties.minLimit, -1);
    }
}

TEST_F(SysmanDevicePowerFixtureI915, GivenValidPowerHandleWhenGettingPowerPropertiesThenHwmonInterfaceExistAndMinLimitIsUnknown) {

    auto pSysFsAccess = std::make_unique<MockPowerSysfsAccessInterface>();

    pSysFsAccess->mockReadReturnStatus.push_back(ZE_RESULT_ERROR_NOT_AVAILABLE);
    pSysFsAccess->mockReadUnsignedIntValue.push_back(0);
    pSysFsAccess->mockReadReturnStatus.push_back(ZE_RESULT_SUCCESS);
    pSysFsAccess->mockReadUnsignedIntValue.push_back(0);

    std::unique_ptr<PublicLinuxPowerImp> pLinuxPowerImp(new PublicLinuxPowerImp(pOsSysman, false, 0));
    pLinuxPowerImp->pSysfsAccess = pSysFsAccess.get();
    pLinuxPowerImp->pPmt = static_cast<MockPowerPmt *>(pLinuxSysmanImp->getPlatformMonitoringTechAccess(0));
    pLinuxPowerImp->isPowerModuleSupported();

    zes_power_properties_t properties{};
    EXPECT_EQ(ZE_RESULT_SUCCESS, pLinuxPowerImp->getProperties(&properties));
    EXPECT_FALSE(properties.onSubdevice);
    EXPECT_EQ(properties.subdeviceId, 0u);
    EXPECT_EQ(properties.defaultLimit, -1);
    EXPECT_EQ(properties.maxLimit, static_cast<int32_t>(mockMaxPowerLimitVal / milliFactor));
    EXPECT_EQ(properties.minLimit, -1);
}

TEST_F(SysmanDevicePowerFixtureI915, GivenValidPowerHandleWhenGettingPowerEnergyCounterFailedWhenHwmonInterfaceExistThenValidErrorCodeReturned) {

    auto pSysFsAccess = std::make_unique<MockPowerSysfsAccessInterface>();
    pSysFsAccess->mockReadReturnStatus.push_back(ZE_RESULT_ERROR_INSUFFICIENT_PERMISSIONS);
    pSysFsAccess->isRepeated = true;

    auto pPmt = static_cast<MockPowerPmt *>(pLinuxSysmanImp->getPlatformMonitoringTechAccess(0));

    pPmt->preadFunction = preadMockPower;

    std::unique_ptr<PublicLinuxPowerImp> pLinuxPowerImp(new PublicLinuxPowerImp(pOsSysman, false, 0));
    pLinuxPowerImp->pSysfsAccess = pSysFsAccess.get();
    pLinuxPowerImp->pPmt = pPmt;
    pLinuxPowerImp->isPowerModuleSupported();

    zes_power_energy_counter_t energyCounter = {};
    uint64_t expectedEnergyCounter = convertJouleToMicroJoule * (setEnergyCounter / 1048576);
    ASSERT_EQ(ZE_RESULT_SUCCESS, pLinuxPowerImp->getEnergyCounter(&energyCounter));
    EXPECT_EQ(energyCounter.energy, expectedEnergyCounter);
}

TEST_F(SysmanDevicePowerFixtureI915, GivenSetPowerLimitsWhenGettingPowerLimitsWhenHwmonInterfaceExistThenLimitsSetEarlierAreRetrieved) {

    auto pSysFsAccess = std::make_unique<MockPowerSysfsAccessInterface>();
    std::unique_ptr<PublicLinuxPowerImp> pLinuxPowerImp(new PublicLinuxPowerImp(pOsSysman, false, 0));
    pLinuxPowerImp->pSysfsAccess = pSysFsAccess.get();
    pLinuxPowerImp->pPmt = static_cast<MockPowerPmt *>(pLinuxSysmanImp->getPlatformMonitoringTechAccess(0));
    pLinuxPowerImp->isPowerModuleSupported();
    zes_power_sustained_limit_t sustainedSet = {};
    zes_power_sustained_limit_t sustainedGet = {};
    sustainedSet.enabled = 1;
    sustainedSet.interval = 10;
    sustainedSet.power = 300000;

    EXPECT_EQ(ZE_RESULT_SUCCESS, pLinuxPowerImp->setLimits(&sustainedSet, nullptr, nullptr));
    EXPECT_EQ(ZE_RESULT_SUCCESS, pLinuxPowerImp->getLimits(&sustainedGet, nullptr, nullptr));
    EXPECT_EQ(sustainedGet.power, sustainedSet.power);
    EXPECT_EQ(sustainedGet.interval, sustainedSet.interval);

    zes_power_burst_limit_t burstSet = {};
    zes_power_burst_limit_t burstGet = {};
    burstSet.enabled = 1;
    burstSet.power = 375000;
    EXPECT_EQ(ZE_RESULT_SUCCESS, pLinuxPowerImp->setLimits(nullptr, &burstSet, nullptr));
    EXPECT_EQ(ZE_RESULT_SUCCESS, pLinuxPowerImp->getLimits(nullptr, &burstGet, nullptr));
    EXPECT_EQ(burstSet.enabled, burstGet.enabled);
    EXPECT_EQ(burstSet.power, burstGet.power);

    burstSet.enabled = 0;
    burstGet.enabled = 0;
    burstGet.power = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, pLinuxPowerImp->setLimits(nullptr, &burstSet, nullptr));
    EXPECT_EQ(ZE_RESULT_SUCCESS, pLinuxPowerImp->getLimits(nullptr, &burstGet, nullptr));
    EXPECT_EQ(burstSet.enabled, burstGet.enabled);
    EXPECT_EQ(burstGet.power, 0);

    zes_power_peak_limit_t peakGet = {};
    EXPECT_EQ(ZE_RESULT_SUCCESS, pLinuxPowerImp->getLimits(nullptr, nullptr, &peakGet));
    EXPECT_EQ(peakGet.powerAC, -1);
    EXPECT_EQ(peakGet.powerDC, -1);
}

TEST_F(SysmanDevicePowerFixtureI915, GivenGetPowerLimitsReturnErrorWhenGettingPowerLimitsWhenHwmonInterfaceExistForBurstPowerLimitThenProperErrorCodesReturned) {
    auto pSysFsAccess = std::make_unique<MockPowerSysfsAccessInterface>();
    pSysFsAccess->mockReadReturnStatus.push_back(ZE_RESULT_ERROR_NOT_AVAILABLE);
    std::unique_ptr<PublicLinuxPowerImp> pLinuxPowerImp(new PublicLinuxPowerImp(pOsSysman, false, 0));
    pLinuxPowerImp->pSysfsAccess = pSysFsAccess.get();
    pLinuxPowerImp->pPmt = static_cast<MockPowerPmt *>(pLinuxSysmanImp->getPlatformMonitoringTechAccess(0));
    pLinuxPowerImp->isPowerModuleSupported();

    zes_power_burst_limit_t burstSet = {};
    zes_power_burst_limit_t burstGet = {};
    burstSet.enabled = 1;
    burstSet.power = 375000;
    EXPECT_EQ(ZE_RESULT_SUCCESS, pLinuxPowerImp->setLimits(nullptr, &burstSet, nullptr));
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, pLinuxPowerImp->getLimits(nullptr, &burstGet, nullptr));
}

TEST_F(SysmanDevicePowerFixtureI915, GivenGetPowerLimitsReturnErrorWhenGettingPowerLimitsWhenHwmonInterfaceExistForBurstPowerLimitThenProperErrorCodesIsReturned) {

    auto pSysFsAccess = std::make_unique<MockPowerSysfsAccessInterface>();
    pSysFsAccess->mockReadReturnStatus.push_back(ZE_RESULT_ERROR_UNKNOWN);
    pSysFsAccess->isRepeated = true;
    std::unique_ptr<PublicLinuxPowerImp> pLinuxPowerImp(new PublicLinuxPowerImp(pOsSysman, false, 0));
    pLinuxPowerImp->pSysfsAccess = pSysFsAccess.get();
    pLinuxPowerImp->pPmt = static_cast<MockPowerPmt *>(pLinuxSysmanImp->getPlatformMonitoringTechAccess(0));
    pLinuxPowerImp->isPowerModuleSupported();

    zes_power_burst_limit_t burstSet = {};
    zes_power_burst_limit_t burstGet = {};
    burstSet.enabled = 1;
    burstSet.power = 375000;
    EXPECT_EQ(ZE_RESULT_SUCCESS, pLinuxPowerImp->setLimits(nullptr, &burstSet, nullptr));
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, pLinuxPowerImp->getLimits(nullptr, &burstGet, nullptr));
}

TEST_F(SysmanDevicePowerFixtureI915, GivenSetPowerLimitsReturnErrorWhenSettingPowerLimitsWhenHwmonInterfaceExistForBurstPowerLimitThenProperErrorCodesReturned) {

    auto pSysFsAccess = std::make_unique<MockPowerSysfsAccessInterface>();
    pSysFsAccess->mockWriteReturnStatus.push_back(ZE_RESULT_ERROR_NOT_AVAILABLE);
    std::unique_ptr<PublicLinuxPowerImp> pLinuxPowerImp(new PublicLinuxPowerImp(pOsSysman, false, 0));
    pLinuxPowerImp->pSysfsAccess = pSysFsAccess.get();
    pLinuxPowerImp->pPmt = static_cast<MockPowerPmt *>(pLinuxSysmanImp->getPlatformMonitoringTechAccess(0));
    pLinuxPowerImp->isPowerModuleSupported();

    zes_power_burst_limit_t burstSet = {};
    burstSet.enabled = 1;
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, pLinuxPowerImp->setLimits(nullptr, &burstSet, nullptr));
}

TEST_F(SysmanDevicePowerFixtureI915, GivenSetPowerLimitsReturnErrorWhenSettingPowerLimitsWhenHwmonInterfaceExistForBurstPowerLimitEnabledThenProperErrorCodesReturned) {

    auto pSysFsAccess = std::make_unique<MockPowerSysfsAccessInterface>();
    pSysFsAccess->mockWriteReturnStatus.push_back(ZE_RESULT_ERROR_NOT_AVAILABLE);
    pSysFsAccess->mockReadReturnStatus.push_back(ZE_RESULT_ERROR_NOT_AVAILABLE);
    std::unique_ptr<PublicLinuxPowerImp> pLinuxPowerImp(new PublicLinuxPowerImp(pOsSysman, false, 0));
    pLinuxPowerImp->pSysfsAccess = pSysFsAccess.get();
    pLinuxPowerImp->pPmt = static_cast<MockPowerPmt *>(pLinuxSysmanImp->getPlatformMonitoringTechAccess(0));
    pLinuxPowerImp->isPowerModuleSupported();

    zes_power_burst_limit_t burstSet = {};
    zes_power_burst_limit_t burstGet = {};
    burstSet.enabled = 1;
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, pLinuxPowerImp->setLimits(nullptr, &burstSet, nullptr));
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, pLinuxPowerImp->getLimits(nullptr, &burstGet, nullptr));
}

TEST_F(SysmanDevicePowerFixtureI915, GivenReadingSustainedPowerLimitNodeReturnErrorWhenSetOrGetPowerLimitsWhenHwmonInterfaceExistForSustainedPowerLimitEnabledThenProperErrorCodesReturned) {

    auto pSysFsAccess = std::make_unique<MockPowerSysfsAccessInterface>();
    pSysFsAccess->mockReadReturnStatus.push_back(ZE_RESULT_ERROR_NOT_AVAILABLE);
    pSysFsAccess->isRepeated = true;
    std::unique_ptr<PublicLinuxPowerImp> pLinuxPowerImp(new PublicLinuxPowerImp(pOsSysman, false, 0));
    pLinuxPowerImp->pSysfsAccess = pSysFsAccess.get();
    pLinuxPowerImp->pPmt = static_cast<MockPowerPmt *>(pLinuxSysmanImp->getPlatformMonitoringTechAccess(0));
    pLinuxPowerImp->isPowerModuleSupported();

    zes_power_sustained_limit_t sustainedSet = {};
    zes_power_sustained_limit_t sustainedGet = {};

    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, pLinuxPowerImp->setLimits(&sustainedSet, nullptr, nullptr));
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, pLinuxPowerImp->getLimits(&sustainedGet, nullptr, nullptr));
}

TEST_F(SysmanDevicePowerFixtureI915, GivenReadingSustainedPowerNodeReturnErrorWhenGetPowerLimitsForSustainedPowerWhenHwmonInterfaceExistThenProperErrorCodesReturned) {

    auto pSysFsAccess = std::make_unique<MockPowerSysfsAccessInterface>();
    pSysFsAccess->mockReadReturnStatus.push_back(ZE_RESULT_ERROR_NOT_AVAILABLE);
    std::unique_ptr<PublicLinuxPowerImp> pLinuxPowerImp(new PublicLinuxPowerImp(pOsSysman, false, 0));
    pLinuxPowerImp->pSysfsAccess = pSysFsAccess.get();
    pLinuxPowerImp->pPmt = static_cast<MockPowerPmt *>(pLinuxSysmanImp->getPlatformMonitoringTechAccess(0));
    pLinuxPowerImp->isPowerModuleSupported();

    zes_power_sustained_limit_t sustainedGet = {};
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, pLinuxPowerImp->getLimits(&sustainedGet, nullptr, nullptr));
}

TEST_F(SysmanDevicePowerFixtureI915, GivenReadingSustainedPowerIntervalNodeReturnErrorWhenGetPowerLimitsForSustainedPowerWhenHwmonInterfaceExistThenProperErrorCodesReturned) {

    auto pSysFsAccess = std::make_unique<MockPowerSysfsAccessInterface>();
    pSysFsAccess->mockReadReturnStatus.push_back(ZE_RESULT_ERROR_NOT_AVAILABLE);
    std::unique_ptr<PublicLinuxPowerImp> pLinuxPowerImp(new PublicLinuxPowerImp(pOsSysman, false, 0));
    pLinuxPowerImp->pSysfsAccess = pSysFsAccess.get();
    pLinuxPowerImp->pPmt = static_cast<MockPowerPmt *>(pLinuxSysmanImp->getPlatformMonitoringTechAccess(0));
    pLinuxPowerImp->isPowerModuleSupported();

    zes_power_sustained_limit_t sustainedGet = {};
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, pLinuxPowerImp->getLimits(&sustainedGet, nullptr, nullptr));
}

TEST_F(SysmanDevicePowerFixtureI915, GivenwritingSustainedPowerNodeReturnErrorWhenSetPowerLimitsForSustainedPowerWhenHwmonInterfaceExistThenProperErrorCodesReturned) {

    auto pSysFsAccess = std::make_unique<MockPowerSysfsAccessInterface>();
    pSysFsAccess->mockWriteReturnStatus.push_back(ZE_RESULT_ERROR_NOT_AVAILABLE);
    std::unique_ptr<PublicLinuxPowerImp> pLinuxPowerImp(new PublicLinuxPowerImp(pOsSysman, false, 0));
    pLinuxPowerImp->pSysfsAccess = pSysFsAccess.get();
    pLinuxPowerImp->pPmt = static_cast<MockPowerPmt *>(pLinuxSysmanImp->getPlatformMonitoringTechAccess(0));
    pLinuxPowerImp->isPowerModuleSupported();

    zes_power_sustained_limit_t sustainedSet = {};
    sustainedSet.enabled = 1;
    sustainedSet.interval = 10;
    sustainedSet.power = 300000;
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, pLinuxPowerImp->setLimits(&sustainedSet, nullptr, nullptr));
}

TEST_F(SysmanDevicePowerFixtureI915, GivenwritingSustainedPowerIntervalNodeReturnErrorWhenSetPowerLimitsForSustainedPowerIntervalWhenHwmonInterfaceExistThenProperErrorCodesReturned) {
    auto pSysFsAccess = std::make_unique<MockPowerSysfsAccessInterface>();
    pSysFsAccess->mockWriteReturnStatus.push_back(ZE_RESULT_ERROR_NOT_AVAILABLE);
    std::unique_ptr<PublicLinuxPowerImp> pLinuxPowerImp(new PublicLinuxPowerImp(pOsSysman, false, 0));
    pLinuxPowerImp->pSysfsAccess = pSysFsAccess.get();
    pLinuxPowerImp->pPmt = static_cast<MockPowerPmt *>(pLinuxSysmanImp->getPlatformMonitoringTechAccess(0));
    pLinuxPowerImp->isPowerModuleSupported();

    zes_power_sustained_limit_t sustainedSet = {};
    sustainedSet.enabled = 1;
    sustainedSet.interval = 10;
    sustainedSet.power = 300000;
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, pLinuxPowerImp->setLimits(&sustainedSet, nullptr, nullptr));
}

TEST_F(SysmanDevicePowerFixtureI915, GivenValidPowerHandleWhenWritingToSustainedPowerEnableNodeWithoutPermissionsThenValidErrorIsReturned) {

    auto pSysFsAccess = std::make_unique<MockPowerSysfsAccessInterface>();
    pSysFsAccess->mockWriteReturnStatus.push_back(ZE_RESULT_ERROR_INSUFFICIENT_PERMISSIONS);
    std::unique_ptr<PublicLinuxPowerImp> pLinuxPowerImp(new PublicLinuxPowerImp(pOsSysman, false, 0));
    pLinuxPowerImp->pSysfsAccess = pSysFsAccess.get();
    pLinuxPowerImp->pPmt = static_cast<MockPowerPmt *>(pLinuxSysmanImp->getPlatformMonitoringTechAccess(0));
    pLinuxPowerImp->isPowerModuleSupported();

    zes_power_sustained_limit_t sustainedSet = {};
    sustainedSet.enabled = 0;
    EXPECT_EQ(ZE_RESULT_ERROR_INSUFFICIENT_PERMISSIONS, pLinuxPowerImp->setLimits(&sustainedSet, nullptr, nullptr));
}

TEST_F(SysmanDevicePowerFixtureI915, GivenValidPowerHandleAndPermissionsThenFirstDisableSustainedPowerLimitAndThenEnableItAndCheckSuccesIsReturned) {

    auto pSysFsAccess = std::make_unique<MockPowerSysfsAccessInterface>();
    std::unique_ptr<PublicLinuxPowerImp> pLinuxPowerImp(new PublicLinuxPowerImp(pOsSysman, false, 0));
    pLinuxPowerImp->pSysfsAccess = pSysFsAccess.get();
    pLinuxPowerImp->pPmt = static_cast<MockPowerPmt *>(pLinuxSysmanImp->getPlatformMonitoringTechAccess(0));
    pLinuxPowerImp->isPowerModuleSupported();

    zes_power_sustained_limit_t sustainedSet = {};
    zes_power_sustained_limit_t sustainedGet = {};
    sustainedSet.enabled = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, pLinuxPowerImp->setLimits(&sustainedSet, nullptr, nullptr));
    sustainedSet.enabled = 1;
    sustainedSet.interval = 10;
    sustainedSet.power = 300000;
    EXPECT_EQ(ZE_RESULT_SUCCESS, pLinuxPowerImp->setLimits(&sustainedSet, nullptr, nullptr));
    EXPECT_EQ(ZE_RESULT_SUCCESS, pLinuxPowerImp->getLimits(&sustainedGet, nullptr, nullptr));
    EXPECT_EQ(sustainedGet.enabled, sustainedSet.enabled);
    EXPECT_EQ(sustainedGet.power, sustainedSet.power);
    EXPECT_EQ(sustainedGet.interval, sustainedSet.interval);
}

TEST_F(SysmanDevicePowerFixtureI915, GivenGetPowerLimitsWhenPowerLimitsAreDisabledWhenHwmonInterfaceExistThenAllPowerValuesAreIgnored) {

    auto pSysFsAccess = std::make_unique<MockPowerSysfsAccessInterface>();
    pSysFsAccess->mockReadReturnStatus.push_back(ZE_RESULT_SUCCESS);
    pSysFsAccess->mockReadUnsignedLongValue.push_back(0);
    std::unique_ptr<PublicLinuxPowerImp> pLinuxPowerImp(new PublicLinuxPowerImp(pOsSysman, false, 0));
    pLinuxPowerImp->pSysfsAccess = pSysFsAccess.get();
    pLinuxPowerImp->pPmt = static_cast<MockPowerPmt *>(pLinuxSysmanImp->getPlatformMonitoringTechAccess(0));
    pLinuxPowerImp->isPowerModuleSupported();

    zes_power_sustained_limit_t sustainedGet = {};
    zes_power_burst_limit_t burstGet = {};
    EXPECT_EQ(ZE_RESULT_SUCCESS, pLinuxPowerImp->getLimits(&sustainedGet, nullptr, nullptr));
    EXPECT_EQ(sustainedGet.interval, 0);
    EXPECT_EQ(sustainedGet.power, 0);
    EXPECT_EQ(sustainedGet.enabled, 0);
    EXPECT_EQ(ZE_RESULT_SUCCESS, pLinuxPowerImp->getLimits(nullptr, &burstGet, nullptr));
    EXPECT_EQ(burstGet.enabled, 0);
    EXPECT_EQ(burstGet.power, 0);
    EXPECT_EQ(ZE_RESULT_SUCCESS, pLinuxPowerImp->setLimits(&sustainedGet, nullptr, nullptr));
    zes_power_burst_limit_t burstSet = {};
    burstSet.enabled = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, pLinuxPowerImp->setLimits(nullptr, &burstSet, nullptr));
}

TEST_F(SysmanDevicePowerFixtureI915, GivenScanDirectoriesFailAndPmtIsNotNullPointerThenPowerModuleIsSupported) {

    pSysfsAccess->mockScanDirEntriesReturnStatus.push_back(ZE_RESULT_ERROR_NOT_AVAILABLE);
    pSysfsAccess->isRepeated = true;
    auto subDeviceCount = pLinuxSysmanImp->getSubDeviceCount();
    pSysmanDeviceImp->pPowerHandleContext->init(subDeviceCount);
    ze_bool_t onSubdevice = (subDeviceCount == 0) ? false : true;
    uint32_t subdeviceId = 0;
    PublicLinuxPowerImp *pPowerImp = new PublicLinuxPowerImp(pOsSysman, onSubdevice, subdeviceId);
    EXPECT_TRUE(pPowerImp->isPowerModuleSupported());
    delete pPowerImp;
}

TEST_F(SysmanDevicePowerFixtureI915, GivenComponentCountZeroWhenEnumeratingPowerDomainsThenValidCountIsReturnedAndVerifySysmanPowerGetCallSucceeds) {

    pSysfsAccess->mockScanDirEntriesReturnStatus.push_back(ZE_RESULT_ERROR_NOT_AVAILABLE);
    pSysfsAccess->isRepeated = true;

    for (const auto &handle : pSysmanDeviceImp->pPowerHandleContext->handleList) {
        delete handle;
    }
    pSysmanDeviceImp->pPowerHandleContext->handleList.clear();
    pSysmanDeviceImp->pPowerHandleContext->init(pLinuxSysmanImp->getSubDeviceCount());
    uint32_t count = 0;
    EXPECT_EQ(zesDeviceEnumPowerDomains(device->toHandle(), &count, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(count, powerHandleComponentCount);
}

TEST_F(SysmanDevicePowerFixtureI915, GivenInvalidComponentCountWhenEnumeratingPowerDomainsThenValidCountIsReturnedAndVerifySysmanPowerGetCallSucceeds) {

    pSysfsAccess->mockScanDirEntriesReturnStatus.push_back(ZE_RESULT_ERROR_NOT_AVAILABLE);
    pSysfsAccess->isRepeated = true;

    for (const auto &handle : pSysmanDeviceImp->pPowerHandleContext->handleList) {
        delete handle;
    }
    pSysmanDeviceImp->pPowerHandleContext->handleList.clear();
    pSysmanDeviceImp->pPowerHandleContext->init(pLinuxSysmanImp->getSubDeviceCount());
    uint32_t count = 0;
    EXPECT_EQ(zesDeviceEnumPowerDomains(device->toHandle(), &count, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(count, powerHandleComponentCount);

    count = count + 1;
    EXPECT_EQ(zesDeviceEnumPowerDomains(device->toHandle(), &count, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(count, powerHandleComponentCount);
}

TEST_F(SysmanDevicePowerFixtureI915, GivenComponentCountZeroWhenEnumeratingPowerDomainsThenValidPowerHandlesIsReturned) {

    pSysfsAccess->mockScanDirEntriesReturnStatus.push_back(ZE_RESULT_ERROR_NOT_AVAILABLE);
    pSysfsAccess->isRepeated = true;

    for (const auto &handle : pSysmanDeviceImp->pPowerHandleContext->handleList) {
        delete handle;
    }
    pSysmanDeviceImp->pPowerHandleContext->handleList.clear();
    pSysmanDeviceImp->pPowerHandleContext->init(pLinuxSysmanImp->getSubDeviceCount());
    uint32_t count = 0;
    EXPECT_EQ(zesDeviceEnumPowerDomains(device->toHandle(), &count, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(count, powerHandleComponentCount);

    std::vector<zes_pwr_handle_t> handles(count, nullptr);
    EXPECT_EQ(zesDeviceEnumPowerDomains(device->toHandle(), &count, handles.data()), ZE_RESULT_SUCCESS);
    for (auto handle : handles) {
        EXPECT_NE(handle, nullptr);
    }
}

TEST_F(SysmanDevicePowerFixtureI915, GivenValidPowerHandleWhenGettingPowerPropertiesThenCallSucceeds) {

    auto pSysFsAccess = std::make_unique<MockPowerSysfsAccessInterface>();
    pSysFsAccess->mockScanDirEntriesReturnStatus.push_back(ZE_RESULT_ERROR_NOT_AVAILABLE);
    pSysFsAccess->isRepeated = true;

    std::unique_ptr<PublicLinuxPowerImp> pLinuxPowerImp(new PublicLinuxPowerImp(pOsSysman, false, 0));
    pLinuxPowerImp->pSysfsAccess = pSysFsAccess.get();
    pLinuxPowerImp->isPowerModuleSupported();

    zes_power_properties_t properties{};
    EXPECT_EQ(ZE_RESULT_SUCCESS, pLinuxPowerImp->getProperties(&properties));
    EXPECT_FALSE(properties.onSubdevice);
    EXPECT_EQ(properties.subdeviceId, 0u);
}

TEST_F(SysmanDevicePowerFixtureI915, GivenValidPowerHandleWhenGettingPowerEnergyCounterThenValidPowerReadingsRetrieved) {

    auto pSysFsAccess = std::make_unique<MockPowerSysfsAccessInterface>();
    pSysFsAccess->mockScanDirEntriesReturnStatus.push_back(ZE_RESULT_ERROR_NOT_AVAILABLE);
    pSysFsAccess->isRepeated = true;

    auto pPmt = static_cast<MockPowerPmt *>(pLinuxSysmanImp->getPlatformMonitoringTechAccess(0));
    pPmt->preadFunction = preadMockPower;

    std::unique_ptr<PublicLinuxPowerImp> pLinuxPowerImp(new PublicLinuxPowerImp(pOsSysman, false, 0));
    pLinuxPowerImp->pSysfsAccess = pSysFsAccess.get();
    pLinuxPowerImp->pPmt = pPmt;
    pLinuxPowerImp->isPowerModuleSupported();

    zes_power_energy_counter_t energyCounter;
    uint64_t expectedEnergyCounter = convertJouleToMicroJoule * (setEnergyCounter / 1048576);
    ASSERT_EQ(ZE_RESULT_SUCCESS, pLinuxPowerImp->getEnergyCounter(&energyCounter));
    EXPECT_EQ(energyCounter.energy, expectedEnergyCounter);
}

TEST_F(SysmanDevicePowerFixtureI915, GivenValidPowerHandleWhenGettingPowerEnergyCounterWhenEnergyHwmonFileReturnsErrorAndPmtFailsThenFailureIsReturned) {

    auto pSysFsAccess = std::make_unique<MockPowerSysfsAccessInterface>();
    pSysFsAccess->mockReadReturnStatus.push_back(ZE_RESULT_ERROR_NOT_AVAILABLE);
    pSysFsAccess->isRepeated = true;
    std::unique_ptr<PublicLinuxPowerImp> pLinuxPowerImp(new PublicLinuxPowerImp(pOsSysman, false, 0));
    pLinuxPowerImp->pSysfsAccess = pSysFsAccess.get();
    pLinuxPowerImp->pPmt = nullptr;
    pLinuxPowerImp->isPowerModuleSupported();

    zes_power_energy_counter_t energyCounter = {};
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, pLinuxPowerImp->getEnergyCounter(&energyCounter));
}

TEST_F(SysmanDevicePowerFixtureI915, GivenValidPowerHandleWhenGettingPowerEnergyThresholdThenUnsupportedFeatureErrorIsReturned) {

    pSysfsAccess->mockScanDirEntriesReturnStatus.push_back(ZE_RESULT_ERROR_NOT_AVAILABLE);
    pSysfsAccess->isRepeated = true;

    for (const auto &handle : pSysmanDeviceImp->pPowerHandleContext->handleList) {
        delete handle;
    }
    pSysmanDeviceImp->pPowerHandleContext->handleList.clear();
    pSysmanDeviceImp->pPowerHandleContext->init(pLinuxSysmanImp->getSubDeviceCount());
    zes_energy_threshold_t threshold;
    auto handles = getPowerHandles(powerHandleComponentCount);
    for (auto handle : handles) {
        EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesPowerGetEnergyThreshold(handle, &threshold));
    }
}

TEST_F(SysmanDevicePowerFixtureI915, GivenScanDirectoriesFailAndPmtIsNullWhenGettingCardPowerThenReturnsFailure) {

    pSysfsAccess->mockScanDirEntriesReturnStatus.push_back(ZE_RESULT_ERROR_NOT_AVAILABLE);
    pSysfsAccess->isRepeated = true;

    for (const auto &handle : pSysmanDeviceImp->pPowerHandleContext->handleList) {
        delete handle;
    }
    pSysmanDeviceImp->pPowerHandleContext->handleList.clear();
    for (auto &pmtMapElement : pLinuxSysmanImp->mapOfSubDeviceIdToPmtObject) {
        if (pmtMapElement.second) {
            delete pmtMapElement.second;
            pmtMapElement.second = nullptr;
        }
    }
    pLinuxSysmanImp->mapOfSubDeviceIdToPmtObject.clear();
    pSysmanDeviceImp->pPowerHandleContext->init(pLinuxSysmanImp->getSubDeviceCount());

    zes_pwr_handle_t phPower = {};
    EXPECT_EQ(zesDeviceGetCardPowerDomain(device->toHandle(), &phPower), ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
}

TEST_F(SysmanDevicePowerFixtureI915, GivenValidPowerHandleWhenSettingPowerEnergyThresholdThenUnsupportedFeatureErrorIsReturned) {

    pSysfsAccess->mockScanDirEntriesReturnStatus.push_back(ZE_RESULT_ERROR_NOT_AVAILABLE);
    pSysfsAccess->isRepeated = true;

    for (const auto &handle : pSysmanDeviceImp->pPowerHandleContext->handleList) {
        delete handle;
    }
    pSysmanDeviceImp->pPowerHandleContext->handleList.clear();
    pSysmanDeviceImp->pPowerHandleContext->init(pLinuxSysmanImp->getSubDeviceCount());
    double threshold = 0;
    auto handles = getPowerHandles(powerHandleComponentCount);
    for (auto handle : handles) {
        EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesPowerSetEnergyThreshold(handle, threshold));
    }
}

TEST_F(SysmanDevicePowerFixtureI915, GivenValidPowerHandleWhenGettingPowerLimitsThenUnsupportedFeatureErrorIsReturned) {

    auto pSysFsAccess = std::make_unique<MockPowerSysfsAccessInterface>();
    pSysFsAccess->mockScanDirEntriesReturnStatus.push_back(ZE_RESULT_ERROR_NOT_AVAILABLE);
    pSysFsAccess->isRepeated = true;
    auto pPmt = static_cast<MockPowerPmt *>(pLinuxSysmanImp->getPlatformMonitoringTechAccess(0));
    std::unique_ptr<PublicLinuxPowerImp> pLinuxPowerImp(new PublicLinuxPowerImp(pOsSysman, false, 0));
    pLinuxPowerImp->pSysfsAccess = pSysFsAccess.get();
    pLinuxPowerImp->pPmt = pPmt;
    pLinuxPowerImp->isPowerModuleSupported();

    zes_power_sustained_limit_t sustained;
    zes_power_burst_limit_t burst;
    zes_power_peak_limit_t peak;
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, pLinuxPowerImp->getLimits(&sustained, &burst, &peak));
}

TEST_F(SysmanDevicePowerFixtureI915, GivenValidPowerHandleWhenSettingPowerLimitsThenUnsupportedFeatureErrorIsReturned) {

    auto pSysFsAccess = std::make_unique<MockPowerSysfsAccessInterface>();
    pSysFsAccess->mockScanDirEntriesReturnStatus.push_back(ZE_RESULT_ERROR_NOT_AVAILABLE);
    pSysFsAccess->isRepeated = true;
    auto pPmt = static_cast<MockPowerPmt *>(pLinuxSysmanImp->getPlatformMonitoringTechAccess(0));
    std::unique_ptr<PublicLinuxPowerImp> pLinuxPowerImp(new PublicLinuxPowerImp(pOsSysman, false, 0));
    pLinuxPowerImp->pSysfsAccess = pSysFsAccess.get();
    pLinuxPowerImp->pPmt = pPmt;
    pLinuxPowerImp->isPowerModuleSupported();

    zes_power_sustained_limit_t sustained;
    zes_power_burst_limit_t burst;
    zes_power_peak_limit_t peak;
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, pLinuxPowerImp->setLimits(&sustained, &burst, &peak));
}

TEST_F(SysmanDevicePowerMultiDeviceFixture, GivenValidPowerHandleWhenGettingPowerEnergyCounterWhenEnergyHwmonFailsThenValidPowerReadingsRetrievedFromPmt) {

    auto pSysFsAccess = std::make_unique<MockPowerSysfsAccessInterface>();
    pSysFsAccess->mockScanDirEntriesReturnStatus.push_back(ZE_RESULT_ERROR_NOT_AVAILABLE);
    pSysFsAccess->isRepeated = true;

    auto pPmt = static_cast<MockPowerPmt *>(pLinuxSysmanImp->getPlatformMonitoringTechAccess(0));
    pPmt->preadFunction = preadMockPower;

    std::unique_ptr<PublicLinuxPowerImp> pLinuxPowerImp(new PublicLinuxPowerImp(pOsSysman, false, 0));
    pLinuxPowerImp->pSysfsAccess = pSysFsAccess.get();
    pLinuxPowerImp->pPmt = pPmt;
    pLinuxPowerImp->isPowerModuleSupported();

    zes_power_energy_counter_t energyCounter;
    uint64_t expectedEnergyCounter = convertJouleToMicroJoule * (setEnergyCounter / 1048576);
    ASSERT_EQ(ZE_RESULT_SUCCESS, pLinuxPowerImp->getEnergyCounter(&energyCounter));
    EXPECT_EQ(energyCounter.energy, expectedEnergyCounter);
}

TEST_F(SysmanDevicePowerFixtureI915, GivenValidPowerHandlesWhenCallingSetAndGetPowerLimitExtWhenHwmonInterfaceExistThenUnsupportedFeatureIsReturned) {

    auto pSysFsAccess = std::make_unique<MockPowerSysfsAccessInterface>();
    auto pPmt = static_cast<MockPowerPmt *>(pLinuxSysmanImp->getPlatformMonitoringTechAccess(0));
    std::unique_ptr<PublicLinuxPowerImp> pLinuxPowerImp(new PublicLinuxPowerImp(pOsSysman, false, 0));
    pLinuxPowerImp->pSysfsAccess = pSysFsAccess.get();
    pLinuxPowerImp->pPmt = pPmt;
    pLinuxPowerImp->isPowerModuleSupported();

    uint32_t limitCount = 0;

    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, pLinuxPowerImp->getLimitsExt(&limitCount, nullptr));
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, pLinuxPowerImp->setLimitsExt(&limitCount, nullptr));
}

TEST_F(SysmanDevicePowerFixtureI915, GivenValidPowerHandleWhenGettingPowerPropertiesExtThenApiReturnsFailure) {
    auto handles = getPowerHandles(powerHandleComponentCount);
    for (auto handle : handles) {
        zes_power_properties_t properties = {};
        zes_power_ext_properties_t extProperties = {};

        extProperties.stype = ZES_STRUCTURE_TYPE_POWER_EXT_PROPERTIES;
        properties.pNext = &extProperties;
        EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesPowerGetProperties(handle, &properties));
    }
}

TEST_F(SysmanDevicePowerFixtureI915, GivenValidPowerHandleWhenGettingPowerPropertiesExtWithoutStypeThenExtPropertiesAreNotReturned) {

    auto pSysFsAccess = std::make_unique<MockPowerSysfsAccessInterface>();
    auto pPmt = static_cast<MockPowerPmt *>(pLinuxSysmanImp->getPlatformMonitoringTechAccess(0));
    std::unique_ptr<PublicLinuxPowerImp> pLinuxPowerImp(new PublicLinuxPowerImp(pOsSysman, false, 0));
    pLinuxPowerImp->pSysfsAccess = pSysFsAccess.get();
    pLinuxPowerImp->pPmt = pPmt;
    pLinuxPowerImp->isPowerModuleSupported();

    zes_power_properties_t properties = {};
    zes_power_ext_properties_t extProperties = {};

    properties.pNext = &extProperties;
    EXPECT_EQ(ZE_RESULT_SUCCESS, pLinuxPowerImp->getProperties(&properties));
    EXPECT_FALSE(properties.onSubdevice);
    EXPECT_EQ(properties.subdeviceId, 0u);
    EXPECT_EQ(properties.canControl, true);
    EXPECT_EQ(properties.isEnergyThresholdSupported, false);
    EXPECT_EQ(properties.defaultLimit, static_cast<int32_t>(mockDefaultPowerLimitVal / milliFactor));
    EXPECT_EQ(properties.maxLimit, static_cast<int32_t>(mockMaxPowerLimitVal / milliFactor));
    EXPECT_EQ(properties.minLimit, static_cast<int32_t>(mockMinPowerLimitVal / milliFactor));
}

TEST_F(SysmanDevicePowerFixtureXe, GivenKmdInterfaceWhenGettingSysFsFilenamesForPowerForXeVersionThenProperPathsAreReturned) {
    auto pSysmanKmdInterface = std::make_unique<SysmanKmdInterfaceXe>(pLinuxSysmanImp->getProductFamily());
    EXPECT_STREQ("energy1_input", pSysmanKmdInterface->getSysfsFilePath(SysfsName::sysfsNameEnergyCounterNode, 0, false).c_str());
    EXPECT_STREQ("power1_rated_max", pSysmanKmdInterface->getSysfsFilePath(SysfsName::sysfsNameDefaultPowerLimit, 0, false).c_str());
    EXPECT_STREQ("power1_max", pSysmanKmdInterface->getSysfsFilePath(SysfsName::sysfsNameSustainedPowerLimit, 0, false).c_str());
    EXPECT_STREQ("power1_max_interval", pSysmanKmdInterface->getSysfsFilePath(SysfsName::sysfsNameSustainedPowerLimitInterval, 0, false).c_str());
}

} // namespace ult
} // namespace Sysman
} // namespace L0
