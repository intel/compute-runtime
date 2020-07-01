/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/tools/source/sysman/power/linux/os_power_imp.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "mock_sysfs_power.h"
#include "sysman/linux/os_sysman_imp.h"
#include "sysman/power/power_imp.h"

using ::testing::_;
using ::testing::NiceMock;

namespace L0 {
namespace ult {

constexpr uint64_t convertJouleToMicroJoule = 1000000u;
class SysmanPowerFixture : public DeviceFixture, public ::testing::Test {

  protected:
    std::unique_ptr<SysmanImp> sysmanImp;
    zet_sysman_handle_t hSysman;
    zet_sysman_pwr_handle_t hSysmanPowerHandle;
    Mock<PowerPmt> *pPmt = nullptr;

    PowerImp *pPowerImp = nullptr;
    PublicLinuxPowerImp linuxPowerImp;

    void SetUp() override {

        DeviceFixture::SetUp();
        sysmanImp = std::make_unique<SysmanImp>(device->toHandle());
        pPmt = new NiceMock<Mock<PowerPmt>>;
        OsPower *pOsPower = nullptr;

        FsAccess *pFsAccess = nullptr;
        const std::string deviceName("device");
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
    zet_power_energy_counter_t energyCounter;
    uint64_t expectedEnergyCounter = convertJouleToMicroJoule * setEnergyCounter;
    ASSERT_EQ(ZE_RESULT_SUCCESS, zetSysmanPowerGetEnergyCounter(hSysmanPowerHandle, &energyCounter));
    EXPECT_EQ(energyCounter.energy, expectedEnergyCounter);
}

TEST_F(SysmanPowerFixture, GivenValidPowerHandleWhenGettingPowerEnergyThresholdThenUnsupportedFeatureErrorIsReturned) {
    zet_energy_threshold_t threshold;

    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zetSysmanPowerGetEnergyThreshold(hSysmanPowerHandle, &threshold));
}

TEST_F(SysmanPowerFixture, GivenValidPowerHandleWhenSettingPowerEnergyThresholdThenUnsupportedFeatureErrorIsReturned) {
    double threshold = 0;

    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zetSysmanPowerSetEnergyThreshold(hSysmanPowerHandle, threshold));
}

} // namespace ult
} // namespace L0
