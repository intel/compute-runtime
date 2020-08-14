/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/test/unit_tests/mocks/mock_memory_manager.h"
#include "level_zero/tools/test/unit_tests/sources/sysman/windows/mock_sysman_fixture.h"

#include "mock_pci.h"

namespace L0 {
namespace ult {

class SysmanDevicePciFixture : public SysmanDeviceFixture {
  protected:
    Mock<PciKmdSysManager> *pKmdSysManager = nullptr;
    KmdSysManager *pOriginalKmdSysManager = nullptr;
    void SetUp() override {
        SysmanDeviceFixture::SetUp();

        pMemoryManagerOld = device->getDriverHandle()->getMemoryManager();

        pMemoryManager = new ::testing::NiceMock<MockMemoryManagerSysman>(*neoDevice->getExecutionEnvironment());

        pMemoryManager->localMemorySupported[0] = false;

        device->getDriverHandle()->setMemoryManager(pMemoryManager);

        pKmdSysManager = new Mock<PciKmdSysManager>;

        EXPECT_CALL(*pKmdSysManager, escape(_, _, _, _, _))
            .WillRepeatedly(::testing::Invoke(pKmdSysManager, &Mock<PciKmdSysManager>::mock_escape));

        pOriginalKmdSysManager = pWddmSysmanImp->pKmdSysManager;
        pWddmSysmanImp->pKmdSysManager = pKmdSysManager;

        delete pSysmanDeviceImp->pPci;

        pSysmanDeviceImp->pPci = new PciImp(pOsSysman);

        if (pSysmanDeviceImp->pPci) {
            pSysmanDeviceImp->pPci->init();
        }
    }

    void TearDown() override {
        device->getDriverHandle()->setMemoryManager(pMemoryManagerOld);
        SysmanDeviceFixture::TearDown();
        pWddmSysmanImp->pKmdSysManager = pOriginalKmdSysManager;
        if (pKmdSysManager != nullptr) {
            delete pKmdSysManager;
            pKmdSysManager = nullptr;
        }
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

TEST_F(SysmanDevicePciFixture, GivenValidSysmanHandleWhenCallingzetSysmanPciGetPropertiesWithNoLocalMemoryThenVerifyzetSysmanPciGetPropertiesCallSucceeds) {
    setLocalMemorySupportedAndReinit(false);

    zes_pci_properties_t properties;

    ze_result_t result = zesDevicePciGetProperties(device, &properties);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(properties.address.domain, pKmdSysManager->mockDomain[KmdSysman::PciDomainsType::PciCurrentDevice]);
    EXPECT_EQ(properties.address.bus, pKmdSysManager->mockBus[KmdSysman::PciDomainsType::PciCurrentDevice]);
    EXPECT_EQ(properties.address.device, pKmdSysManager->mockDevice[KmdSysman::PciDomainsType::PciCurrentDevice]);
    EXPECT_EQ(properties.address.function, pKmdSysManager->mockFunction[KmdSysman::PciDomainsType::PciCurrentDevice]);
    EXPECT_EQ(properties.maxSpeed.gen, pKmdSysManager->mockMaxLinkSpeed[KmdSysman::PciDomainsType::PciCurrentDevice]);
    EXPECT_EQ(properties.maxSpeed.width, pKmdSysManager->mockMaxLinkWidth[KmdSysman::PciDomainsType::PciCurrentDevice]);
}

TEST_F(SysmanDevicePciFixture, GivenValidSysmanHandleWhenCallingzetSysmanPciGetBarsThenVerifyzetSysmanPciGetBarsCallSucceeds) {
    uint32_t count = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDevicePciGetBars(device, &count, nullptr));
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

TEST_F(SysmanDevicePciFixture, TestLinkSpeedToGenAndBack) {
    for (int32_t i = PciGenerations::PciGen1; i <= PciGenerations::PciGen5; i++) {
        double speed = convertPciGenToLinkSpeed(i);
        int32_t gen = convertLinkSpeedToPciGen(speed);
        EXPECT_EQ(i, gen);
    }

    EXPECT_EQ(-1, convertLinkSpeedToPciGen(0.0));
    EXPECT_EQ(0.0, convertPciGenToLinkSpeed(0));
}

} // namespace ult
} // namespace L0
