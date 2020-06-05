/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/tools/source/sysman/pci/linux/os_pci_imp.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "mock_sysfs_pci.h"

#include <string>

using ::testing::_;
using ::testing::DoAll;
using ::testing::InSequence;
using ::testing::Invoke;
using ::testing::Matcher;
using ::testing::NiceMock;
using ::testing::Return;

namespace L0 {
namespace ult {
constexpr int mockMaxLinkWidth = 1;
constexpr int mockMaxLinkWidthInvalid = 255;
constexpr uint32_t expectedBus = 0u;
constexpr uint32_t expectedDevice = 2u;
constexpr uint32_t expectedFunction = 0u;
constexpr uint32_t expectedWidth = 1u;
constexpr uint32_t expectedGen = 1u; // As mockMaxLinkSpeed = 2.5, hence expectedGen should be 1
// As mockMaxLinkSpeed = 2.5, hence, pcieSpeedWithEnc = mockMaxLinkWidth * (2.5 * 1000 * 8/10 * 125000) = 250000000
constexpr uint64_t expectedBandwidth = 250000000u;
class SysmanPciFixture : public DeviceFixture, public ::testing::Test {

  protected:
    std::unique_ptr<SysmanImp> sysmanImp;
    zet_sysman_handle_t hSysman;

    OsPci *pOsPci = nullptr;
    Mock<PciSysfsAccess> *pSysfsAccess = nullptr;
    L0::Pci *pPciPrev = nullptr;
    L0::PciImp pciImp;
    PublicLinuxPciImp linuxPciImp;

    void SetUp() override {
        DeviceFixture::SetUp();
        sysmanImp = std::make_unique<SysmanImp>(device->toHandle());
        pSysfsAccess = new NiceMock<Mock<PciSysfsAccess>>;
        linuxPciImp.pSysfsAccess = pSysfsAccess;
        pOsPci = static_cast<OsPci *>(&linuxPciImp);
        pSysfsAccess->setValInt(maxLinkWidthFile, mockMaxLinkWidth);

        ON_CALL(*pSysfsAccess, read(_, Matcher<std::vector<std::string> &>(_)))
            .WillByDefault(::testing::Invoke(pSysfsAccess, &Mock<PciSysfsAccess>::getValVector));
        ON_CALL(*pSysfsAccess, read(_, Matcher<int &>(_)))
            .WillByDefault(::testing::Invoke(pSysfsAccess, &Mock<PciSysfsAccess>::getValInt));
        ON_CALL(*pSysfsAccess, readSymLink(_, _))
            .WillByDefault(::testing::Invoke(pSysfsAccess, &Mock<PciSysfsAccess>::getValString));
        ON_CALL(*pSysfsAccess, read(_, Matcher<double &>(_)))
            .WillByDefault(::testing::Invoke(pSysfsAccess, &Mock<PciSysfsAccess>::getValDouble));

        pPciPrev = sysmanImp->pPci;
        sysmanImp->pPci = static_cast<Pci *>(&pciImp);
        pciImp.pOsPci = pOsPci;
        pciImp.init();
        hSysman = sysmanImp->toHandle();
    }

    void TearDown() override {
        sysmanImp->pPci = pPciPrev;
        pciImp.pOsPci = nullptr;
        // cleanup
        if (pSysfsAccess != nullptr) {
            delete pSysfsAccess;
            pSysfsAccess = nullptr;
        }

        DeviceFixture::TearDown();
    }
};

TEST_F(SysmanPciFixture, GivenValidSysmanHandleWhenCallingzetSysmanPciGetPropertiesThenVerifyzetSysmanPciGetPropertiesCallSucceeds) {
    zet_pci_properties_t properties;
    ze_result_t result = zetSysmanPciGetProperties(hSysman, &properties);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(properties.address.bus, expectedBus);
    EXPECT_EQ(properties.address.device, expectedDevice);
    EXPECT_EQ(properties.address.function, expectedFunction);
    EXPECT_EQ(properties.maxSpeed.gen, expectedGen);
    EXPECT_EQ(properties.maxSpeed.width, expectedWidth);
    EXPECT_EQ(properties.maxSpeed.maxBandwidth, expectedBandwidth);
}

TEST_F(SysmanPciFixture, GivenValidSysmanHandleWhenGettingPCIWidthThenZeroWidthIsReturnedIfSystemProvidesInvalidValue) {
    uint32_t width = 0;
    pSysfsAccess->setValInt(maxLinkWidthFile, mockMaxLinkWidthInvalid);
    ON_CALL(*pSysfsAccess, read(_, Matcher<int &>(_)))
        .WillByDefault(::testing::Invoke(pSysfsAccess, &Mock<PciSysfsAccess>::getValInt));

    EXPECT_EQ(ZE_RESULT_SUCCESS, pciImp.pOsPci->getMaxLinkWidth(width));
    EXPECT_EQ(width, 0u);
}

TEST_F(SysmanPciFixture, GivenValidSysmanHandleWhenCallingzetSysmanPciGetBarsThenVerifyzetSysmanPciGetBarsCallSucceeds) {
    uint32_t count = 0;
    ze_result_t result = zetSysmanPciGetBars(hSysman, &count, nullptr);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_GT(count, 0u);

    std::vector<zet_pci_bar_properties_t> pciBarProps(count);
    result = zetSysmanPciGetBars(hSysman, &count, pciBarProps.data());
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    for (uint32_t i = 0; i < count; i++) {
        EXPECT_LE(pciBarProps[i].type, ZET_PCI_BAR_TYPE_OTHER);
        EXPECT_NE(pciBarProps[i].base, 0u);
        EXPECT_NE(pciBarProps[i].size, 0u);
    }
}

} // namespace ult
} // namespace L0
