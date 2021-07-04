/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/sysman/power/linux/os_power_imp.h"
#include "level_zero/tools/test/unit_tests/sources/sysman/linux/mock_sysman_fixture.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "mock_sysfs_power.h"
#include "sysman/power/power_imp.h"

using ::testing::DoDefault;
using ::testing::Matcher;
using ::testing::Return;

namespace L0 {
namespace ult {

constexpr uint64_t convertJouleToMicroJoule = 1000000u;
constexpr uint32_t powerHandleComponentCount = 1u;
const std::map<std::string, uint64_t> deviceKeyOffsetMapPower = {
    {"PACKAGE_ENERGY", 0x400},
    {"COMPUTE_TEMPERATURES", 0x68},
    {"SOC_TEMPERATURES", 0x60},
    {"CORE_TEMPERATURES", 0x6c}};

class SysmanDevicePowerFixture : public SysmanDeviceFixture {
  protected:
    std::unique_ptr<PublicLinuxPowerImp> pPublicLinuxPowerImp;
    std::unique_ptr<Mock<PowerPmt>> pPmt;
    std::unique_ptr<Mock<PowerFsAccess>> pFsAccess;
    std::unique_ptr<Mock<PowerSysfsAccess>> pSysfsAccess;
    SysfsAccess *pSysfsAccessOld = nullptr;
    FsAccess *pFsAccessOriginal = nullptr;
    OsPower *pOsPowerOriginal = nullptr;
    std::vector<ze_device_handle_t> deviceHandles;
    void SetUp() override {
        SysmanDeviceFixture::SetUp();
        pFsAccess = std::make_unique<NiceMock<Mock<PowerFsAccess>>>();
        pFsAccessOriginal = pLinuxSysmanImp->pFsAccess;
        pLinuxSysmanImp->pFsAccess = pFsAccess.get();
        pSysfsAccessOld = pLinuxSysmanImp->pSysfsAccess;
        pSysfsAccess = std::make_unique<NiceMock<Mock<PowerSysfsAccess>>>();
        pLinuxSysmanImp->pSysfsAccess = pSysfsAccess.get();
        ON_CALL(*pFsAccess.get(), listDirectory(_, _))
            .WillByDefault(::testing::Invoke(pFsAccess.get(), &Mock<PowerFsAccess>::listDirectorySuccess));
        ON_CALL(*pFsAccess.get(), getRealPath(_, _))
            .WillByDefault(::testing::Invoke(pFsAccess.get(), &Mock<PowerFsAccess>::getRealPathSuccess));
        ON_CALL(*pSysfsAccess.get(), read(_, Matcher<std::string &>(_)))
            .WillByDefault(::testing::Invoke(pSysfsAccess.get(), &Mock<PowerSysfsAccess>::getValString));
        ON_CALL(*pSysfsAccess.get(), read(_, Matcher<uint64_t &>(_)))
            .WillByDefault(::testing::Invoke(pSysfsAccess.get(), &Mock<PowerSysfsAccess>::getValUnsignedLong));
        ON_CALL(*pSysfsAccess.get(), read(_, Matcher<uint32_t &>(_)))
            .WillByDefault(::testing::Invoke(pSysfsAccess.get(), &Mock<PowerSysfsAccess>::getValUnsignedInt));
        ON_CALL(*pSysfsAccess.get(), write(_, _))
            .WillByDefault(::testing::Invoke(pSysfsAccess.get(), &Mock<PowerSysfsAccess>::setVal));
        ON_CALL(*pSysfsAccess.get(), scanDirEntries(_, _))
            .WillByDefault(::testing::Invoke(pSysfsAccess.get(), &Mock<PowerSysfsAccess>::getscanDirEntries));

        uint32_t subDeviceCount = 0;
        Device::fromHandle(device->toHandle())->getSubDevices(&subDeviceCount, nullptr);
        if (subDeviceCount == 0) {
            deviceHandles.resize(1, device->toHandle());
        } else {
            deviceHandles.resize(subDeviceCount, nullptr);
            Device::fromHandle(device->toHandle())->getSubDevices(&subDeviceCount, deviceHandles.data());
        }

        for (auto &deviceHandle : deviceHandles) {
            ze_device_properties_t deviceProperties = {ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES};
            Device::fromHandle(deviceHandle)->getProperties(&deviceProperties);
            auto pPmt = new NiceMock<Mock<PowerPmt>>(pFsAccess.get(), deviceProperties.flags & ZE_DEVICE_PROPERTY_FLAG_SUBDEVICE,
                                                     deviceProperties.subdeviceId);
            pPmt->mockedInit(pFsAccess.get());
            pPmt->keyOffsetMap = deviceKeyOffsetMapPower;
            pLinuxSysmanImp->mapOfSubDeviceIdToPmtObject.emplace(deviceProperties.subdeviceId, pPmt);
        }

        pSysmanDeviceImp->pPowerHandleContext->init();
    }
    void TearDown() override {
        SysmanDeviceFixture::TearDown();
        pLinuxSysmanImp->pFsAccess = pFsAccessOriginal;
        pLinuxSysmanImp->pSysfsAccess = pSysfsAccessOld;
    }

    std::vector<zes_pwr_handle_t> getPowerHandles(uint32_t count) {
        std::vector<zes_pwr_handle_t> handles(count, nullptr);
        EXPECT_EQ(zesDeviceEnumPowerDomains(device->toHandle(), &count, handles.data()), ZE_RESULT_SUCCESS);
        return handles;
    }
};

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
        EXPECT_EQ(properties.maxLimit, (int32_t)(mockMaxPowerLimitVal / milliFactor));
        EXPECT_EQ(properties.minLimit, (int32_t)(mockMinPowerLimitVal / milliFactor));
    }
}

TEST_F(SysmanDevicePowerFixture, GivenValidPowerHandleWhenGettingPowerPropertiesWhenHwmonInterfaceExistThenLimitsReturnsUnkown) {

    EXPECT_CALL(*pSysfsAccess.get(), read(_, Matcher<uint32_t &>(_)))
        .WillRepeatedly(Return(ZE_RESULT_ERROR_NOT_AVAILABLE));
    for (const auto &handle : pSysmanDeviceImp->pPowerHandleContext->handleList) {
        delete handle;
    }
    pSysmanDeviceImp->pPowerHandleContext->handleList.clear();
    pSysmanDeviceImp->pPowerHandleContext->init();
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

    EXPECT_CALL(*pSysfsAccess.get(), read(_, Matcher<uint32_t &>(_)))
        .WillOnce(Return(ZE_RESULT_ERROR_NOT_AVAILABLE))
        .WillOnce(Return(ZE_RESULT_ERROR_NOT_AVAILABLE))
        .WillOnce(::testing::Invoke(pSysfsAccess.get(), &Mock<PowerSysfsAccess>::getValUnsignedIntMax))
        .WillRepeatedly(DoDefault());
    for (const auto &handle : pSysmanDeviceImp->pPowerHandleContext->handleList) {
        delete handle;
    }
    pSysmanDeviceImp->pPowerHandleContext->handleList.clear();
    pSysmanDeviceImp->pPowerHandleContext->init();
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

TEST_F(SysmanDevicePowerFixture, GivenValidPowerHandleWhenGettingPowerEnergyCounterWhenHwmonInterfaceExistThenValidPowerReadingsRetrieved) {
    auto handles = getPowerHandles(powerHandleComponentCount);

    for (auto handle : handles) {
        zes_power_energy_counter_t energyCounter = {};
        ASSERT_EQ(ZE_RESULT_SUCCESS, zesPowerGetEnergyCounter(handle, &energyCounter));
        EXPECT_EQ(energyCounter.energy, expectedEnergyCounter);
    }
}

TEST_F(SysmanDevicePowerFixture, GivenValidPowerHandleWhenGettingPowerEnergyCounterFailedWhenHwmonInterfaceExistThenValidErrorCodeReturned) {
    auto handles = getPowerHandles(powerHandleComponentCount);

    ON_CALL(*pSysfsAccess.get(), read(_, Matcher<uint64_t &>(_)))
        .WillByDefault(::testing::Invoke(pSysfsAccess.get(), &Mock<PowerSysfsAccess>::getValUnsignedLongReturnErrorForEnergyCounter));
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
        sustainedSet.interval = 10;
        sustainedSet.power = 300000;
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesPowerSetLimits(handle, &sustainedSet, nullptr, nullptr));
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesPowerGetLimits(handle, &sustainedGet, nullptr, nullptr));
        EXPECT_EQ(sustainedGet.power, sustainedSet.power);
        EXPECT_EQ(sustainedGet.interval, sustainedSet.interval);

        zes_power_burst_limit_t burstSet = {};
        zes_power_burst_limit_t burstGet = {};
        burstSet.enabled = 1;
        burstSet.power = 375000;
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesPowerSetLimits(handle, nullptr, &burstSet, nullptr));
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesPowerGetLimits(handle, nullptr, &burstGet, nullptr));
        EXPECT_EQ(burstSet.enabled, burstGet.enabled);
        EXPECT_EQ(burstSet.power, burstGet.power);

        burstSet.enabled = 0;
        burstGet.enabled = 0;
        burstGet.power = 0;
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesPowerSetLimits(handle, nullptr, &burstSet, nullptr));
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesPowerGetLimits(handle, nullptr, &burstGet, nullptr));
        EXPECT_EQ(burstSet.enabled, burstGet.enabled);
        EXPECT_EQ(burstGet.power, 0);

        zes_power_peak_limit_t peakGet = {};
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesPowerGetLimits(handle, nullptr, nullptr, &peakGet));
        EXPECT_EQ(peakGet.powerAC, -1);
        EXPECT_EQ(peakGet.powerDC, -1);
    }
}

TEST_F(SysmanDevicePowerFixture, GivenGetPowerLimitsReturnErrorWhenGettingPowerLimitsWhenHwmonInterfaceExistForBurstPowerLimitThenProperErrorCodesReturned) {
    auto handles = getPowerHandles(powerHandleComponentCount);

    EXPECT_CALL(*pSysfsAccess.get(), read(_, Matcher<uint64_t &>(_)))
        .WillOnce(::testing::Invoke(pSysfsAccess.get(), &Mock<PowerSysfsAccess>::getValUnsignedLongReturnErrorForBurstPowerLimit))
        .WillOnce(Return(ZE_RESULT_ERROR_NOT_AVAILABLE));
    for (auto handle : handles) {
        zes_power_burst_limit_t burstSet = {};
        zes_power_burst_limit_t burstGet = {};
        burstSet.enabled = 1;
        burstSet.power = 375000;
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesPowerSetLimits(handle, nullptr, &burstSet, nullptr));
        EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesPowerGetLimits(handle, nullptr, &burstGet, nullptr));
    }
}

TEST_F(SysmanDevicePowerFixture, GivenGetPowerLimitsReturnErrorWhenGettingPowerLimitsWhenHwmonInterfaceExistForBurstPowerLimitThenProperErrorCodesIsReturned) {
    auto handles = getPowerHandles(powerHandleComponentCount);

    ON_CALL(*pSysfsAccess.get(), read(_, Matcher<uint64_t &>(_)))
        .WillByDefault(::testing::Invoke(pSysfsAccess.get(), &Mock<PowerSysfsAccess>::getValUnsignedLongReturnErrorUnknown));

    for (auto handle : handles) {
        zes_power_burst_limit_t burstSet = {};
        zes_power_burst_limit_t burstGet = {};
        burstSet.enabled = 1;
        burstSet.power = 375000;
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesPowerSetLimits(handle, nullptr, &burstSet, nullptr));
        EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, zesPowerGetLimits(handle, nullptr, &burstGet, nullptr));
    }
}

TEST_F(SysmanDevicePowerFixture, GivenSetPowerLimitsReturnErrorWhenSettingPowerLimitsWhenHwmonInterfaceExistForBurstPowerLimitThenProperErrorCodesReturned) {
    auto handles = getPowerHandles(powerHandleComponentCount);

    ON_CALL(*pSysfsAccess.get(), write(_, _))
        .WillByDefault(::testing::Invoke(pSysfsAccess.get(), &Mock<PowerSysfsAccess>::setValUnsignedLongReturnErrorForBurstPowerLimit));

    for (auto handle : handles) {
        zes_power_burst_limit_t burstSet = {};
        burstSet.enabled = 1;
        EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesPowerSetLimits(handle, nullptr, &burstSet, nullptr));
    }
}

TEST_F(SysmanDevicePowerFixture, GivenSetPowerLimitsReturnErrorWhenSettingPowerLimitsWhenHwmonInterfaceExistForBurstPowerLimitEnabledThenProperErrorCodesReturned) {
    auto handles = getPowerHandles(powerHandleComponentCount);

    ON_CALL(*pSysfsAccess.get(), write(_, _))
        .WillByDefault(::testing::Invoke(pSysfsAccess.get(), &Mock<PowerSysfsAccess>::setValUnsignedLongReturnErrorForBurstPowerLimitEnabled));
    ON_CALL(*pSysfsAccess.get(), read(_, Matcher<uint64_t &>(_)))
        .WillByDefault(::testing::Invoke(pSysfsAccess.get(), &Mock<PowerSysfsAccess>::getValUnsignedLongReturnErrorForBurstPowerLimitEnabled));

    for (auto handle : handles) {
        zes_power_burst_limit_t burstSet = {};
        zes_power_burst_limit_t burstGet = {};
        burstSet.enabled = 1;
        EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesPowerSetLimits(handle, nullptr, &burstSet, nullptr));
        EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesPowerGetLimits(handle, nullptr, &burstGet, nullptr));
    }
}

TEST_F(SysmanDevicePowerFixture, GivenReadingSustainedPowerLimitNodeReturnErrorWhenSetOrGetPowerLimitsWhenHwmonInterfaceExistForSustainedPowerLimitEnabledThenProperErrorCodesReturned) {
    auto handles = getPowerHandles(powerHandleComponentCount);

    ON_CALL(*pSysfsAccess.get(), read(_, Matcher<uint64_t &>(_)))
        .WillByDefault(::testing::Invoke(pSysfsAccess.get(), &Mock<PowerSysfsAccess>::getValUnsignedLongReturnErrorForSustainedPowerLimitEnabled));

    for (auto handle : handles) {
        zes_power_sustained_limit_t sustainedSet = {};
        zes_power_sustained_limit_t sustainedGet = {};

        EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesPowerSetLimits(handle, &sustainedSet, nullptr, nullptr));
        EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesPowerGetLimits(handle, &sustainedGet, nullptr, nullptr));
    }
}

TEST_F(SysmanDevicePowerFixture, GivenReadingSustainedPowerNodeReturnErrorWhenGetPowerLimitsForSustainedPowerWhenHwmonInterfaceExistThenProperErrorCodesReturned) {
    auto handles = getPowerHandles(powerHandleComponentCount);

    ON_CALL(*pSysfsAccess.get(), read(_, Matcher<uint64_t &>(_)))
        .WillByDefault(::testing::Invoke(pSysfsAccess.get(), &Mock<PowerSysfsAccess>::getValUnsignedLongReturnErrorForSustainedPower));

    for (auto handle : handles) {
        zes_power_sustained_limit_t sustainedGet = {};
        EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesPowerGetLimits(handle, &sustainedGet, nullptr, nullptr));
    }
}

TEST_F(SysmanDevicePowerFixture, GivenReadingSustainedPowerIntervalNodeReturnErrorWhenGetPowerLimitsForSustainedPowerWhenHwmonInterfaceExistThenProperErrorCodesReturned) {
    auto handles = getPowerHandles(powerHandleComponentCount);

    ON_CALL(*pSysfsAccess.get(), read(_, Matcher<uint64_t &>(_)))
        .WillByDefault(::testing::Invoke(pSysfsAccess.get(), &Mock<PowerSysfsAccess>::getValUnsignedLongReturnErrorForSustainedPowerInterval));

    for (auto handle : handles) {
        zes_power_sustained_limit_t sustainedGet = {};
        EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesPowerGetLimits(handle, &sustainedGet, nullptr, nullptr));
    }
}

TEST_F(SysmanDevicePowerFixture, GivenwritingSustainedPowerNodeReturnErrorWhenSetPowerLimitsForSustainedPowerWhenHwmonInterfaceExistThenProperErrorCodesReturned) {
    auto handles = getPowerHandles(powerHandleComponentCount);

    ON_CALL(*pSysfsAccess.get(), write(_, _))
        .WillByDefault(::testing::Invoke(pSysfsAccess.get(), &Mock<PowerSysfsAccess>::setValReturnErrorForSustainedPower));

    zes_power_sustained_limit_t sustainedSet = {};
    sustainedSet.interval = 10;
    sustainedSet.power = 300000;
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesPowerSetLimits(handles[0], &sustainedSet, nullptr, nullptr));
}

TEST_F(SysmanDevicePowerFixture, GivenwritingSustainedPowerIntervalNodeReturnErrorWhenSetPowerLimitsForSustainedPowerIntervalWhenHwmonInterfaceExistThenProperErrorCodesReturned) {
    auto handles = getPowerHandles(powerHandleComponentCount);

    ON_CALL(*pSysfsAccess.get(), write(_, _))
        .WillByDefault(::testing::Invoke(pSysfsAccess.get(), &Mock<PowerSysfsAccess>::setValReturnErrorForSustainedPowerInterval));

    zes_power_sustained_limit_t sustainedSet = {};
    sustainedSet.interval = 10;
    sustainedSet.power = 300000;
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesPowerSetLimits(handles[0], &sustainedSet, nullptr, nullptr));
}

TEST_F(SysmanDevicePowerFixture, GivenGetPowerLimitsWhenPowerLimitsAreDisabledWhenHwmonInterfaceExistThenAllPowerValuesAreIgnored) {
    auto handles = getPowerHandles(powerHandleComponentCount);

    ON_CALL(*pSysfsAccess.get(), read(_, Matcher<uint64_t &>(_)))
        .WillByDefault(::testing::Invoke(pSysfsAccess.get(), &Mock<PowerSysfsAccess>::getValUnsignedLongReturnsPowerLimitEnabledAsDisabled));

    zes_power_sustained_limit_t sustainedGet = {};
    zes_power_burst_limit_t burstGet = {};
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesPowerGetLimits(handles[0], &sustainedGet, nullptr, nullptr));
    EXPECT_EQ(sustainedGet.interval, 0);
    EXPECT_EQ(sustainedGet.power, 0);
    EXPECT_EQ(sustainedGet.enabled, 0);
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesPowerGetLimits(handles[0], nullptr, &burstGet, nullptr));
    EXPECT_EQ(burstGet.enabled, 0);
    EXPECT_EQ(burstGet.power, 0);
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesPowerSetLimits(handles[0], &sustainedGet, nullptr, nullptr));
    zes_power_burst_limit_t burstSet = {};
    burstSet.enabled = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesPowerSetLimits(handles[0], nullptr, &burstSet, nullptr));
}

TEST_F(SysmanDevicePowerFixture, GivenScanDiectoriesFailAndPmtIsNotNullPointerThenPowerModuleIsSupported) {
    ON_CALL(*pSysfsAccess.get(), scanDirEntries(_, _))
        .WillByDefault(::testing::Invoke(pSysfsAccess.get(), &Mock<PowerSysfsAccess>::getscanDirEntriesStatusReturnError));
    pSysmanDeviceImp->pPowerHandleContext->init();
    PublicLinuxPowerImp *pPowerImp = new PublicLinuxPowerImp(pOsSysman);
    EXPECT_TRUE(pPowerImp->isPowerModuleSupported());
    delete pPowerImp;
}

TEST_F(SysmanDevicePowerFixture, GivenComponentCountZeroWhenEnumeratingPowerDomainsThenValidCountIsReturnedAndVerifySysmanPowerGetCallSucceeds) {
    ON_CALL(*pSysfsAccess.get(), scanDirEntries(_, _))
        .WillByDefault(::testing::Invoke(pSysfsAccess.get(), &Mock<PowerSysfsAccess>::getscanDirEntriesStatusReturnError));
    for (const auto &handle : pSysmanDeviceImp->pPowerHandleContext->handleList) {
        delete handle;
    }
    pSysmanDeviceImp->pPowerHandleContext->handleList.clear();
    pSysmanDeviceImp->pPowerHandleContext->init();
    uint32_t count = 0;
    EXPECT_EQ(zesDeviceEnumPowerDomains(device->toHandle(), &count, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(count, powerHandleComponentCount);
}

TEST_F(SysmanDevicePowerFixture, GivenInvalidComponentCountWhenEnumeratingPowerDomainsThenValidCountIsReturnedAndVerifySysmanPowerGetCallSucceeds) {
    ON_CALL(*pSysfsAccess.get(), scanDirEntries(_, _))
        .WillByDefault(::testing::Invoke(pSysfsAccess.get(), &Mock<PowerSysfsAccess>::getscanDirEntriesStatusReturnError));
    for (const auto &handle : pSysmanDeviceImp->pPowerHandleContext->handleList) {
        delete handle;
    }
    pSysmanDeviceImp->pPowerHandleContext->handleList.clear();
    pSysmanDeviceImp->pPowerHandleContext->init();
    uint32_t count = 0;
    EXPECT_EQ(zesDeviceEnumPowerDomains(device->toHandle(), &count, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(count, powerHandleComponentCount);

    count = count + 1;
    EXPECT_EQ(zesDeviceEnumPowerDomains(device->toHandle(), &count, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(count, powerHandleComponentCount);
}

TEST_F(SysmanDevicePowerFixture, GivenComponentCountZeroWhenEnumeratingPowerDomainsThenValidPowerHandlesIsReturned) {
    ON_CALL(*pSysfsAccess.get(), scanDirEntries(_, _))
        .WillByDefault(::testing::Invoke(pSysfsAccess.get(), &Mock<PowerSysfsAccess>::getscanDirEntriesStatusReturnError));
    for (const auto &handle : pSysmanDeviceImp->pPowerHandleContext->handleList) {
        delete handle;
    }
    pSysmanDeviceImp->pPowerHandleContext->handleList.clear();
    pSysmanDeviceImp->pPowerHandleContext->init();
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
    ON_CALL(*pSysfsAccess.get(), scanDirEntries(_, _))
        .WillByDefault(::testing::Invoke(pSysfsAccess.get(), &Mock<PowerSysfsAccess>::getscanDirEntriesStatusReturnError));
    for (const auto &handle : pSysmanDeviceImp->pPowerHandleContext->handleList) {
        delete handle;
    }
    pSysmanDeviceImp->pPowerHandleContext->handleList.clear();
    pSysmanDeviceImp->pPowerHandleContext->init();
    auto handles = getPowerHandles(powerHandleComponentCount);

    for (auto handle : handles) {
        zes_power_properties_t properties;
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesPowerGetProperties(handle, &properties));
        EXPECT_FALSE(properties.onSubdevice);
        EXPECT_EQ(properties.subdeviceId, 0u);
    }
}

TEST_F(SysmanDevicePowerFixture, GivenValidPowerHandleWhenGettingPowerEnergyCounterThenValidPowerReadingsRetrieved) {
    ON_CALL(*pSysfsAccess.get(), scanDirEntries(_, _))
        .WillByDefault(::testing::Invoke(pSysfsAccess.get(), &Mock<PowerSysfsAccess>::getscanDirEntriesStatusReturnError));
    for (const auto &handle : pSysmanDeviceImp->pPowerHandleContext->handleList) {
        delete handle;
    }
    pSysmanDeviceImp->pPowerHandleContext->handleList.clear();
    pSysmanDeviceImp->pPowerHandleContext->init();
    auto handles = getPowerHandles(powerHandleComponentCount);

    for (auto handle : handles) {
        zes_power_energy_counter_t energyCounter;
        uint64_t expectedEnergyCounter = convertJouleToMicroJoule * (setEnergyCounter / 1048576);
        ASSERT_EQ(ZE_RESULT_SUCCESS, zesPowerGetEnergyCounter(handle, &energyCounter));
        EXPECT_EQ(energyCounter.energy, expectedEnergyCounter);
    }
}

TEST_F(SysmanDevicePowerFixture, GivenValidPowerHandleWhenGettingPowerEnergyThresholdThenUnsupportedFeatureErrorIsReturned) {
    ON_CALL(*pSysfsAccess.get(), scanDirEntries(_, _))
        .WillByDefault(::testing::Invoke(pSysfsAccess.get(), &Mock<PowerSysfsAccess>::getscanDirEntriesStatusReturnError));
    for (const auto &handle : pSysmanDeviceImp->pPowerHandleContext->handleList) {
        delete handle;
    }
    pSysmanDeviceImp->pPowerHandleContext->handleList.clear();
    pSysmanDeviceImp->pPowerHandleContext->init();
    zes_energy_threshold_t threshold;
    auto handles = getPowerHandles(powerHandleComponentCount);
    for (auto handle : handles) {
        EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesPowerGetEnergyThreshold(handle, &threshold));
    }
}

TEST_F(SysmanDevicePowerFixture, GivenValidPowerHandleWhenSettingPowerEnergyThresholdThenUnsupportedFeatureErrorIsReturned) {
    ON_CALL(*pSysfsAccess.get(), scanDirEntries(_, _))
        .WillByDefault(::testing::Invoke(pSysfsAccess.get(), &Mock<PowerSysfsAccess>::getscanDirEntriesStatusReturnError));
    for (const auto &handle : pSysmanDeviceImp->pPowerHandleContext->handleList) {
        delete handle;
    }
    pSysmanDeviceImp->pPowerHandleContext->handleList.clear();
    pSysmanDeviceImp->pPowerHandleContext->init();
    double threshold = 0;
    auto handles = getPowerHandles(powerHandleComponentCount);
    for (auto handle : handles) {
        EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesPowerSetEnergyThreshold(handle, threshold));
    }
}

TEST_F(SysmanDevicePowerFixture, GivenValidPowerHandleWhenGettingPowerLimitsThenUnsupportedFeatureErrorIsReturned) {
    ON_CALL(*pSysfsAccess.get(), scanDirEntries(_, _))
        .WillByDefault(::testing::Invoke(pSysfsAccess.get(), &Mock<PowerSysfsAccess>::getscanDirEntriesStatusReturnError));
    for (const auto &handle : pSysmanDeviceImp->pPowerHandleContext->handleList) {
        delete handle;
    }
    pSysmanDeviceImp->pPowerHandleContext->handleList.clear();
    pSysmanDeviceImp->pPowerHandleContext->init();
    auto handles = getPowerHandles(powerHandleComponentCount);
    for (auto handle : handles) {
        zes_power_sustained_limit_t sustained;
        zes_power_burst_limit_t burst;
        zes_power_peak_limit_t peak;
        EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesPowerGetLimits(handle, &sustained, &burst, &peak));
    }
}

TEST_F(SysmanDevicePowerFixture, GivenValidPowerHandleWhenSettingPowerLimitsThenUnsupportedFeatureErrorIsReturned) {
    ON_CALL(*pSysfsAccess.get(), scanDirEntries(_, _))
        .WillByDefault(::testing::Invoke(pSysfsAccess.get(), &Mock<PowerSysfsAccess>::getscanDirEntriesStatusReturnError));
    for (const auto &handle : pSysmanDeviceImp->pPowerHandleContext->handleList) {
        delete handle;
    }
    pSysmanDeviceImp->pPowerHandleContext->handleList.clear();
    pSysmanDeviceImp->pPowerHandleContext->init();
    auto handles = getPowerHandles(powerHandleComponentCount);
    for (auto handle : handles) {
        zes_power_sustained_limit_t sustained;
        zes_power_burst_limit_t burst;
        zes_power_peak_limit_t peak;
        EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesPowerSetLimits(handle, &sustained, &burst, &peak));
    }
}

} // namespace ult
} // namespace L0
