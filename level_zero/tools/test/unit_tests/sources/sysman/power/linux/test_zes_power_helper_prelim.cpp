/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/test/unit_tests/sources/sysman/power/linux/mock_sysfs_power_prelim.h"

extern bool sysmanUltsEnable;

namespace L0 {
namespace ult {

constexpr uint32_t powerHandleComponentCountMultiDevice = 3u;

class SysmanDevicePowerMultiDeviceFixtureHelper : public SysmanDevicePowerMultiDeviceFixture {

  protected:
    void SetUp() override {
        if (!sysmanUltsEnable) {
            GTEST_SKIP();
        }
        SysmanDevicePowerMultiDeviceFixture::SetUp();
        for (const auto &handle : pSysmanDeviceImp->pPowerHandleContext->handleList) {
            delete handle;
        }
        pSysmanDeviceImp->pPowerHandleContext->handleList.clear();
        if (subDeviceCount == 0) {
            deviceHandles.resize(1, device->toHandle());
        } else {
            deviceHandles.resize(subDeviceCount, nullptr);
            Device::fromHandle(device->toHandle())->getSubDevices(&subDeviceCount, deviceHandles.data());
        }
    }

    void TearDown() override {
        if (!sysmanUltsEnable) {
            GTEST_SKIP();
        }
        SysmanDevicePowerMultiDeviceFixture::TearDown();
    }
};

TEST_F(SysmanDevicePowerMultiDeviceFixtureHelper, GivenInvalidComponentCountWhenEnumeratingPowerDomainsThenValidCountIsReturnedAndVerifySysmanPowerGetCallSucceeds) {
    uint32_t count = 0;
    EXPECT_EQ(zesDeviceEnumPowerDomains(device->toHandle(), &count, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(count, powerHandleComponentCountMultiDevice);

    count = count + 1;
    EXPECT_EQ(zesDeviceEnumPowerDomains(device->toHandle(), &count, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(count, powerHandleComponentCountMultiDevice);
}

TEST_F(SysmanDevicePowerMultiDeviceFixtureHelper, GivenValidPowerPointerWhenGettingCardPowerDomainWhenhwmonInterfaceExistsAndThenCallSucceds) {
    zes_pwr_handle_t phPower = {};
    EXPECT_EQ(zesDeviceGetCardPowerDomain(device->toHandle(), &phPower), ZE_RESULT_SUCCESS);
}

TEST_F(SysmanDevicePowerMultiDeviceFixtureHelper, GivenValidPowerHandleWhenGettingPowerPropertiesThenCallSucceeds) {
    auto handles = getPowerHandles(powerHandleComponentCountMultiDevice);
    for (auto handle : handles) {
        zes_power_properties_t properties;
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesPowerGetProperties(handle, &properties));
        if (properties.onSubdevice) {
            EXPECT_FALSE(properties.canControl);
            EXPECT_EQ(properties.defaultLimit, -1);

        } else {
            EXPECT_EQ(properties.canControl, true);
            EXPECT_EQ(properties.defaultLimit, (int32_t)(mockDefaultPowerLimitVal / milliFactor));
        }
        EXPECT_EQ(properties.isEnergyThresholdSupported, false);
        EXPECT_EQ(properties.maxLimit, -1);
        EXPECT_EQ(properties.minLimit, -1);
    }
}

TEST_F(SysmanDevicePowerMultiDeviceFixtureHelper, GivenScanDiectoriesFailAndPmtIsNullForSubDeviceZeroWhenGettingCardPowerThenReturnsFailure) {

    pSysfsAccess->mockscanDirEntriesResult = ZE_RESULT_ERROR_NOT_AVAILABLE;
    for (const auto &handle : pSysmanDeviceImp->pPowerHandleContext->handleList) {
        delete handle;
    }
    pSysmanDeviceImp->pPowerHandleContext->handleList.clear();
    for (auto &pmtMapElement : pLinuxSysmanImp->mapOfSubDeviceIdToPmtObject) {
        if (pmtMapElement.first == 0) {
            delete pmtMapElement.second;
            pmtMapElement.second = nullptr;
        }
    }
    pSysmanDeviceImp->pPowerHandleContext->init(deviceHandles, device->toHandle());

    zes_pwr_handle_t phPower = {};
    EXPECT_EQ(zesDeviceGetCardPowerDomain(device->toHandle(), &phPower), ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
}

TEST_F(SysmanDevicePowerMultiDeviceFixtureHelper, GivenValidPowerHandleWhenGettingPowerEnergyCounterThenValidPowerReadingsRetrieved) {
    auto handles = getPowerHandles(powerHandleComponentCountMultiDevice);

    for (auto handle : handles) {
        zes_power_properties_t properties;
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesPowerGetProperties(handle, &properties));
        zes_power_energy_counter_t energyCounter = {};
        ASSERT_EQ(ZE_RESULT_SUCCESS, zesPowerGetEnergyCounter(handle, &energyCounter));
        if (properties.subdeviceId == 0) {
            EXPECT_EQ(energyCounter.energy, expectedEnergyCounterTile0);
        } else if (properties.subdeviceId == 1) {
            EXPECT_EQ(energyCounter.energy, expectedEnergyCounterTile1);
        }
    }
}

TEST_F(SysmanDevicePowerMultiDeviceFixtureHelper, GivenSetPowerLimitsWhenGettingPowerLimitsThenLimitsSetEarlierAreRetrieved) {
    auto handles = getPowerHandles(powerHandleComponentCountMultiDevice);
    for (auto handle : handles) {
        zes_power_properties_t properties;
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

} // namespace ult
} // namespace L0
