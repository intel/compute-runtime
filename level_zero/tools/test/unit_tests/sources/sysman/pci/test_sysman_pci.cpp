/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "mock_pci.h"

#include <string>

using ::testing::_;
using ::testing::DoAll;
using ::testing::InSequence;
using ::testing::Invoke;
using ::testing::NiceMock;
using ::testing::Return;

namespace L0 {
namespace ult {

ACTION_P(SetString, value) {
    arg0 = value;
}

ACTION_P(SetUint32_t, value) {
    arg0 = value;
}

ACTION_P(SetUint64_t, value) {
    arg0 = value;
}

ACTION_P(SetDouble, value) {
    arg0 = value;
}

ACTION_P(SetBarProp_t, value) {
    arg0 = value;
}

class SysmanPciFixture : public DeviceFixture, public ::testing::Test {

  protected:
    SysmanImp *sysmanImp;
    zet_sysman_handle_t hSysman;

    Mock<OsPci> *pOsPci;
    Pci *pPciPrev;
    PciImp *pPci;

    const std::string bdf = "0000:00:02.0";
    const double maxLinkSpeed = 2.5;
    const uint32_t maxLinkWidth = 1;
    const uint32_t linkGen = 3;
    zet_pci_bar_properties_t mockBar1 = {ZET_PCI_BAR_TYPE_VRAM, 3, 0x0A000000, 0x1000000};
    zet_pci_bar_properties_t mockBar2 = {ZET_PCI_BAR_TYPE_INDIRECT_MEM, 5, 0x58000000, 0x10000};
    zet_pci_bar_properties_t mockBar3 = {ZET_PCI_BAR_TYPE_INDIRECT_MEM, 5, 0x38000000, 0x10000};

    std::vector<zet_pci_bar_properties_t *> MockpBarProperties = {&mockBar1, &mockBar2, &mockBar3};

    void SetUp() override {
        DeviceFixture::SetUp();
        sysmanImp = new SysmanImp(device->toHandle());
        pOsPci = new NiceMock<Mock<OsPci>>;
        ON_CALL(*pOsPci, getPciBdf(_))
            .WillByDefault(DoAll(
                SetString(bdf),
                Return(ZE_RESULT_SUCCESS)));
        ON_CALL(*pOsPci, getMaxLinkSpeed(_))
            .WillByDefault(DoAll(
                SetDouble(maxLinkSpeed),
                Return(ZE_RESULT_SUCCESS)));
        ON_CALL(*pOsPci, getMaxLinkWidth(_))
            .WillByDefault(DoAll(
                SetUint32_t(maxLinkWidth),
                Return(ZE_RESULT_SUCCESS)));
        ON_CALL(*pOsPci, getLinkGen(_))
            .WillByDefault(DoAll(
                SetUint32_t(linkGen),
                Return(ZE_RESULT_SUCCESS)));
        ON_CALL(*pOsPci, initializeBarProperties(_))
            .WillByDefault(DoAll(
                SetBarProp_t(MockpBarProperties),
                Return(ZE_RESULT_SUCCESS)));

        pPci = new PciImp();
        pPciPrev = sysmanImp->pPci;
        sysmanImp->pPci = static_cast<Pci *>(pPci);
        pPci->pOsPci = pOsPci;
        pPci->init();

        hSysman = sysmanImp->toHandle();
    }

    void TearDown() override {
        sysmanImp->pPci = pPciPrev;
        // cleanup
        if (pPci != nullptr) {
            delete pPci;
            pPci = nullptr;
        }
        if (sysmanImp != nullptr) {
            delete sysmanImp;
            sysmanImp = nullptr;
        }
        DeviceFixture::TearDown();
    }
};

TEST_F(SysmanPciFixture, GivenValidSysmanHandleWhenCallingzetSysmanPciGetPropertiesThenVerifyzetSysmanPciGetPropertiesCallSucceeds) {
    zet_pci_properties_t properties;
    ze_result_t result = zetSysmanPciGetProperties(hSysman, &properties);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_LT(properties.address.bus, 256u);
    EXPECT_LT(properties.address.device, 32u);
    EXPECT_LT(properties.address.function, 8u);
    EXPECT_LE(properties.maxSpeed.gen, 5u);
    EXPECT_LE(properties.maxSpeed.width, 32u);
    EXPECT_LE(properties.maxSpeed.maxBandwidth, std::numeric_limits<uint64_t>::max());
}

TEST_F(SysmanPciFixture, GivenValidSysmanHandleWhenCallingzetSysmanPciGetBarsThenVerifyzetSysmanPciGetBarsCallSucceeds) {
    uint32_t count = 0;
    ze_result_t result = zetSysmanPciGetBars(hSysman, &count, nullptr);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_GT(count, static_cast<uint32_t>(0));

    std::vector<zet_pci_bar_properties_t> pciBarProps(count);
    result = zetSysmanPciGetBars(hSysman, &count, pciBarProps.data());
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    for (uint32_t i = 0; i < count; i++) {
        EXPECT_LE(pciBarProps[i].type, ZET_PCI_BAR_TYPE_OTHER);
        EXPECT_NE(pciBarProps[i].base, static_cast<uint32_t>(0));
        EXPECT_NE(pciBarProps[i].size, static_cast<uint32_t>(0));
    }
}

} // namespace ult
} // namespace L0
