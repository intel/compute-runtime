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
    FsAccess *pFsAccessOriginal = nullptr;
    OsPower *pOsPowerOriginal = nullptr;
    std::vector<ze_device_handle_t> deviceHandles;
    void SetUp() override {
        SysmanDeviceFixture::SetUp();
        pFsAccess = std::make_unique<NiceMock<Mock<PowerFsAccess>>>();
        pFsAccessOriginal = pLinuxSysmanImp->pFsAccess;
        pLinuxSysmanImp->pFsAccess = pFsAccess.get();
        ON_CALL(*pFsAccess.get(), listDirectory(_, _))
            .WillByDefault(::testing::Invoke(pFsAccess.get(), &Mock<PowerFsAccess>::listDirectorySuccess));
        ON_CALL(*pFsAccess.get(), getRealPath(_, _))
            .WillByDefault(::testing::Invoke(pFsAccess.get(), &Mock<PowerFsAccess>::getRealPathSuccess));

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
    }

    std::vector<zes_pwr_handle_t> get_power_handles(uint32_t count) {
        std::vector<zes_pwr_handle_t> handles(count, nullptr);
        EXPECT_EQ(zesDeviceEnumPowerDomains(device->toHandle(), &count, handles.data()), ZE_RESULT_SUCCESS);
        return handles;
    }
};

TEST_F(SysmanDevicePowerFixture, GivenComponentCountZeroWhenEnumeratingPowerDomainsThenValidCountIsReturnedAndVerifySysmanPowerGetCallSucceeds) {
    uint32_t count = 0;
    EXPECT_EQ(zesDeviceEnumPowerDomains(device->toHandle(), &count, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(count, powerHandleComponentCount);
}

TEST_F(SysmanDevicePowerFixture, GivenInvalidComponentCountWhenEnumeratingPowerDomainsThenValidCountIsReturnedAndVerifySysmanPowerGetCallSucceeds) {
    uint32_t count = 0;
    EXPECT_EQ(zesDeviceEnumPowerDomains(device->toHandle(), &count, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(count, powerHandleComponentCount);

    count = count + 1;
    EXPECT_EQ(zesDeviceEnumPowerDomains(device->toHandle(), &count, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(count, powerHandleComponentCount);
}

TEST_F(SysmanDevicePowerFixture, GivenComponentCountZeroWhenEnumeratingPowerDomainsThenValidPowerHandlesIsReturned) {
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
    auto handles = get_power_handles(powerHandleComponentCount);

    for (auto handle : handles) {
        zes_power_properties_t properties;
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesPowerGetProperties(handle, &properties));
        EXPECT_FALSE(properties.onSubdevice);
        EXPECT_EQ(properties.subdeviceId, 0u);
    }
}

TEST_F(SysmanDevicePowerFixture, GivenValidPowerHandleWhenGettingPowerEnergyCounterThenValidPowerReadingsRetrieved) {
    auto handles = get_power_handles(powerHandleComponentCount);

    for (auto handle : handles) {
        zes_power_energy_counter_t energyCounter;
        uint64_t expectedEnergyCounter = convertJouleToMicroJoule * setEnergyCounter;
        ASSERT_EQ(ZE_RESULT_SUCCESS, zesPowerGetEnergyCounter(handle, &energyCounter));
        EXPECT_EQ(energyCounter.energy, expectedEnergyCounter);
    }
}

TEST_F(SysmanDevicePowerFixture, GivenValidPowerHandleWhenGettingPowerEnergyThresholdThenUnsupportedFeatureErrorIsReturned) {
    zes_energy_threshold_t threshold;
    auto handles = get_power_handles(powerHandleComponentCount);
    for (auto handle : handles) {
        EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesPowerGetEnergyThreshold(handle, &threshold));
    }
}

TEST_F(SysmanDevicePowerFixture, GivenValidPowerHandleWhenSettingPowerEnergyThresholdThenUnsupportedFeatureErrorIsReturned) {
    double threshold = 0;
    auto handles = get_power_handles(powerHandleComponentCount);
    for (auto handle : handles) {
        EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesPowerSetEnergyThreshold(handle, threshold));
    }
}

TEST_F(SysmanDevicePowerFixture, GivenValidPowerHandleWhenGettingPowerLimitsThenUnsupportedFeatureErrorIsReturned) {
    auto handles = get_power_handles(powerHandleComponentCount);
    for (auto handle : handles) {
        zes_power_sustained_limit_t sustained;
        zes_power_burst_limit_t burst;
        zes_power_peak_limit_t peak;
        EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesPowerGetLimits(handle, &sustained, &burst, &peak));
    }
}

TEST_F(SysmanDevicePowerFixture, GivenValidPowerHandleWhenSettingPowerLimitsThenUnsupportedFeatureErrorIsReturned) {
    auto handles = get_power_handles(powerHandleComponentCount);
    for (auto handle : handles) {
        zes_power_sustained_limit_t sustained;
        zes_power_burst_limit_t burst;
        zes_power_peak_limit_t peak;
        EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesPowerSetLimits(handle, &sustained, &burst, &peak));
    }
}

} // namespace ult
} // namespace L0
