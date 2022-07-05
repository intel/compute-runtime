/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/sysman/power/linux/os_power_imp_prelim.h"
#include "level_zero/tools/test/unit_tests/sources/sysman/power/linux/mock_sysfs_power_prelim.h"

namespace L0 {
namespace ult {

static int fakeFileDescriptor = 123;
constexpr uint64_t convertJouleToMicroJoule = 1000000u;
constexpr uint32_t powerHandleComponentCount = 1u;

inline static int openMockPower(const char *pathname, int flags) {
    if (strcmp(pathname, "/sys/class/intel_pmt/telem2/telem") == 0) {
        return fakeFileDescriptor;
    }
    if (strcmp(pathname, "/sys/class/intel_pmt/telem3/telem") == 0) {
        return fakeFileDescriptor;
    }
    return -1;
}

inline static int closeMockPower(int fd) {
    if (fd == fakeFileDescriptor) {
        return 0;
    }
    return -1;
}

ssize_t preadMockPower(int fd, void *buf, size_t count, off_t offset) {
    uint64_t *mockBuf = static_cast<uint64_t *>(buf);
    *mockBuf = setEnergyCounter;
    return count;
}

TEST_F(SysmanDevicePowerFixture, GivenComponentCountZeroWhenEnumeratingPowerDomainsWhenhwmonInterfaceExistsThenValidCountIsReturnedAndVerifySysmanPowerGetCallSucceeds) {
    uint32_t count = 0;
    EXPECT_EQ(zesDeviceEnumPowerDomains(device->toHandle(), &count, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(count, powerHandleComponentCount);
}

TEST_F(SysmanDevicePowerFixture, GivenInvalidComponentCountWhenEnumeratingPowerDomainsWhenhwmonInterfaceExistsThenValidCountIsReturnedAndVerifySysmanPowerGetCallSucceeds) {
    uint32_t count = 0;
    EXPECT_EQ(zesDeviceEnumPowerDomains(device->toHandle(), &count, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(count, powerHandleComponentCount);

    count = count + 1;
    EXPECT_EQ(zesDeviceEnumPowerDomains(device->toHandle(), &count, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(count, powerHandleComponentCount);
}

TEST_F(SysmanDevicePowerFixture, GivenComponentCountZeroWhenEnumeratingPowerDomainsWhenhwmonInterfaceExistsThenValidPowerHandlesIsReturned) {
    uint32_t count = 0;
    EXPECT_EQ(zesDeviceEnumPowerDomains(device->toHandle(), &count, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(count, powerHandleComponentCount);

    std::vector<zes_pwr_handle_t> handles(count, nullptr);
    EXPECT_EQ(zesDeviceEnumPowerDomains(device->toHandle(), &count, handles.data()), ZE_RESULT_SUCCESS);
    for (auto handle : handles) {
        EXPECT_NE(handle, nullptr);
    }
}

TEST_F(SysmanDevicePowerFixture, GivenValidPowerPointerWhenGettingCardPowerDomainWhenhwmonInterfaceExistsAndThenCallSucceds) {
    zes_pwr_handle_t phPower = {};
    EXPECT_EQ(zesDeviceGetCardPowerDomain(device->toHandle(), &phPower), ZE_RESULT_SUCCESS);
}

TEST_F(SysmanDevicePowerFixture, GivenInvalidPowerPointerWhenGettingCardPowerDomainAndThenReturnsFailure) {
    EXPECT_EQ(zesDeviceGetCardPowerDomain(device->toHandle(), nullptr), ZE_RESULT_ERROR_INVALID_NULL_POINTER);
}

TEST_F(SysmanDevicePowerFixture, GivenUninitializedPowerHandlesAndWhenGettingCardPowerDomainThenReurnsFailure) {
    for (const auto &handle : pSysmanDeviceImp->pPowerHandleContext->handleList) {
        delete handle;
    }
    pSysmanDeviceImp->pPowerHandleContext->handleList.clear();

    zes_pwr_handle_t phPower = {};
    EXPECT_EQ(zesDeviceGetCardPowerDomain(device->toHandle(), &phPower), ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
}

TEST_F(SysmanDevicePowerFixture, GivenScanDiectoriesFailAndPmtIsNullWhenGettingCardPowerThenReturnsFailure) {

    pSysfsAccess->mockscanDirEntriesResult = ZE_RESULT_ERROR_NOT_AVAILABLE;
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
    pSysmanDeviceImp->pPowerHandleContext->init(deviceHandles, device->toHandle());

    zes_pwr_handle_t phPower = {};
    EXPECT_EQ(zesDeviceGetCardPowerDomain(device->toHandle(), &phPower), ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
}

TEST_F(SysmanDevicePowerFixture, GivenValidPowerHandleWhenGettingPowerPropertiesWhenhwmonInterfaceExistsThenCallSucceeds) {
    auto handles = getPowerHandles(powerHandleComponentCount);

    for (auto handle : handles) {
        zes_power_properties_t properties;
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesPowerGetProperties(handle, &properties));
        EXPECT_FALSE(properties.onSubdevice);
        EXPECT_EQ(properties.subdeviceId, 0u);
        EXPECT_EQ(properties.canControl, true);
        EXPECT_EQ(properties.isEnergyThresholdSupported, false);
        EXPECT_EQ(properties.defaultLimit, (int32_t)(mockDefaultPowerLimitVal / milliFactor));
        EXPECT_EQ(properties.maxLimit, -1);
        EXPECT_EQ(properties.minLimit, -1);
    }
}

TEST_F(SysmanDevicePowerFixture, GivenValidPowerHandleWhenGettingPowerPropertiesWhenHwmonInterfaceExistThenLimitsReturnsUnkown) {

    pSysfsAccess->mockReadResult = ZE_RESULT_ERROR_NOT_AVAILABLE;
    for (const auto &handle : pSysmanDeviceImp->pPowerHandleContext->handleList) {
        delete handle;
    }
    pSysmanDeviceImp->pPowerHandleContext->handleList.clear();
    pSysmanDeviceImp->pPowerHandleContext->init(deviceHandles, device->toHandle());
    auto handles = getPowerHandles(powerHandleComponentCount);
    for (auto handle : handles) {
        zes_power_properties_t properties;
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesPowerGetProperties(handle, &properties));
        EXPECT_FALSE(properties.onSubdevice);
        EXPECT_EQ(properties.subdeviceId, 0u);
        EXPECT_EQ(properties.defaultLimit, -1);
        EXPECT_EQ(properties.maxLimit, -1);
        EXPECT_EQ(properties.minLimit, -1);
    }
}

TEST_F(SysmanDevicePowerFixture, GivenValidPowerHandleWhenGettingPowerPropertiesWhenHwmonInterfaceExistThenMaxLimitIsUnsupported) {
    for (const auto &handle : pSysmanDeviceImp->pPowerHandleContext->handleList) {
        delete handle;
    }
    pSysmanDeviceImp->pPowerHandleContext->handleList.clear();
    pSysmanDeviceImp->pPowerHandleContext->init(deviceHandles, device->toHandle());
    auto handles = getPowerHandles(powerHandleComponentCount);
    for (auto handle : handles) {
        zes_power_properties_t properties;
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesPowerGetProperties(handle, &properties));
        EXPECT_FALSE(properties.onSubdevice);
        EXPECT_EQ(properties.subdeviceId, 0u);
        EXPECT_EQ(properties.defaultLimit, (int32_t)(mockDefaultPowerLimitVal / milliFactor));
        EXPECT_EQ(properties.maxLimit, -1);
        EXPECT_EQ(properties.minLimit, -1);
    }
}

TEST_F(SysmanDevicePowerFixture, GivenValidPowerHandleWhenGettingPowerPropertiesThenHwmonInterfaceExistAndMinLimitIsUnknown) {
    for (const auto &handle : pSysmanDeviceImp->pPowerHandleContext->handleList) {
        delete handle;
    }
    pSysmanDeviceImp->pPowerHandleContext->handleList.clear();
    pSysmanDeviceImp->pPowerHandleContext->init(deviceHandles, device->toHandle());
    auto handles = getPowerHandles(powerHandleComponentCount);
    for (auto handle : handles) {
        zes_power_properties_t properties;
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesPowerGetProperties(handle, &properties));
        EXPECT_FALSE(properties.onSubdevice);
        EXPECT_EQ(properties.subdeviceId, 0u);
        EXPECT_EQ(properties.defaultLimit, (int32_t)(mockDefaultPowerLimitVal / milliFactor));
        EXPECT_EQ(properties.maxLimit, -1);
        EXPECT_EQ(properties.minLimit, -1);
    }
}

TEST_F(SysmanDevicePowerFixture, GivenValidPowerHandleWhenGettingPowerEnergyCounterFailedWhenHwmonInterfaceExistThenValidErrorCodeReturned) {
    auto handles = getPowerHandles(powerHandleComponentCount);

    for (auto &deviceHandle : deviceHandles) {
        ze_device_properties_t deviceProperties = {ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES};
        Device::fromHandle(deviceHandle)->getProperties(&deviceProperties);
        auto pPmt = static_cast<NiceMock<Mock<PowerPmt>> *>(pLinuxSysmanImp->getPlatformMonitoringTechAccess(deviceProperties.subdeviceId));
        pPmt->openFunction = openMockPower;
        pPmt->closeFunction = closeMockPower;
        pPmt->preadFunction = preadMockPower;
    }

    pSysfsAccess->mockReadValUnsignedLongResult = ZE_RESULT_ERROR_INSUFFICIENT_PERMISSIONS;
    for (auto handle : handles) {
        zes_power_energy_counter_t energyCounter = {};
        uint64_t expectedEnergyCounter = convertJouleToMicroJoule * (setEnergyCounter / 1048576);
        ASSERT_EQ(ZE_RESULT_SUCCESS, zesPowerGetEnergyCounter(handle, &energyCounter));
        EXPECT_EQ(energyCounter.energy, expectedEnergyCounter);
    }
}

TEST_F(SysmanDevicePowerFixture, GivenSetPowerLimitsWhenGettingPowerLimitsWhenHwmonInterfaceExistThenLimitsSetEarlierAreRetrieved) {
    auto handles = getPowerHandles(powerHandleComponentCount);
    for (auto handle : handles) {
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

TEST_F(SysmanDevicePowerFixture, GivenValidPowerHandlesWhenCallingSetAndGetPowerLimitExtWhenHwmonInterfaceExistThenUnsupportedFeatureIsReturned) {
    auto handles = getPowerHandles(powerHandleComponentCount);
    for (auto handle : handles) {
        uint32_t limitCount = 0;

        EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesPowerGetLimitsExt(handle, &limitCount, nullptr));
        EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesPowerSetLimitsExt(handle, &limitCount, nullptr));
    }
}

TEST_F(SysmanDevicePowerFixture, GivenReadingSustainedPowerLimitNodeReturnErrorWhenSetOrGetPowerLimitsWhenHwmonInterfaceExistForSustainedPowerLimitEnabledThenProperErrorCodesReturned) {
    auto handles = getPowerHandles(powerHandleComponentCount);

    pSysfsAccess->mockWriteResult = ZE_RESULT_ERROR_NOT_AVAILABLE;
    pSysfsAccess->mockReadValUnsignedLongResult = ZE_RESULT_ERROR_NOT_AVAILABLE;

    for (auto handle : handles) {
        zes_power_sustained_limit_t sustainedSet = {};
        zes_power_sustained_limit_t sustainedGet = {};

        EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesPowerSetLimits(handle, &sustainedSet, nullptr, nullptr));
        EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesPowerGetLimits(handle, &sustainedGet, nullptr, nullptr));
    }
}

TEST_F(SysmanDevicePowerFixture, GivenReadingPeakPowerLimitNodeReturnErrorWhenSetOrGetPowerLimitsWhenHwmonInterfaceExistForPeakPowerLimitEnabledThenProperErrorCodesReturned) {
    auto handles = getPowerHandles(powerHandleComponentCount);

    pSysfsAccess->mockWriteResult = ZE_RESULT_ERROR_NOT_AVAILABLE;
    pSysfsAccess->mockReadValUnsignedLongResult = ZE_RESULT_ERROR_NOT_AVAILABLE;

    for (auto handle : handles) {
        zes_power_peak_limit_t peakSet = {};
        zes_power_peak_limit_t peakGet = {};

        EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesPowerSetLimits(handle, nullptr, nullptr, &peakSet));
        EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesPowerGetLimits(handle, nullptr, nullptr, &peakGet));
    }
}

TEST_F(SysmanDevicePowerFixture, GivenReadingSustainedPowerNodeReturnErrorWhenGetPowerLimitsForSustainedPowerWhenHwmonInterfaceExistThenProperErrorCodesReturned) {
    auto handles = getPowerHandles(powerHandleComponentCount);

    pSysfsAccess->mockReadValUnsignedLongResult = ZE_RESULT_ERROR_NOT_AVAILABLE;
    for (auto handle : handles) {
        zes_power_sustained_limit_t sustainedGet = {};
        EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesPowerGetLimits(handle, &sustainedGet, nullptr, nullptr));
    }
}

TEST_F(SysmanDevicePowerFixture, GivenReadingpeakPowerNodeReturnErrorWhenGetPowerLimitsForpeakPowerWhenHwmonInterfaceExistThenProperErrorCodesReturned) {
    auto handles = getPowerHandles(powerHandleComponentCount);
    pSysfsAccess->mockReadValUnsignedLongResult = ZE_RESULT_ERROR_NOT_AVAILABLE;

    for (auto handle : handles) {
        zes_power_peak_limit_t peakGet = {};
        EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesPowerGetLimits(handle, nullptr, nullptr, &peakGet));
    }
}

TEST_F(SysmanDevicePowerFixture, GivenwritingSustainedPowerNodeReturnErrorWhenSetPowerLimitsForSustainedPowerWhenHwmonInterfaceExistThenProperErrorCodesReturned) {
    auto handles = getPowerHandles(powerHandleComponentCount);

    pSysfsAccess->mockWriteResult = ZE_RESULT_ERROR_NOT_AVAILABLE;

    zes_power_sustained_limit_t sustainedSet = {};
    sustainedSet.enabled = 1;
    sustainedSet.interval = 10;
    sustainedSet.power = 300000;
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesPowerSetLimits(handles[0], &sustainedSet, nullptr, nullptr));
}

TEST_F(SysmanDevicePowerFixture, GivenwritingSustainedPowerIntervalNodeReturnErrorWhenSetPowerLimitsForSustainedPowerIntervalWhenHwmonInterfaceExistThenProperErrorCodesReturned) {
    auto handles = getPowerHandles(powerHandleComponentCount);

    pSysfsAccess->mockWriteResult = ZE_RESULT_ERROR_NOT_AVAILABLE;
    zes_power_sustained_limit_t sustainedSet = {};
    sustainedSet.enabled = 1;
    sustainedSet.interval = 10;
    sustainedSet.power = 300000;
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesPowerSetLimits(handles[0], &sustainedSet, nullptr, nullptr));
}

TEST_F(SysmanDevicePowerFixture, GivenValidPowerHandleWhenWritingToSustainedPowerEnableNodeWithoutPermissionsThenValidErrorIsReturned) {
    auto handles = getPowerHandles(powerHandleComponentCount);

    pSysfsAccess->mockWriteResult = ZE_RESULT_ERROR_INSUFFICIENT_PERMISSIONS;
    zes_power_sustained_limit_t sustainedSet = {};
    sustainedSet.enabled = 0;
    EXPECT_EQ(ZE_RESULT_ERROR_INSUFFICIENT_PERMISSIONS, zesPowerSetLimits(handles[0], &sustainedSet, nullptr, nullptr));
}

TEST_F(SysmanDevicePowerFixture, GivenValidPowerHandleAndPermissionsThenFirstDisableSustainedPowerLimitAndThenEnableItAndCheckSuccesIsReturned) {
    auto handles = getPowerHandles(powerHandleComponentCount);
    zes_power_sustained_limit_t sustainedSet = {};
    zes_power_sustained_limit_t sustainedGet = {};
    sustainedSet.enabled = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesPowerSetLimits(handles[0], &sustainedSet, nullptr, nullptr));
    sustainedSet.power = 300000;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesPowerSetLimits(handles[0], &sustainedSet, nullptr, nullptr));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesPowerGetLimits(handles[0], &sustainedGet, nullptr, nullptr));
    EXPECT_EQ(sustainedGet.power, sustainedSet.power);
}

TEST_F(SysmanDevicePowerFixture, GivenScanDiectoriesFailAndPmtIsNotNullPointerThenPowerModuleIsSupported) {

    pSysfsAccess->mockscanDirEntriesResult = ZE_RESULT_ERROR_NOT_AVAILABLE;
    pSysmanDeviceImp->pPowerHandleContext->init(deviceHandles, device->toHandle());
    ze_device_properties_t deviceProperties = {ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES};
    Device::fromHandle(device)->getProperties(&deviceProperties);
    PublicLinuxPowerImp *pPowerImp = new PublicLinuxPowerImp(pOsSysman, deviceProperties.flags & ZE_DEVICE_PROPERTY_FLAG_SUBDEVICE, deviceProperties.subdeviceId);
    EXPECT_TRUE(pPowerImp->isPowerModuleSupported());
    delete pPowerImp;
}

TEST_F(SysmanDevicePowerFixture, GivenComponentCountZeroWhenEnumeratingPowerDomainsThenValidCountIsReturnedAndVerifySysmanPowerGetCallSucceeds) {

    pSysfsAccess->mockscanDirEntriesResult = ZE_RESULT_ERROR_NOT_AVAILABLE;
    for (const auto &handle : pSysmanDeviceImp->pPowerHandleContext->handleList) {
        delete handle;
    }
    pSysmanDeviceImp->pPowerHandleContext->handleList.clear();
    pSysmanDeviceImp->pPowerHandleContext->init(deviceHandles, device->toHandle());
    uint32_t count = 0;
    EXPECT_EQ(zesDeviceEnumPowerDomains(device->toHandle(), &count, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(count, powerHandleComponentCount);
}

TEST_F(SysmanDevicePowerFixture, GivenInvalidComponentCountWhenEnumeratingPowerDomainsThenValidCountIsReturnedAndVerifySysmanPowerGetCallSucceeds) {

    pSysfsAccess->mockscanDirEntriesResult = ZE_RESULT_ERROR_NOT_AVAILABLE;
    for (const auto &handle : pSysmanDeviceImp->pPowerHandleContext->handleList) {
        delete handle;
    }
    pSysmanDeviceImp->pPowerHandleContext->handleList.clear();
    pSysmanDeviceImp->pPowerHandleContext->init(deviceHandles, device->toHandle());
    uint32_t count = 0;
    EXPECT_EQ(zesDeviceEnumPowerDomains(device->toHandle(), &count, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(count, powerHandleComponentCount);

    count = count + 1;
    EXPECT_EQ(zesDeviceEnumPowerDomains(device->toHandle(), &count, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(count, powerHandleComponentCount);
}

TEST_F(SysmanDevicePowerFixture, GivenComponentCountZeroWhenEnumeratingPowerDomainsThenValidPowerHandlesIsReturned) {

    pSysfsAccess->mockscanDirEntriesResult = ZE_RESULT_ERROR_NOT_AVAILABLE;
    for (const auto &handle : pSysmanDeviceImp->pPowerHandleContext->handleList) {
        delete handle;
    }
    pSysmanDeviceImp->pPowerHandleContext->handleList.clear();
    pSysmanDeviceImp->pPowerHandleContext->init(deviceHandles, device->toHandle());
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

    pSysfsAccess->mockscanDirEntriesResult = ZE_RESULT_ERROR_NOT_AVAILABLE;
    for (const auto &handle : pSysmanDeviceImp->pPowerHandleContext->handleList) {
        delete handle;
    }
    pSysmanDeviceImp->pPowerHandleContext->handleList.clear();
    pSysmanDeviceImp->pPowerHandleContext->init(deviceHandles, device->toHandle());
    auto handles = getPowerHandles(powerHandleComponentCount);

    for (auto handle : handles) {
        zes_power_properties_t properties;
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesPowerGetProperties(handle, &properties));
        EXPECT_FALSE(properties.onSubdevice);
        EXPECT_EQ(properties.subdeviceId, 0u);
    }
}

TEST_F(SysmanDevicePowerFixture, GivenValidPowerHandleWhenGettingPowerEnergyCounterThenValidPowerReadingsRetrieved) {

    pSysfsAccess->mockscanDirEntriesResult = ZE_RESULT_ERROR_NOT_AVAILABLE;
    for (const auto &handle : pSysmanDeviceImp->pPowerHandleContext->handleList) {
        delete handle;
    }
    pSysmanDeviceImp->pPowerHandleContext->handleList.clear();
    pSysmanDeviceImp->pPowerHandleContext->init(deviceHandles, device->toHandle());
    auto handles = getPowerHandles(powerHandleComponentCount);

    for (auto &deviceHandle : deviceHandles) {
        ze_device_properties_t deviceProperties = {ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES};
        Device::fromHandle(deviceHandle)->getProperties(&deviceProperties);
        auto pPmt = static_cast<NiceMock<Mock<PowerPmt>> *>(pLinuxSysmanImp->getPlatformMonitoringTechAccess(deviceProperties.subdeviceId));
        pPmt->openFunction = openMockPower;
        pPmt->closeFunction = closeMockPower;
        pPmt->preadFunction = preadMockPower;
    }

    for (auto handle : handles) {
        zes_power_energy_counter_t energyCounter;
        uint64_t expectedEnergyCounter = convertJouleToMicroJoule * (setEnergyCounter / 1048576);
        ASSERT_EQ(ZE_RESULT_SUCCESS, zesPowerGetEnergyCounter(handle, &energyCounter));
        EXPECT_EQ(energyCounter.energy, expectedEnergyCounter);
    }
}

TEST_F(SysmanDevicePowerFixture, GivenValidPowerHandleWhenGettingPowerEnergyCounterWhenEnergyHwmonFileReturnsErrorAndPmtFailsThenFailureIsReturned) {
    pSysfsAccess->mockReadValUnsignedLongResult = ZE_RESULT_ERROR_NOT_AVAILABLE;
    for (const auto &handle : pSysmanDeviceImp->pPowerHandleContext->handleList) {
        delete handle;
    }
    for (auto &subDeviceIdToPmtEntry : pLinuxSysmanImp->mapOfSubDeviceIdToPmtObject) {
        delete subDeviceIdToPmtEntry.second;
    }
    pLinuxSysmanImp->mapOfSubDeviceIdToPmtObject.clear();
    for (auto &deviceHandle : deviceHandles) {
        ze_device_properties_t deviceProperties = {ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES};
        Device::fromHandle(deviceHandle)->getProperties(&deviceProperties);
        pLinuxSysmanImp->mapOfSubDeviceIdToPmtObject.emplace(deviceProperties.subdeviceId, nullptr);
    }
    pSysmanDeviceImp->pPowerHandleContext->handleList.clear();
    pSysmanDeviceImp->pPowerHandleContext->init(deviceHandles, device->toHandle());
    auto handles = getPowerHandles(powerHandleComponentCount);

    for (auto handle : handles) {
        zes_power_energy_counter_t energyCounter = {};
        EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesPowerGetEnergyCounter(handle, &energyCounter));
    }
}

TEST_F(SysmanDevicePowerFixture, GivenValidPowerHandleWhenGettingPowerEnergyThresholdThenUnsupportedFeatureErrorIsReturned) {
    pSysfsAccess->mockscanDirEntriesResult = ZE_RESULT_ERROR_NOT_AVAILABLE;
    for (const auto &handle : pSysmanDeviceImp->pPowerHandleContext->handleList) {
        delete handle;
    }
    pSysmanDeviceImp->pPowerHandleContext->handleList.clear();
    pSysmanDeviceImp->pPowerHandleContext->init(deviceHandles, device->toHandle());
    zes_energy_threshold_t threshold;
    auto handles = getPowerHandles(powerHandleComponentCount);
    for (auto handle : handles) {
        EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesPowerGetEnergyThreshold(handle, &threshold));
    }
}

TEST_F(SysmanDevicePowerFixture, GivenValidPowerHandleWhenSettingPowerEnergyThresholdThenUnsupportedFeatureErrorIsReturned) {
    pSysfsAccess->mockscanDirEntriesResult = ZE_RESULT_ERROR_NOT_AVAILABLE;
    for (const auto &handle : pSysmanDeviceImp->pPowerHandleContext->handleList) {
        delete handle;
    }
    pSysmanDeviceImp->pPowerHandleContext->handleList.clear();
    pSysmanDeviceImp->pPowerHandleContext->init(deviceHandles, device->toHandle());
    double threshold = 0;
    auto handles = getPowerHandles(powerHandleComponentCount);
    for (auto handle : handles) {
        EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesPowerSetEnergyThreshold(handle, threshold));
    }
}

TEST_F(SysmanDevicePowerFixture, GivenValidPowerHandleWhenGettingPowerLimitsThenUnsupportedFeatureErrorIsReturned) {
    pSysfsAccess->mockscanDirEntriesResult = ZE_RESULT_ERROR_NOT_AVAILABLE;
    for (const auto &handle : pSysmanDeviceImp->pPowerHandleContext->handleList) {
        delete handle;
    }
    pSysmanDeviceImp->pPowerHandleContext->handleList.clear();
    pSysmanDeviceImp->pPowerHandleContext->init(deviceHandles, device->toHandle());
    auto handles = getPowerHandles(powerHandleComponentCount);
    for (auto handle : handles) {
        zes_power_sustained_limit_t sustained;
        zes_power_burst_limit_t burst;
        zes_power_peak_limit_t peak;
        EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesPowerGetLimits(handle, &sustained, &burst, &peak));
    }
}

TEST_F(SysmanDevicePowerFixture, GivenValidPowerHandleWhenSettingPowerLimitsThenUnsupportedFeatureErrorIsReturned) {
    pSysfsAccess->mockscanDirEntriesResult = ZE_RESULT_ERROR_NOT_AVAILABLE;
    for (const auto &handle : pSysmanDeviceImp->pPowerHandleContext->handleList) {
        delete handle;
    }
    pSysmanDeviceImp->pPowerHandleContext->handleList.clear();
    pSysmanDeviceImp->pPowerHandleContext->init(deviceHandles, device->toHandle());
    auto handles = getPowerHandles(powerHandleComponentCount);
    for (auto handle : handles) {
        zes_power_sustained_limit_t sustained;
        zes_power_burst_limit_t burst;
        zes_power_peak_limit_t peak;
        EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesPowerSetLimits(handle, &sustained, &burst, &peak));
    }
}

TEST_F(SysmanDevicePowerMultiDeviceFixture, GivenValidPowerHandleWhenGettingPowerEnergyCounterWhenEnergyHwmonFailsThenValidPowerReadingsRetrievedFromPmt) {
    pSysfsAccess->mockscanDirEntriesResult = ZE_RESULT_ERROR_NOT_AVAILABLE;
    for (const auto &handle : pSysmanDeviceImp->pPowerHandleContext->handleList) {
        delete handle;
    }
    pSysmanDeviceImp->pPowerHandleContext->handleList.clear();
    pSysmanDeviceImp->pPowerHandleContext->init(deviceHandles, device->toHandle());
    auto handles = getPowerHandles(powerHandleComponentCount);

    for (auto &deviceHandle : deviceHandles) {
        ze_device_properties_t deviceProperties = {ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES};
        Device::fromHandle(deviceHandle)->getProperties(&deviceProperties);
        auto pPmt = static_cast<NiceMock<Mock<PowerPmt>> *>(pLinuxSysmanImp->getPlatformMonitoringTechAccess(deviceProperties.subdeviceId));
        pPmt->openFunction = openMockPower;
        pPmt->closeFunction = closeMockPower;
        pPmt->preadFunction = preadMockPower;
    }

    for (auto handle : handles) {
        zes_power_energy_counter_t energyCounter;
        uint64_t expectedEnergyCounter = convertJouleToMicroJoule * (setEnergyCounter / 1048576);
        ASSERT_EQ(ZE_RESULT_SUCCESS, zesPowerGetEnergyCounter(handle, &energyCounter));
        EXPECT_EQ(energyCounter.energy, expectedEnergyCounter);
    }
}

} // namespace ult
} // namespace L0
