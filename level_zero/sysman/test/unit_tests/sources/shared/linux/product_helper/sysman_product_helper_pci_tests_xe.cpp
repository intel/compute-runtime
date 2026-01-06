/*
 * Copyright (C) 2024-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/sysman/test/unit_tests/sources/linux/mock_sysman_fixture.h"
#include "level_zero/sysman/test/unit_tests/sources/pci/linux/mock_sysfs_pci.h"
#include "level_zero/sysman/test/unit_tests/sources/shared/linux/kmd_interface/mock_sysman_kmd_interface_xe.h"

namespace L0 {
namespace Sysman {
namespace ult {

class ZesPcieDowngradeFixture : public SysmanDeviceFixture {
  protected:
    MockPciSysfsAccess *pSysfsAccess = nullptr;
    MockSysmanKmdInterfaceXe *pSysmanKmdInterface = nullptr;
    L0::Sysman::SysmanDevice *device = nullptr;
    L0::Sysman::SysFsAccessInterface *pOriginalSysfsAccess = nullptr;
    L0::Sysman::FsAccessInterface *pOriginalFsAccess = nullptr;
    L0::Sysman::PciImp *pPciImp;
    L0::Sysman::OsPci *pOsPciPrev;
    std::unique_ptr<L0::ult::Mock<L0::DriverHandleImp>> driverHandle;
    std::unique_ptr<MockPcieDowngradeFwInterface> pMockFwInterface;
    L0::Sysman::FirmwareUtil *pFwUtilInterfaceOld = nullptr;

    void SetUp() override {
        SysmanDeviceFixture::SetUp();
        device = pSysmanDevice;
        pOriginalSysfsAccess = pLinuxSysmanImp->pSysfsAccess;
        pSysmanKmdInterface = new MockSysmanKmdInterfaceXe(pLinuxSysmanImp->getSysmanProductHelper());
        pSysmanKmdInterface->pSysfsAccess = std::make_unique<MockPciSysfsAccess>();
        pLinuxSysmanImp->pSysfsAccess = pSysmanKmdInterface->pSysfsAccess.get();
        pLinuxSysmanImp->pSysmanKmdInterface.reset(pSysmanKmdInterface);
        pSysfsAccess = static_cast<MockPciSysfsAccess *>(pSysmanKmdInterface->pSysfsAccess.get());

        pFwUtilInterfaceOld = pLinuxSysmanImp->pFwUtilInterface;
        pMockFwInterface = std::make_unique<MockPcieDowngradeFwInterface>();
        pLinuxSysmanImp->pFwUtilInterface = pMockFwInterface.get();

        pSysmanDeviceImp->getRootDeviceEnvironment().getMutableHardwareInfo()->capabilityTable.isIntegratedDevice = false;
        pPciImp = static_cast<L0::Sysman::PciImp *>(pSysmanDeviceImp->pPci);
        pOsPciPrev = pPciImp->pOsPci;
        pPciImp->pOsPci = nullptr;
        PublicLinuxPciImp *pLinuxPciImp = new PublicLinuxPciImp(pOsSysman);

        pPciImp->pOsPci = static_cast<L0::Sysman::OsPci *>(pLinuxPciImp);
        pPciImp->pciGetStaticFields();
    }

    void TearDown() override {
        if (nullptr != pPciImp->pOsPci) {
            delete pPciImp->pOsPci;
        }
        pPciImp->pOsPci = pOsPciPrev;
        pPciImp = nullptr;
        pLinuxSysmanImp->pSysfsAccess = pOriginalSysfsAccess;
        pLinuxSysmanImp->pFwUtilInterface = pFwUtilInterfaceOld;
        SysmanDeviceFixture::TearDown();
    }
};

HWTEST2_F(ZesPcieDowngradeFixture, GivenValidSysmanHandleAndFwInterfaceIsAbsentWhenCallingZesIntelDevicePciLinkSpeedUpdateExpThenVerifyApiCallReturnFailure, IsBMG) {
    L0::Sysman::PciImp *tempPciImp = new L0::Sysman::PciImp(pOsSysman);
    pLinuxSysmanImp->pFwUtilInterface = nullptr;
    tempPciImp->init();

    ze_bool_t downgradeUpgrade = true;
    zes_device_action_t pendingAction = {};
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesIntelDevicePciLinkSpeedUpdateExp(device, downgradeUpgrade, &pendingAction));

    delete tempPciImp;
}

HWTEST2_F(ZesPcieDowngradeFixture, GivenValidSysmanHandleWhenCallingZesIntelDevicePciLinkSpeedUpdateExpThenVerifyApiCallSucceed, IsBMG) {
    ze_bool_t downgradeUpgrade = true;
    zes_device_action_t pendingAction = {};
    pSysfsAccess->mockpcieDowngradeStatus = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelDevicePciLinkSpeedUpdateExp(device, downgradeUpgrade, &pendingAction));
    EXPECT_EQ(ZES_DEVICE_ACTION_COLD_SYSTEM_REBOOT, pendingAction);
}

HWTEST2_F(ZesPcieDowngradeFixture, GivenValidSysmanHandleWhenCallingZesIntelDevicePciLinkSpeedUpdateExpAndAttemptingUpgradeThenVerifyApiCallSucceed, IsBMG) {
    ze_bool_t downgradeUpgrade = false;
    zes_device_action_t pendingAction = {};
    pSysfsAccess->mockpcieDowngradeStatus = 1;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelDevicePciLinkSpeedUpdateExp(device, downgradeUpgrade, &pendingAction));
    EXPECT_EQ(ZES_DEVICE_ACTION_COLD_SYSTEM_REBOOT, pendingAction);
}

HWTEST2_F(ZesPcieDowngradeFixture, GivenValidSysmanHandleWhenCallingZesIntelDevicePciLinkSpeedUpdateExpAndFwUtilsGetGfspConfigCallFailsThenVerifyApiCallFails, IsBMG) {
    ze_bool_t downgradeUpgrade = true;
    zes_device_action_t pendingAction = {};
    pMockFwInterface->mockFwGetGfspConfigResult = ZE_RESULT_ERROR_NOT_AVAILABLE;
    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, zesIntelDevicePciLinkSpeedUpdateExp(device, downgradeUpgrade, &pendingAction));
}

HWTEST2_F(ZesPcieDowngradeFixture, GivenValidSysmanHandleWhenCallingZesIntelDevicePciLinkSpeedUpdateExpAndFwUtilsSetGfspConfigCallFailsThenVerifyApiCallFails, IsBMG) {
    ze_bool_t downgradeUpgrade = true;
    zes_device_action_t pendingAction = {};
    pMockFwInterface->mockFwSetGfspConfigResult = ZE_RESULT_ERROR_NOT_AVAILABLE;
    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, zesIntelDevicePciLinkSpeedUpdateExp(device, downgradeUpgrade, &pendingAction));
}

HWTEST2_F(ZesPcieDowngradeFixture, GivenValidSysmanHandleWhenCallingZesIntelDevicePciLinkSpeedUpdateExpAndSysfsReadFailsThenVerifyApiCallFails, IsBMG) {
    ze_bool_t downgradeUpgrade = true;
    zes_device_action_t pendingAction = {};
    pSysfsAccess->mockReadFailure = true;
    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, zesIntelDevicePciLinkSpeedUpdateExp(device, downgradeUpgrade, &pendingAction));
}

HWTEST2_F(ZesPcieDowngradeFixture, GivenValidSysmanHandleWhenCallingZesIntelDevicePciLinkSpeedUpdateExpAndCurrentAndPendingStatesAreSameThenCorrectPendingActionIsReturned, IsBMG) {
    ze_bool_t downgradeUpgrade = true;
    zes_device_action_t pendingAction = {};
    pSysfsAccess->mockpcieDowngradeStatus = 1;
    pMockFwInterface->mockBuf = {2, 0, 0, 0};
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelDevicePciLinkSpeedUpdateExp(device, downgradeUpgrade, &pendingAction));
    EXPECT_EQ(ZES_DEVICE_ACTION_NONE, pendingAction);
}

} // namespace ult
} // namespace Sysman
} // namespace L0