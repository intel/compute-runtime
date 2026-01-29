/*
 * Copyright (C) 2025-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/sysman/source/api/pci/sysman_pci_utils.h"
#include "level_zero/sysman/test/unit_tests/sources/linux/mocks/mock_sysman_product_helper.h"
#include "level_zero/sysman/test/unit_tests/sources/pci/linux/mock_sysfs_pci.h"
#include "level_zero/sysman/test/unit_tests/sources/shared/linux/kmd_interface/mock_sysman_kmd_interface_xe.h"

namespace L0 {
namespace Sysman {
namespace ult {

class ZesPciFixtureXe : public SysmanDeviceFixture {

  protected:
    MockPciSysfsAccess *pSysfsAccess = nullptr;
    MockSysmanKmdInterfaceXe *pSysmanKmdInterface = nullptr;
    L0::Sysman::SysmanDevice *device = nullptr;
    L0::Sysman::SysFsAccessInterface *pOriginalSysfsAccess = nullptr;
    L0::Sysman::FsAccessInterface *pOriginalFsAccess = nullptr;
    L0::Sysman::PciImp *pPciImp;
    L0::Sysman::OsPci *pOsPciPrev;
    std::unique_ptr<L0::ult::Mock<L0::DriverHandle>> driverHandle;

    void SetUp() override {
        SysmanDeviceFixture::SetUp();
        device = pSysmanDevice;
        pOriginalSysfsAccess = pLinuxSysmanImp->pSysfsAccess;
        pSysmanKmdInterface = new MockSysmanKmdInterfaceXe(pLinuxSysmanImp->getSysmanProductHelper());
        pSysmanKmdInterface->pSysfsAccess = std::make_unique<MockPciSysfsAccess>();
        pLinuxSysmanImp->pSysfsAccess = pSysmanKmdInterface->pSysfsAccess.get();
        pLinuxSysmanImp->pSysmanKmdInterface.reset(pSysmanKmdInterface);
        pSysfsAccess = static_cast<MockPciSysfsAccess *>(pSysmanKmdInterface->pSysfsAccess.get());

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
        SysmanDeviceFixture::TearDown();
    }
};

TEST_F(ZesPciFixtureXe, GivenValidSysmanHandleWhenCallingZesDevicePciGetPropertiesWithExtensionStructureThenVerifyApiCallSucceeds) {
    zes_pci_properties_t properties = {};
    zes_intel_pci_link_speed_downgrade_exp_properties_t extProps = {};
    extProps.stype = ZES_INTEL_PCI_LINK_SPEED_DOWNGRADE_EXP_PROPERTIES;
    properties.pNext = &extProps;

    ze_result_t result = zesDevicePciGetProperties(device, &properties);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_TRUE(extProps.pciLinkSpeedUpdateCapable);
    EXPECT_EQ(extProps.maxPciGenSupported, pLinuxSysmanImp->getSysmanProductHelper()->maxPcieGenSupported());

    // Repeat the test for zes_pci_link_speed_downgrade_ext_properties_t
    properties = {};
    zes_pci_link_speed_downgrade_ext_properties_t extProps1 = {};
    extProps1.stype = ZES_STRUCTURE_TYPE_PCI_LINK_SPEED_DOWNGRADE_EXT_PROPERTIES;
    properties.pNext = &extProps1;

    result = zesDevicePciGetProperties(device, &properties);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_TRUE(extProps1.pciLinkSpeedUpdateCapable);
    EXPECT_EQ(extProps1.maxPciGenSupported, pLinuxSysmanImp->getSysmanProductHelper()->maxPcieGenSupported());
}

TEST_F(ZesPciFixtureXe, GivenValidSysmanHandleWhenCallingZesDevicePciGetPropertiesWithWrongExtensionStructureThenCallFails) {
    zes_pci_properties_t properties = {};
    zes_intel_pci_link_speed_downgrade_exp_properties_t extProps = {};
    extProps.stype = ZES_STRUCTURE_TYPE_FORCE_UINT32;
    properties.pNext = &extProps;
    ze_result_t result = zesDevicePciGetProperties(device, &properties);

    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, result);

    // Repeat the test for zes_pci_link_speed_downgrade_ext_properties_t
    properties = {};
    zes_pci_link_speed_downgrade_ext_properties_t extProps1 = {};
    extProps1.stype = ZES_STRUCTURE_TYPE_FORCE_UINT32;
    properties.pNext = &extProps1;
    result = zesDevicePciGetProperties(device, &properties);

    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, result);
}

TEST_F(ZesPciFixtureXe, GivenValidSysmanHandleWhenCallingZesDevicePciGetPropertiesWithExtensionStructureAndReadFailsThenVerifyApiCallSucceeds) {
    zes_pci_properties_t properties = {};
    zes_intel_pci_link_speed_downgrade_exp_properties_t extProps = {};
    extProps.stype = ZES_INTEL_PCI_LINK_SPEED_DOWNGRADE_EXP_PROPERTIES;
    properties.pNext = &extProps;
    pSysfsAccess->mockReadFailure = true;
    ze_result_t result = zesDevicePciGetProperties(device, &properties);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    // Repeat the test for zes_pci_link_speed_downgrade_ext_properties_t
    properties = {};
    zes_pci_link_speed_downgrade_ext_properties_t extProps1 = {};
    extProps1.stype = ZES_STRUCTURE_TYPE_PCI_LINK_SPEED_DOWNGRADE_EXT_PROPERTIES;
    properties.pNext = &extProps1;
    pSysfsAccess->mockReadFailure = true;
    result = zesDevicePciGetProperties(device, &properties);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

TEST_F(ZesPciFixtureXe, GivenValidSysmanHandleWhenCallingGetPropertiesWithWrongExtensionStructureThenCallSucceedsWithOnlyBaseProperties) {
    L0::Sysman::OsPci *pOsPciOriginal = pPciImp->pOsPci;
    PublicLinuxPciImp *pLinuxPciImpTemp = new PublicLinuxPciImp(pOsSysman);
    pPciImp->pOsPci = static_cast<L0::Sysman::OsPci *>(pLinuxPciImpTemp);

    zes_pci_properties_t properties = {};
    zes_intel_pci_link_speed_downgrade_exp_properties_t extProps = {};
    extProps.stype = ZES_STRUCTURE_TYPE_FORCE_UINT32;
    properties.pNext = &extProps;
    ze_result_t result = pPciImp->pOsPci->getProperties(&properties);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    // Repeat the test for zes_pci_link_speed_downgrade_ext_properties_t
    properties = {};
    zes_pci_link_speed_downgrade_ext_properties_t extProps1 = {};
    extProps1.stype = ZES_STRUCTURE_TYPE_FORCE_UINT32;
    properties.pNext = &extProps1;
    result = pPciImp->pOsPci->getProperties(&properties);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    delete pLinuxPciImpTemp;
    pPciImp->pOsPci = pOsPciOriginal;
}

TEST_F(ZesPciFixtureXe, GivenValidSysmanHandleWhenCallingZesSysmanPciGetStateWithExtensionStructureAndReadFailsCallFails) {
    zes_pci_state_t pciState = {};
    zes_intel_pci_link_speed_downgrade_exp_state_t extState = {};
    extState.stype = ZES_INTEL_PCI_LINK_SPEED_DOWNGRADE_EXP_STATE;
    pciState.pNext = &extState;
    pSysfsAccess->mockReadFailure = true;
    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, zesDevicePciGetState(device, &pciState));

    // Repeat the test for zes_pci_link_speed_downgrade_ext_state_t
    pciState = {};
    zes_pci_link_speed_downgrade_ext_state_t extState1 = {};
    extState1.stype = ZES_STRUCTURE_TYPE_PCI_LINK_SPEED_DOWNGRADE_EXT_STATE;
    pciState.pNext = &extState1;
    pSysfsAccess->mockReadFailure = true;
    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, zesDevicePciGetState(device, &pciState));
}

TEST_F(ZesPciFixtureXe, GivenValidSysmanHandleWhenCallingZesSysmanPciGetStateWithExtensionStructureThenCallSucceeds) {
    zes_pci_state_t pciState = {};
    zes_intel_pci_link_speed_downgrade_exp_state_t extState = {};
    extState.stype = ZES_INTEL_PCI_LINK_SPEED_DOWNGRADE_EXP_STATE;
    pciState.pNext = &extState;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDevicePciGetState(device, &pciState));
    EXPECT_TRUE(extState.pciLinkSpeedDowngradeStatus);
    EXPECT_EQ(pciState.status, ZES_PCI_LINK_STATUS_UNKNOWN);
    EXPECT_EQ(pciState.speed.gen, -1);
    EXPECT_EQ(pciState.speed.width, -1);
    EXPECT_EQ(pciState.speed.maxBandwidth, -1);

    // Repeat the test for zes_pci_link_speed_downgrade_ext_state_t
    pciState = {};
    zes_pci_link_speed_downgrade_ext_state_t extState1 = {};
    extState1.stype = ZES_STRUCTURE_TYPE_PCI_LINK_SPEED_DOWNGRADE_EXT_STATE;
    pciState.pNext = &extState1;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDevicePciGetState(device, &pciState));
    EXPECT_TRUE(extState1.pciLinkSpeedDowngradeStatus);
    EXPECT_EQ(pciState.status, ZES_PCI_LINK_STATUS_UNKNOWN);
    EXPECT_EQ(pciState.speed.gen, -1);
    EXPECT_EQ(pciState.speed.width, -1);
    EXPECT_EQ(pciState.speed.maxBandwidth, -1);
}

TEST_F(ZesPciFixtureXe, GivenValidSysmanHandleWhenCallingZesSysmanPciGetStateWithWrongExtensionStructureThenCallSucceedsWithOnlyBaseProperties) {
    L0::Sysman::OsPci *pOsPciOriginal = pPciImp->pOsPci;
    PublicLinuxPciImp *pLinuxPciImpTemp = new PublicLinuxPciImp(pOsSysman);
    pPciImp->pOsPci = static_cast<L0::Sysman::OsPci *>(pLinuxPciImpTemp);

    zes_pci_state_t pciState = {};
    zes_intel_pci_link_speed_downgrade_exp_state_t extState = {};
    extState.stype = ZES_STRUCTURE_TYPE_FORCE_UINT32;
    pciState.pNext = &extState;
    ze_result_t result = pPciImp->pOsPci->getState(&pciState);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, result);

    // Repeat the test for zes_pci_link_speed_downgrade_ext_state_t
    pciState = {};
    zes_pci_link_speed_downgrade_ext_state_t extState1 = {};
    extState1.stype = ZES_STRUCTURE_TYPE_FORCE_UINT32;
    pciState.pNext = &extState1;
    result = pPciImp->pOsPci->getState(&pciState);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, result);

    delete pLinuxPciImpTemp;
    pPciImp->pOsPci = pOsPciOriginal;
}

TEST_F(ZesPciFixtureXe, GivenValidSysmanHandleWhenCallingZesIntelDevicePciLinkSpeedUpdateExpThenVerifyApiCallFails) {
    struct MockSysmanProductHelperPcieDowngrade : L0::Sysman::SysmanProductHelperHw<IGFX_UNKNOWN> {
        MockSysmanProductHelperPcieDowngrade() = default;
    };

    std::unique_ptr<SysmanProductHelper> pSysmanProductHelper = std::make_unique<MockSysmanProductHelperPcieDowngrade>();
    std::swap(pLinuxSysmanImp->pSysmanProductHelper, pSysmanProductHelper);
    ze_bool_t downgradeUpgrade = true;
    zes_device_action_t pendingAction = {};
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesIntelDevicePciLinkSpeedUpdateExp(device, downgradeUpgrade, &pendingAction));
}

TEST_F(ZesPciFixtureXe, GivenValidSysmanHandleWhenCallingZesDevicePciLinkSpeedUpdateExtThenVerifyApiCallFails) {
    struct MockSysmanProductHelperPcieDowngrade : L0::Sysman::SysmanProductHelperHw<IGFX_UNKNOWN> {
        MockSysmanProductHelperPcieDowngrade() = default;
    };

    std::unique_ptr<SysmanProductHelper> pSysmanProductHelper = std::make_unique<MockSysmanProductHelperPcieDowngrade>();
    std::swap(pLinuxSysmanImp->pSysmanProductHelper, pSysmanProductHelper);
    ze_bool_t downgradeUpgrade = true;
    zes_device_action_t pendingAction = {};
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesDevicePciLinkSpeedUpdateExt(device, downgradeUpgrade, &pendingAction));
}

} // namespace ult
} // namespace Sysman
} // namespace L0
