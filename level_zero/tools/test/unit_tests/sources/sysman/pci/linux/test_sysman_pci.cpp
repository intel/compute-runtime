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
