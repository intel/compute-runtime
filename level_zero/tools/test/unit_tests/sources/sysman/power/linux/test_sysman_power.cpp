/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/sysman/power/linux/os_power_imp.h"
#include "level_zero/tools/test/unit_tests/sources/sysman/mock_sysman_fixture.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "mock_sysfs_power.h"
#include "sysman/power/power_imp.h"

namespace L0 {
namespace ult {

constexpr uint64_t convertJouleToMicroJoule = 1000000u;
constexpr uint32_t powerHandleComponentCount = 1u;
class SysmanDevicePowerFixture : public SysmanDeviceFixture {
  protected:
    std::unique_ptr<Mock<PowerPmt>> pPmt;
    PlatformMonitoringTech *pPmtOld = nullptr;
    void SetUp() override {
        SysmanDeviceFixture::SetUp();
        pPmtOld = pLinuxSysmanImp->pPmt;
        pPmt = std::make_unique<NiceMock<Mock<PowerPmt>>>();
        pPmt->init(deviceName, pFsAccess);
        pLinuxSysmanImp->pPmt = pPmt.get();
        pSysmanDeviceImp->pPowerHandleContext->init();
    }
    void TearDown() override {
        SysmanDeviceFixture::TearDown();
        pLinuxSysmanImp->pPmt = pPmtOld;
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

class SysmanPowerFixture : public DeviceFixture, public ::testing::Test {

  protected:
    std::unique_ptr<SysmanDeviceImp> sysmanImp;
    zet_sysman_handle_t hSysman;
    zes_pwr_handle_t hSysmanPowerHandle;
    Mock<PowerPmt> *pPmt = nullptr;

    PowerImp *pPowerImp = nullptr;
    PublicLinuxPowerImp linuxPowerImp;

    void SetUp() override {

        DeviceFixture::SetUp();
        sysmanImp = std::make_unique<SysmanDeviceImp>(device->toHandle());
        pPmt = new NiceMock<Mock<PowerPmt>>;
        OsPower *pOsPower = nullptr;

        pPmt->init(deviceName, pFsAccess);
        linuxPowerImp.pPmt = pPmt;
        pOsPower = static_cast<OsPower *>(&linuxPowerImp);
        pPowerImp = new PowerImp();
        pPowerImp->pOsPower = pOsPower;

        pPowerImp->init();
        sysmanImp->pPowerHandleContext->handleList.push_back(pPowerImp);
        hSysman = sysmanImp->toHandle();
        hSysmanPowerHandle = pPowerImp->toHandle();
    }
    void TearDown() override {
        // pOsPower is static_cast of LinuxPowerImp class , hence in cleanup assign to nullptr
        pPowerImp->pOsPower = nullptr;

        if (pPmt != nullptr) {
            delete pPmt;
            pPmt = nullptr;
        }
        DeviceFixture::TearDown();
    }
};

TEST_F(SysmanPowerFixture, GivenComponentCountZeroWhenCallingZetSysmanPowerGetThenZeroCountIsReturnedAndVerifySysmanPowerGetCallSucceeds) {
    uint32_t count = 0;
    ze_result_t result = zetSysmanPowerGet(hSysman, &count, NULL);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(count, 1u);

    uint32_t testcount = count + 1;

    result = zetSysmanPowerGet(hSysman, &testcount, NULL);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(testcount, count);
}

TEST_F(SysmanPowerFixture, GivenValidPowerHandleWhenGettingPowerEnergyCounterThenValidPowerReadingsRetrieved) {
    zes_power_energy_counter_t energyCounter;
    uint64_t expectedEnergyCounter = convertJouleToMicroJoule * setEnergyCounter;
    ASSERT_EQ(ZE_RESULT_SUCCESS, zetSysmanPowerGetEnergyCounter(hSysmanPowerHandle, &energyCounter));
    EXPECT_EQ(energyCounter.energy, expectedEnergyCounter);
}

TEST_F(SysmanPowerFixture, GivenValidPowerHandleWhenGettingPowerEnergyThresholdThenUnsupportedFeatureErrorIsReturned) {
    zes_energy_threshold_t threshold;

    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zetSysmanPowerGetEnergyThreshold(hSysmanPowerHandle, &threshold));
}

TEST_F(SysmanPowerFixture, GivenValidPowerHandleWhenSettingPowerEnergyThresholdThenUnsupportedFeatureErrorIsReturned) {
    double threshold = 0;

    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zetSysmanPowerSetEnergyThreshold(hSysmanPowerHandle, threshold));
}

} // namespace ult
} // namespace L0
