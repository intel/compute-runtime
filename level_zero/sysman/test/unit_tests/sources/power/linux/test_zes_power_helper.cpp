/*
 * Copyright (C) 2021-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/sysman/test/unit_tests/sources/power/linux/mock_sysfs_power.h"

namespace L0 {
namespace Sysman {
namespace ult {

using SysmanDevicePowerFixtureHelper = SysmanDevicePowerFixtureI915;

TEST_F(SysmanDevicePowerFixtureHelper, GivenValidPowerHandleWhenGettingPowerEnergyCounterThenValidPowerReadingsRetrieved) {

    auto pSysFsAccess = std::make_unique<MockPowerSysfsAccessInterface>();
    auto pPmt = static_cast<MockPowerPmt *>(pLinuxSysmanImp->getPlatformMonitoringTechAccess(0));
    std::unique_ptr<PublicLinuxPowerImp> pLinuxPowerImp(new PublicLinuxPowerImp(pOsSysman, false, 0));
    pLinuxPowerImp->pSysfsAccess = pSysFsAccess.get();
    pLinuxPowerImp->pPmt = pPmt;
    pLinuxPowerImp->isPowerModuleSupported();

    zes_power_energy_counter_t energyCounter = {};
    ASSERT_EQ(ZE_RESULT_SUCCESS, pLinuxPowerImp->getEnergyCounter(&energyCounter));
    EXPECT_EQ(energyCounter.energy, expectedEnergyCounter);
}

using SysmanDevicePowerMultiDeviceFixtureHelper = SysmanDevicePowerMultiDeviceFixture;

TEST_F(SysmanDevicePowerMultiDeviceFixtureHelper, GivenValidDeviceHandlesAndHwmonInterfaceExistThenSuccessIsReturned) {
    auto subDeviceCount = pLinuxSysmanImp->getSubDeviceCount();
    uint32_t subdeviceId = 0;
    do {
        ze_bool_t onSubdevice = (subDeviceCount == 0) ? false : true;
        PublicLinuxPowerImp *pPowerImp = new PublicLinuxPowerImp(pOsSysman, onSubdevice, subdeviceId);
        EXPECT_TRUE(pPowerImp->isPowerModuleSupported());
        delete pPowerImp;

    } while (++subdeviceId < subDeviceCount);
}

TEST_F(SysmanDevicePowerMultiDeviceFixtureHelper, GivenValidPowerPointerWhenGettingCardPowerDomainWhenhwmonInterfaceExistsAndThenCallSucceeds) {
    zes_pwr_handle_t phPower = {};
    EXPECT_EQ(zesDeviceGetCardPowerDomain(device->toHandle(), &phPower), ZE_RESULT_SUCCESS);
}

TEST_F(SysmanDevicePowerMultiDeviceFixtureHelper, GivenScanDirectoriesFailAndPmtIsNullForSubDeviceZeroWhenGettingCardPowerThenReturnsFailure) {

    pSysfsAccess->mockScanDirEntriesReturnStatus.push_back(ZE_RESULT_ERROR_NOT_AVAILABLE);
    pSysfsAccess->isRepeated = true;

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
    pSysmanDeviceImp->pPowerHandleContext->init(pLinuxSysmanImp->getSubDeviceCount());

    zes_pwr_handle_t phPower = {};
    EXPECT_EQ(zesDeviceGetCardPowerDomain(device->toHandle(), &phPower), ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
}

} // namespace ult
} // namespace Sysman
} // namespace L0
