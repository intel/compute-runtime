/*
 * Copyright (C) 2024-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/sysman/source/api/pci/sysman_pci_utils.h"
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
    std::unique_ptr<L0::ult::Mock<L0::DriverHandle>> driverHandle;
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

HWTEST2_F(ZesPcieDowngradeFixture, GivenValidSysmanHandleAndFwInterfaceIsAbsentWhenCallingZesDevicePciLinkSpeedUpdateExtThenVerifyApiCallReturnFailure, IsBMG) {
    L0::Sysman::PciImp *tempPciImp = new L0::Sysman::PciImp(pOsSysman);
    pLinuxSysmanImp->pFwUtilInterface = nullptr;
    tempPciImp->init();

    ze_bool_t downgradeUpgrade = true;
    zes_device_action_t pendingAction = {};
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesDevicePciLinkSpeedUpdateExt(device, downgradeUpgrade, &pendingAction));

    delete tempPciImp;
}

HWTEST2_F(ZesPcieDowngradeFixture, GivenValidSysmanHandleWhenCallingZesIntelDevicePciLinkSpeedUpdateExpThenVerifyApiCallSucceed, IsBMG) {
    ze_bool_t downgradeUpgrade = true;
    zes_device_action_t pendingAction = {};
    pSysfsAccess->mockpcieDowngradeStatus = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelDevicePciLinkSpeedUpdateExp(device, downgradeUpgrade, &pendingAction));
    EXPECT_EQ(ZES_DEVICE_ACTION_COLD_SYSTEM_REBOOT, pendingAction);
}

HWTEST2_F(ZesPcieDowngradeFixture, GivenValidSysmanHandleWhenCallingZesDevicePciLinkSpeedUpdateExtThenVerifyApiCallSucceed, IsBMG) {
    ze_bool_t downgradeUpgrade = true;
    zes_device_action_t pendingAction = {};
    pSysfsAccess->mockpcieDowngradeStatus = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDevicePciLinkSpeedUpdateExt(device, downgradeUpgrade, &pendingAction));
    EXPECT_EQ(ZES_DEVICE_ACTION_COLD_SYSTEM_REBOOT, pendingAction);
}

HWTEST2_F(ZesPcieDowngradeFixture, GivenValidSysmanHandleWhenCallingZesIntelDevicePciLinkSpeedUpdateExpAndAttemptingUpgradeThenVerifyApiCallSucceed, IsBMG) {
    ze_bool_t downgradeUpgrade = false;
    zes_device_action_t pendingAction = {};
    pSysfsAccess->mockpcieDowngradeStatus = 1;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelDevicePciLinkSpeedUpdateExp(device, downgradeUpgrade, &pendingAction));
    EXPECT_EQ(ZES_DEVICE_ACTION_COLD_SYSTEM_REBOOT, pendingAction);
}

HWTEST2_F(ZesPcieDowngradeFixture, GivenValidSysmanHandleWhenCallingZesDevicePciLinkSpeedUpdateExtAndAttemptingUpgradeThenVerifyApiCallSucceed, IsBMG) {
    ze_bool_t downgradeUpgrade = false;
    zes_device_action_t pendingAction = {};
    pSysfsAccess->mockpcieDowngradeStatus = 1;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDevicePciLinkSpeedUpdateExt(device, downgradeUpgrade, &pendingAction));
    EXPECT_EQ(ZES_DEVICE_ACTION_COLD_SYSTEM_REBOOT, pendingAction);
}

HWTEST2_F(ZesPcieDowngradeFixture, GivenValidSysmanHandleWhenCallingZesIntelDevicePciLinkSpeedUpdateExpAndFwUtilsGetGfspConfigCallFailsThenVerifyApiCallFails, IsBMG) {
    ze_bool_t downgradeUpgrade = true;
    zes_device_action_t pendingAction = {};
    pMockFwInterface->mockFwGetGfspConfigResult = ZE_RESULT_ERROR_NOT_AVAILABLE;
    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, zesIntelDevicePciLinkSpeedUpdateExp(device, downgradeUpgrade, &pendingAction));
}

HWTEST2_F(ZesPcieDowngradeFixture, GivenValidSysmanHandleWhenCallingZesDevicePciLinkSpeedUpdateExtAndFwUtilsGetGfspConfigCallFailsThenVerifyApiCallFails, IsBMG) {
    ze_bool_t downgradeUpgrade = true;
    zes_device_action_t pendingAction = {};
    pMockFwInterface->mockFwGetGfspConfigResult = ZE_RESULT_ERROR_NOT_AVAILABLE;
    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, zesDevicePciLinkSpeedUpdateExt(device, downgradeUpgrade, &pendingAction));
}

HWTEST2_F(ZesPcieDowngradeFixture, GivenValidSysmanHandleWhenCallingZesIntelDevicePciLinkSpeedUpdateExpAndFwUtilsSetGfspConfigCallFailsThenVerifyApiCallFails, IsBMG) {
    ze_bool_t downgradeUpgrade = true;
    zes_device_action_t pendingAction = {};
    pMockFwInterface->mockFwSetGfspConfigResult = ZE_RESULT_ERROR_NOT_AVAILABLE;
    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, zesIntelDevicePciLinkSpeedUpdateExp(device, downgradeUpgrade, &pendingAction));
}

HWTEST2_F(ZesPcieDowngradeFixture, GivenValidSysmanHandleWhenCallingZesDevicePciLinkSpeedUpdateExtAndFwUtilsSetGfspConfigCallFailsThenVerifyApiCallFails, IsBMG) {
    ze_bool_t downgradeUpgrade = true;
    zes_device_action_t pendingAction = {};
    pMockFwInterface->mockFwSetGfspConfigResult = ZE_RESULT_ERROR_NOT_AVAILABLE;
    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, zesDevicePciLinkSpeedUpdateExt(device, downgradeUpgrade, &pendingAction));
}

HWTEST2_F(ZesPcieDowngradeFixture, GivenValidSysmanHandleWhenCallingZesIntelDevicePciLinkSpeedUpdateExpAndSysfsReadFailsThenVerifyApiCallFails, IsBMG) {
    ze_bool_t downgradeUpgrade = true;
    zes_device_action_t pendingAction = {};
    pSysfsAccess->mockReadFailure = true;
    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, zesIntelDevicePciLinkSpeedUpdateExp(device, downgradeUpgrade, &pendingAction));
}

HWTEST2_F(ZesPcieDowngradeFixture, GivenValidSysmanHandleWhenCallingZesDevicePciLinkSpeedUpdateExtAndSysfsReadFailsThenVerifyApiCallFails, IsBMG) {
    ze_bool_t downgradeUpgrade = true;
    zes_device_action_t pendingAction = {};
    pSysfsAccess->mockReadFailure = true;
    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, zesDevicePciLinkSpeedUpdateExt(device, downgradeUpgrade, &pendingAction));
}

HWTEST2_F(ZesPcieDowngradeFixture, GivenValidSysmanHandleWhenCallingZesIntelDevicePciLinkSpeedUpdateExpAndCurrentAndPendingStatesAreSameThenCorrectPendingActionIsReturned, IsBMG) {
    ze_bool_t downgradeUpgrade = true;
    zes_device_action_t pendingAction = {};
    pSysfsAccess->mockpcieDowngradeStatus = 1;
    pMockFwInterface->mockBuf = {2, 0, 0, 0};
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelDevicePciLinkSpeedUpdateExp(device, downgradeUpgrade, &pendingAction));
    EXPECT_EQ(ZES_DEVICE_ACTION_NONE, pendingAction);
}

HWTEST2_F(ZesPcieDowngradeFixture, GivenValidSysmanHandleWhenCallingZesDevicePciLinkSpeedUpdateExtAndCurrentAndPendingStatesAreSameThenCorrectPendingActionIsReturned, IsBMG) {
    ze_bool_t downgradeUpgrade = true;
    zes_device_action_t pendingAction = {};
    pSysfsAccess->mockpcieDowngradeStatus = 1;
    pMockFwInterface->mockBuf = {2, 0, 0, 0};
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDevicePciLinkSpeedUpdateExt(device, downgradeUpgrade, &pendingAction));
    EXPECT_EQ(ZES_DEVICE_ACTION_NONE, pendingAction);
}

HWTEST2_F(ZesPcieDowngradeFixture, GivenValidSysmanHandleWhenPciConfigSpaceReadSucceedsThenValidValuesAreReturned, IsBMG) {
    PublicLinuxPciImp *pLinuxPciImp = static_cast<PublicLinuxPciImp *>(pPciImp->pOsPci);
    auto openMockReturnSuccess = +[](const char *pathname, int flags) -> int {
        return 5;
    };
    auto preadMockConfigSpace = +[](int fd, void *buf, size_t count, off_t offset) -> ssize_t {
        uint8_t *mockBuf = static_cast<uint8_t *>(buf);
        if (count == PCI_CFG_SPACE_SIZE) {
            memset(mockBuf, 0, PCI_CFG_SPACE_SIZE);
            mockBuf[0x06] = 0x10;
            mockBuf[0x34] = 0x40;
            mockBuf[0x40] = 0x10;
            mockBuf[0x41] = 0x00;
            mockBuf[0x42] = 0x02;
            mockBuf[0x4C] = 0x51;
            mockBuf[0x4D] = 0x00;

            uint16_t linkStatus = 5 | (8 << 4);
            mockBuf[0x52] = linkStatus & 0xFF;
            mockBuf[0x53] = (linkStatus >> 8) & 0xFF;
            return PCI_CFG_SPACE_SIZE;
        }
        return -1;
    };

    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> openBackup(&NEO::SysCalls::sysCallsOpen, openMockReturnSuccess);
    pLinuxPciImp->preadFunction = preadMockConfigSpace;

    zes_pci_state_t pciState = {};
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDevicePciGetState(device, &pciState));
    EXPECT_EQ(pciState.speed.gen, 5);
    EXPECT_EQ(pciState.speed.width, 8);
    EXPECT_EQ(pciState.speed.maxBandwidth, mockPciMaxBandwidth); // 31.5 GB/s for PCIe Gen5 x8 lanes
}

} // namespace ult
} // namespace Sysman
} // namespace L0
