/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_memory_manager.h"
#include "level_zero/tools/source/sysman/pci/linux/os_pci_imp.h"
#include "level_zero/tools/source/sysman/sysman_imp.h"

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

struct MockMemoryManagerPci : public MemoryManagerMock {
    MockMemoryManagerPci(NEO::ExecutionEnvironment &executionEnvironment) : MemoryManagerMock(const_cast<NEO::ExecutionEnvironment &>(executionEnvironment)) {}
};

class SysmanPciFixture : public ::testing::Test {

  protected:
    std::unique_ptr<SysmanImp> sysmanImp;
    zet_sysman_handle_t hSysman;

    OsPci *pOsPci = nullptr;
    Mock<PciSysfsAccess> *pSysfsAccess = nullptr;
    Mock<PcifsAccess> *pfsAccess = nullptr;
    L0::Pci *pPciPrev = nullptr;
    L0::PciImp pciImp;
    PublicLinuxPciImp linuxPciImp;

    MockMemoryManagerPci *memoryManager = nullptr;
    std::unique_ptr<Mock<L0::DriverHandleImp>> driverHandle;
    NEO::MockDevice *neoDevice = nullptr;
    L0::Device *device = nullptr;

    void SetUp() override {
        neoDevice = NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(NEO::defaultHwInfo.get());
        NEO::DeviceVector devices;
        devices.push_back(std::unique_ptr<NEO::Device>(neoDevice));
        memoryManager = new ::testing::NiceMock<MockMemoryManagerPci>(*devices[0].get()->getExecutionEnvironment());
        devices[0].get()->getExecutionEnvironment()->memoryManager.reset(memoryManager);
        driverHandle = std::make_unique<Mock<L0::DriverHandleImp>>();
        driverHandle->initialize(std::move(devices));
        device = driverHandle->devices[0];

        sysmanImp = std::make_unique<SysmanImp>(device->toHandle());
        pSysfsAccess = new NiceMock<Mock<PciSysfsAccess>>;
        linuxPciImp.pSysfsAccess = pSysfsAccess;
        pfsAccess = new NiceMock<Mock<PcifsAccess>>;
        linuxPciImp.pfsAccess = pfsAccess;
        pOsPci = static_cast<OsPci *>(&linuxPciImp);
        pSysfsAccess->setValInt(maxLinkWidthFile, mockMaxLinkWidth);
        pfsAccess->setValInt(maxLinkWidthFile, mockMaxLinkWidth);

        ON_CALL(*pSysfsAccess, read(_, Matcher<std::vector<std::string> &>(_)))
            .WillByDefault(::testing::Invoke(pSysfsAccess, &Mock<PciSysfsAccess>::getValVector));
        ON_CALL(*pSysfsAccess, read(_, Matcher<uint32_t &>(_)))
            .WillByDefault(::testing::Invoke(pSysfsAccess, &Mock<PciSysfsAccess>::getValInt));
        ON_CALL(*pSysfsAccess, readSymLink(_, _))
            .WillByDefault(::testing::Invoke(pSysfsAccess, &Mock<PciSysfsAccess>::getValStringSymLink));
        ON_CALL(*pSysfsAccess, getRealPath(_, _))
            .WillByDefault(::testing::Invoke(pSysfsAccess, &Mock<PciSysfsAccess>::getValStringRealPath));
        ON_CALL(*pSysfsAccess, read(_, Matcher<double &>(_)))
            .WillByDefault(::testing::Invoke(pSysfsAccess, &Mock<PciSysfsAccess>::getValDouble));
        ON_CALL(*pfsAccess, read(_, Matcher<double &>(_)))
            .WillByDefault(::testing::Invoke(pfsAccess, &Mock<PcifsAccess>::getValDouble));
        ON_CALL(*pfsAccess, read(_, Matcher<uint32_t &>(_)))
            .WillByDefault(::testing::Invoke(pfsAccess, &Mock<PcifsAccess>::getValInt));

        pPciPrev = sysmanImp->pPci;
        sysmanImp->pPci = static_cast<Pci *>(&pciImp);
        pciImp.pOsPci = pOsPci;
        pciImp.hCoreDevice = device;
        memoryManager->localMemorySupported[0] = 0;
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
        if (pfsAccess != nullptr) {
            delete pfsAccess;
            pfsAccess = nullptr;
        }
    }
};

TEST_F(SysmanPciFixture, GivenValidSysmanHandleWhenCallingzetSysmanPciGetPropertiesThenVerifyzetSysmanPciGetPropertiesCallSucceeds) {
    zet_pci_properties_t properties, propertiesBefore;

    memset(&properties.address.bus, std::numeric_limits<int>::max(), sizeof(properties.address.bus));
    memset(&properties.address.device, std::numeric_limits<int>::max(), sizeof(properties.address.device));
    memset(&properties.address.function, std::numeric_limits<int>::max(), sizeof(properties.address.function));
    memset(&properties.maxSpeed.gen, std::numeric_limits<int>::max(), sizeof(properties.maxSpeed.gen));
    memset(&properties.maxSpeed.width, std::numeric_limits<int>::max(), sizeof(properties.maxSpeed.width));
    memset(&properties.maxSpeed.maxBandwidth, std::numeric_limits<int>::max(), sizeof(properties.maxSpeed.maxBandwidth));
    propertiesBefore = properties;

    ze_result_t result = zetSysmanPciGetProperties(hSysman, &properties);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(properties.address.bus, expectedBus);
    EXPECT_EQ(properties.address.device, expectedDevice);
    EXPECT_EQ(properties.address.function, expectedFunction);
    EXPECT_EQ(properties.maxSpeed.gen, expectedGen);
    EXPECT_EQ(properties.maxSpeed.width, expectedWidth);
    EXPECT_EQ(properties.maxSpeed.maxBandwidth, expectedBandwidth);

    EXPECT_NE(properties.address.bus, propertiesBefore.address.bus);
    EXPECT_NE(properties.address.device, propertiesBefore.address.device);
    EXPECT_NE(properties.address.function, propertiesBefore.address.function);
    EXPECT_NE(properties.maxSpeed.gen, propertiesBefore.maxSpeed.gen);
    EXPECT_NE(properties.maxSpeed.width, propertiesBefore.maxSpeed.width);
    EXPECT_NE(properties.maxSpeed.maxBandwidth, propertiesBefore.maxSpeed.maxBandwidth);
}

TEST_F(SysmanPciFixture, GivenValidSysmanHandleWhenSettingLmemSupportAndCallingzetSysmanPciGetPropertiesThenVerifyzetSysmanPciGetPropertiesCallSucceeds) {
    zet_pci_properties_t properties, propertiesBefore;
    memoryManager->localMemorySupported[0] = 1;
    pciImp.init();

    memset(&properties.address.bus, std::numeric_limits<int>::max(), sizeof(properties.address.bus));
    memset(&properties.address.device, std::numeric_limits<int>::max(), sizeof(properties.address.device));
    memset(&properties.address.function, std::numeric_limits<int>::max(), sizeof(properties.address.function));
    memset(&properties.maxSpeed.gen, std::numeric_limits<int>::max(), sizeof(properties.maxSpeed.gen));
    memset(&properties.maxSpeed.width, std::numeric_limits<int>::max(), sizeof(properties.maxSpeed.width));
    memset(&properties.maxSpeed.maxBandwidth, std::numeric_limits<int>::max(), sizeof(properties.maxSpeed.maxBandwidth));
    propertiesBefore = properties;

    ze_result_t result = zetSysmanPciGetProperties(hSysman, &properties);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(properties.address.bus, expectedBus);
    EXPECT_EQ(properties.address.device, expectedDevice);
    EXPECT_EQ(properties.address.function, expectedFunction);
    EXPECT_EQ(properties.maxSpeed.gen, expectedGen);
    EXPECT_EQ(properties.maxSpeed.width, expectedWidth);
    EXPECT_EQ(properties.maxSpeed.maxBandwidth, expectedBandwidth);

    EXPECT_NE(properties.address.bus, propertiesBefore.address.bus);
    EXPECT_NE(properties.address.device, propertiesBefore.address.device);
    EXPECT_NE(properties.address.function, propertiesBefore.address.function);
    EXPECT_NE(properties.maxSpeed.gen, propertiesBefore.maxSpeed.gen);
    EXPECT_NE(properties.maxSpeed.width, propertiesBefore.maxSpeed.width);
    EXPECT_NE(properties.maxSpeed.maxBandwidth, propertiesBefore.maxSpeed.maxBandwidth);
}

TEST_F(SysmanPciFixture, GivenValidSysmanHandleWhenGettingPCIWidthThenZeroWidthIsReturnedIfSystemProvidesInvalidValue) {
    uint32_t width = 0;
    pSysfsAccess->setValInt(maxLinkWidthFile, mockMaxLinkWidthInvalid);
    pfsAccess->setValInt(maxLinkWidthFile, mockMaxLinkWidthInvalid);
    ON_CALL(*pSysfsAccess, read(_, Matcher<uint32_t &>(_)))
        .WillByDefault(::testing::Invoke(pSysfsAccess, &Mock<PciSysfsAccess>::getValInt));
    ON_CALL(*pfsAccess, read(_, Matcher<uint32_t &>(_)))
        .WillByDefault(::testing::Invoke(pfsAccess, &Mock<PcifsAccess>::getValInt));

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
