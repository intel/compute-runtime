/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/test/unit_tests/mocks/mock_memory_manager.h"
#include "level_zero/tools/test/unit_tests/sources/sysman/windows/mock_sysman_fixture.h"

#include "mock_pci.h"

extern bool sysmanUltsEnable;

namespace L0 {
namespace ult {

class SysmanDevicePciFixture : public SysmanDeviceFixture {
  protected:
    std::unique_ptr<Mock<PciKmdSysManager>> pKmdSysManager = nullptr;
    KmdSysManager *pOriginalKmdSysManager = nullptr;
    void SetUp() override {
        if (!sysmanUltsEnable) {
            GTEST_SKIP();
        }
        SysmanDeviceFixture::SetUp();

        pMemoryManagerOld = device->getDriverHandle()->getMemoryManager();

        pMemoryManager = new ::testing::NiceMock<MockMemoryManagerSysman>(*neoDevice->getExecutionEnvironment());

        pMemoryManager->localMemorySupported[0] = false;

        device->getDriverHandle()->setMemoryManager(pMemoryManager);

        pKmdSysManager.reset(new Mock<PciKmdSysManager>);

        pOriginalKmdSysManager = pWddmSysmanImp->pKmdSysManager;
        pWddmSysmanImp->pKmdSysManager = pKmdSysManager.get();

        delete pSysmanDeviceImp->pPci;

        pSysmanDeviceImp->pPci = new PciImp(pOsSysman);

        if (pSysmanDeviceImp->pPci) {
            pSysmanDeviceImp->pPci->init();
        }
    }

    void TearDown() override {
        if (!sysmanUltsEnable) {
            GTEST_SKIP();
        }
        device->getDriverHandle()->setMemoryManager(pMemoryManagerOld);
        pWddmSysmanImp->pKmdSysManager = pOriginalKmdSysManager;
        SysmanDeviceFixture::TearDown();
        if (pMemoryManager != nullptr) {
            delete pMemoryManager;
            pMemoryManager = nullptr;
        }
    }

    void setLocalMemorySupportedAndReinit(bool supported) {
        pMemoryManager->localMemorySupported[0] = supported;

        delete pSysmanDeviceImp->pPci;

        pSysmanDeviceImp->pPci = new PciImp(pOsSysman);

        if (pSysmanDeviceImp->pPci) {
            pSysmanDeviceImp->pPci->init();
        }
    }

    MockMemoryManagerSysman *pMemoryManager = nullptr;
    MemoryManager *pMemoryManagerOld;
};

TEST_F(SysmanDevicePciFixture, GivenValidSysmanHandleWhenCallingzetSysmanPciGetPropertiesWithLocalMemoryThenVerifyzetSysmanPciGetPropertiesCallSucceeds) {
    setLocalMemorySupportedAndReinit(true);

    zes_pci_properties_t properties;

    ze_result_t result = zesDevicePciGetProperties(device, &properties);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(properties.address.domain, pKmdSysManager->mockDomain[KmdSysman::PciDomainsType::PciRootPort]);
    EXPECT_EQ(properties.address.bus, pKmdSysManager->mockBus[KmdSysman::PciDomainsType::PciRootPort]);
    EXPECT_EQ(properties.address.device, pKmdSysManager->mockDevice[KmdSysman::PciDomainsType::PciRootPort]);
    EXPECT_EQ(properties.address.function, pKmdSysManager->mockFunction[KmdSysman::PciDomainsType::PciRootPort]);
    EXPECT_EQ(properties.maxSpeed.gen, pKmdSysManager->mockMaxLinkSpeed[KmdSysman::PciDomainsType::PciRootPort]);
    EXPECT_EQ(properties.maxSpeed.width, pKmdSysManager->mockMaxLinkWidth[KmdSysman::PciDomainsType::PciRootPort]);
}

TEST_F(SysmanDevicePciFixture, GivenValidSysmanHandleWhenCallinggetPciBdfAndkmdSysmanCallFailsThenUnknownValuesArereturned) {
    setLocalMemorySupportedAndReinit(true);
    pKmdSysManager->pciBusReturnCode = KmdSysman::KmdSysmanFail;
    pKmdSysManager->pciDomainReturnCode = KmdSysman::KmdSysmanFail;
    pKmdSysManager->pciDeviceReturnCode = KmdSysman::KmdSysmanFail;
    pKmdSysManager->pciFunctionReturnCode = KmdSysman::KmdSysmanFail;
    zes_pci_properties_t properties = {};
    WddmPciImp *pPciImp = new WddmPciImp(pOsSysman);
    EXPECT_EQ(ZE_RESULT_SUCCESS, pPciImp->getPciBdf(properties));
    EXPECT_EQ(0, properties.address.domain);
    EXPECT_EQ(0, properties.address.bus);
    EXPECT_EQ(0, properties.address.device);
    EXPECT_EQ(0, properties.address.function);
    delete pPciImp;
}

TEST_F(SysmanDevicePciFixture, GivenValidSysmanHandleWhenCallingzetSysmanPciGetProLocalMemoryThenVerifyzetSysmanPciGetPropertiesCallSucceeds) {
    setLocalMemorySupportedAndReinit(true);

    zes_pci_properties_t properties;

    ze_result_t result = zesDevicePciGetProperties(device, &properties);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(properties.address.domain, pKmdSysManager->mockDomain[KmdSysman::PciDomainsType::PciRootPort]);
    EXPECT_EQ(properties.address.bus, pKmdSysManager->mockBus[KmdSysman::PciDomainsType::PciRootPort]);
    EXPECT_EQ(properties.address.device, pKmdSysManager->mockDevice[KmdSysman::PciDomainsType::PciRootPort]);
    EXPECT_EQ(properties.address.function, pKmdSysManager->mockFunction[KmdSysman::PciDomainsType::PciRootPort]);
    EXPECT_EQ(properties.maxSpeed.gen, pKmdSysManager->mockMaxLinkSpeed[KmdSysman::PciDomainsType::PciRootPort]);
    EXPECT_EQ(properties.maxSpeed.width, pKmdSysManager->mockMaxLinkWidth[KmdSysman::PciDomainsType::PciRootPort]);
}

TEST_F(SysmanDevicePciFixture, GivenValidSysmanHandleWhenCallingGetPciBdfAndRequestMultpileFailsThenFailureIsReturned) {
    setLocalMemorySupportedAndReinit(true);
    pKmdSysManager->mockRequestMultiple = true;
    pKmdSysManager->mockRequestMultipleResult = ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    zes_pci_properties_t properties = {};
    WddmPciImp *pPciImp = new WddmPciImp(pOsSysman);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, pPciImp->getPciBdf(properties));
    delete pPciImp;
}

TEST_F(SysmanDevicePciFixture, GivenValidSysmanHandleWhenGettingMaxLinkSpeedAndMaxLinkWidthAndRequestSingleFailsThenUnknownValuesAreReturned) {
    setLocalMemorySupportedAndReinit(true);
    pKmdSysManager->mockRequestSingle = true;
    pKmdSysManager->mockRequestSingleResult = ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    double maxLinkSpeed;
    int32_t maxLinkWidth;
    WddmPciImp *pPciImp = new WddmPciImp(pOsSysman);
    pPciImp->getMaxLinkCaps(maxLinkSpeed, maxLinkWidth);
    EXPECT_DOUBLE_EQ(0.0, maxLinkSpeed);
    EXPECT_EQ(-1, maxLinkWidth);
    delete pPciImp;
}

TEST_F(SysmanDevicePciFixture, GivenValidSysmanHandleWhenCallingzetSysmanPciGetBarsThenVerifyzetSysmanPciGetBarsCallSucceeds) {
    uint32_t count = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDevicePciGetBars(device, &count, nullptr));
}

TEST_F(SysmanDevicePciFixture, GivenValidSysmanHandleWhenCallingzetSysmanPciGetBarsThenVerifyzetSysmanPciGetBarsCallSucceedsWith1_2Extension) {
    uint32_t count = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDevicePciGetBars(device, &count, nullptr));
    EXPECT_NE(count, 0u);

    std::vector<zes_pci_bar_properties_t> pBarProps(count);
    std::vector<zes_pci_bar_properties_1_2_t> props1_2(count);
    for (uint32_t i = 0; i < count; i++) {
        props1_2[i].stype = ZES_STRUCTURE_TYPE_PCI_BAR_PROPERTIES_1_2;
        props1_2[i].pNext = nullptr;
        pBarProps[i].stype = ZES_STRUCTURE_TYPE_PCI_BAR_PROPERTIES;
        pBarProps[i].pNext = static_cast<void *>(&props1_2[i]);
    }

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDevicePciGetBars(device, &count, pBarProps.data()));

    for (uint32_t i = 0; i < count; i++) {
        EXPECT_EQ(pBarProps[i].stype, zes_structure_type_t::ZES_STRUCTURE_TYPE_PCI_BAR_PROPERTIES);
        EXPECT_EQ(props1_2[i].stype, zes_structure_type_t::ZES_STRUCTURE_TYPE_PCI_BAR_PROPERTIES_1_2);
        EXPECT_EQ(props1_2[i].resizableBarSupported, true);
        EXPECT_EQ(props1_2[i].resizableBarEnabled, true);
    }
}

TEST_F(SysmanDevicePciFixture, GivenValidSysmanHandleWhenCallingPciGetBarsThenVerifyAPICallSucceedsWith1_2ExtensionWithNullPtr) {
    uint32_t count = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDevicePciGetBars(device, &count, nullptr));
    EXPECT_NE(count, 0u);

    zes_pci_bar_properties_t *pBarProps = new zes_pci_bar_properties_t[count];

    for (uint32_t i = 0; i < count; i++) {
        pBarProps[i].pNext = nullptr;
        pBarProps[i].stype = zes_structure_type_t::ZES_STRUCTURE_TYPE_PCI_BAR_PROPERTIES;
    }

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDevicePciGetBars(device, &count, pBarProps));

    delete[] pBarProps;
    pBarProps = nullptr;
}

TEST_F(SysmanDevicePciFixture, GivenValidSysmanHandleWhenCallingzetSysmanPciGetBarsThenVerifyzetSysmanPciGetBarsCallSucceedsWith1_2ExtensionWrongType) {
    uint32_t count = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDevicePciGetBars(device, &count, nullptr));
    EXPECT_NE(count, 0u);

    std::vector<zes_pci_bar_properties_t> pBarProps(count);
    std::vector<zes_pci_bar_properties_1_2_t> props1_2(count);
    for (uint32_t i = 0; i < count; i++) {
        props1_2[i].stype = ZES_STRUCTURE_TYPE_PCI_STATE;
        props1_2[i].pNext = nullptr;
        pBarProps[i].stype = ZES_STRUCTURE_TYPE_PCI_BAR_PROPERTIES;
        pBarProps[i].pNext = static_cast<void *>(&props1_2[i]);
    }

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDevicePciGetBars(device, &count, pBarProps.data()));

    for (uint32_t i = 0; i < count; i++) {
        EXPECT_EQ(pBarProps[i].stype, zes_structure_type_t::ZES_STRUCTURE_TYPE_PCI_BAR_PROPERTIES);
        EXPECT_LE(pBarProps[i].type, ZES_PCI_BAR_TYPE_MEM);
        EXPECT_EQ(props1_2[i].stype, zes_structure_type_t::ZES_STRUCTURE_TYPE_PCI_STATE);
    }
}

TEST_F(SysmanDevicePciFixture, GivenValidSysmanHandleWhenCallingzetSysmanPciGetStatsWithLocalMemoryThenVerifyzetSysmanPciGetBarsCallSucceeds) {
    setLocalMemorySupportedAndReinit(true);

    zes_pci_state_t state;
    ze_result_t result = zesDevicePciGetState(device, &state);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(state.speed.gen, pKmdSysManager->mockCurrentLinkSpeed[KmdSysman::PciDomainsType::PciRootPort]);
    EXPECT_EQ(state.speed.width, pKmdSysManager->mockCurrentLinkWidth[KmdSysman::PciDomainsType::PciRootPort]);
}

TEST_F(SysmanDevicePciFixture, GivenValidSysmanHandleWhenCallingzetSysmanPciGetStatsWithNoLocalMemoryThenVerifyzetSysmanPciGetBarsCallSucceeds) {
    setLocalMemorySupportedAndReinit(false);

    zes_pci_state_t state;
    ze_result_t result = zesDevicePciGetState(device, &state);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(state.speed.gen, pKmdSysManager->mockCurrentLinkSpeed[KmdSysman::PciDomainsType::PciCurrentDevice]);
    EXPECT_EQ(state.speed.width, pKmdSysManager->mockCurrentLinkWidth[KmdSysman::PciDomainsType::PciCurrentDevice]);
}

TEST_F(SysmanDevicePciFixture, GivenValidSysmanHandleWhenCallingGetPciStateAndRequestMultipleFailsThenFailureIsReturned) {
    setLocalMemorySupportedAndReinit(true);
    pKmdSysManager->mockRequestMultiple = true;
    pKmdSysManager->mockRequestMultipleResult = ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    zes_pci_state_t pState = {};
    WddmPciImp *pPciImp = new WddmPciImp(pOsSysman);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, pPciImp->getState(&pState));
    delete pPciImp;
}

TEST_F(SysmanDevicePciFixture, GivenValidSysmanHandleWhenCallingGetPciStateAndKmdSysmanCallFailsThenUnknownValuesAreReturned) {
    setLocalMemorySupportedAndReinit(true);
    pKmdSysManager->pciCurrentLinkSpeedReturnCode = KmdSysman::KmdSysmanFail;
    pKmdSysManager->pciCurrentLinkWidthReturnCode = KmdSysman::KmdSysmanFail;
    zes_pci_state_t pState = {};
    pState.speed.gen = -1;
    pState.speed.width = -1;
    WddmPciImp *pPciImp = new WddmPciImp(pOsSysman);
    EXPECT_EQ(ZE_RESULT_SUCCESS, pPciImp->getState(&pState));
    EXPECT_EQ(pState.speed.gen, -1);
    EXPECT_EQ(pState.speed.gen, -1);
    delete pPciImp;
}

TEST_F(SysmanDevicePciFixture, WhenConvertingLinkSpeedThenResultIsCorrect) {
    for (int32_t i = PciGenerations::PciGen1; i <= PciGenerations::PciGen5; i++) {
        double speed = convertPciGenToLinkSpeed(i);
        int32_t gen = convertLinkSpeedToPciGen(speed);
        EXPECT_EQ(i, gen);
    }

    EXPECT_EQ(-1, convertLinkSpeedToPciGen(0.0));
    EXPECT_EQ(0.0, convertPciGenToLinkSpeed(0));
}

TEST_F(SysmanDevicePciFixture, GivenValidSysmanHandleWhenGettingResizableBarSupportAndRequestSingleFailsThenUnknownValuesAreReturned) {
    setLocalMemorySupportedAndReinit(true);
    pKmdSysManager->mockRequestSingle = true;
    pKmdSysManager->mockRequestSingleResult = ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    WddmPciImp *pPciImp = new WddmPciImp(pOsSysman);
    EXPECT_EQ(false, pPciImp->resizableBarSupported());
    delete pPciImp;
}

TEST_F(SysmanDevicePciFixture, GivenValidSysmanHandleWhenGettingResizableBarEnabledAndRequestSingleFailsThenUnknownValuesAreReturned) {
    setLocalMemorySupportedAndReinit(true);
    pKmdSysManager->mockRequestSingle = true;
    pKmdSysManager->mockRequestSingleResult = ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    uint32_t barIndex = 1;
    WddmPciImp *pPciImp = new WddmPciImp(pOsSysman);
    EXPECT_EQ(false, pPciImp->resizableBarEnabled(barIndex));
    delete pPciImp;
}

} // namespace ult
} // namespace L0
