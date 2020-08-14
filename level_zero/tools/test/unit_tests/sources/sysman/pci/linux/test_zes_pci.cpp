/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/test/unit_tests/mocks/mock_memory_manager.h"
#include "level_zero/tools/test/unit_tests/sources/sysman/linux/mock_sysman_fixture.h"

#include "mock_sysfs_pci.h"

#include <string>

using ::testing::_;
using ::testing::Invoke;
using ::testing::Matcher;
using ::testing::NiceMock;

namespace L0 {
namespace ult {
constexpr int mockMaxLinkWidth = 1;
constexpr int mockMaxLinkWidthInvalid = 255;
constexpr uint32_t expectedBus = 0u;
constexpr uint32_t expectedDevice = 2u;
constexpr uint32_t expectedFunction = 0u;
constexpr int32_t expectedWidth = 1u;
constexpr int32_t expectedGen = 1u; // As mockMaxLinkSpeed = 2.5, hence expectedGen should be 1
// As mockMaxLinkSpeed = 2.5, hence, pcieSpeedWithEnc = mockMaxLinkWidth * (2.5 * 1000 * 8/10 * 125000) = 250000000
constexpr int64_t expectedBandwidth = 250000000u;

struct MockMemoryManagerPci : public MemoryManagerMock {
    MockMemoryManagerPci(NEO::ExecutionEnvironment &executionEnvironment) : MemoryManagerMock(const_cast<NEO::ExecutionEnvironment &>(executionEnvironment)) {}
};

class ZesPciFixture : public ::testing::Test {

  protected:
    std::unique_ptr<Mock<PciSysfsAccess>> pSysfsAccess;
    std::unique_ptr<Mock<PcifsAccess>> pfsAccess;
    MockMemoryManagerPci *memoryManager = nullptr;
    SysfsAccess *pOriginalSysfsAccess = nullptr;
    FsAccess *pOriginalFsAccess = nullptr;
    L0::PciImp *pPciImp;
    OsPci *pOsPciPrev;
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

        neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[device->getRootDeviceIndex()]->osInterface = std::make_unique<NEO::OSInterface>();
        auto osInterface = device->getOsInterface().get();
        osInterface->setDrm(new SysmanMockDrm(const_cast<NEO::RootDeviceEnvironment &>(neoDevice->getRootDeviceEnvironment())));
        setenv("ZES_ENABLE_SYSMAN", "1", 1);
        device->setSysmanHandle(L0::SysmanDeviceHandleContext::init(device->toHandle()));
        pSysmanDevice = device->getSysmanHandle();
        pSysmanDeviceImp = static_cast<SysmanDeviceImp *>(pSysmanDevice);
        pOsSysman = pSysmanDeviceImp->pOsSysman;
        pLinuxSysmanImp = static_cast<PublicLinuxSysmanImp *>(pOsSysman);

        pSysfsAccess = std::make_unique<NiceMock<Mock<PciSysfsAccess>>>();
        pOriginalSysfsAccess = pLinuxSysmanImp->pSysfsAccess;
        pLinuxSysmanImp->pSysfsAccess = pSysfsAccess.get();
        pfsAccess = std::make_unique<NiceMock<Mock<PcifsAccess>>>();
        pOriginalFsAccess = pLinuxSysmanImp->pFsAccess;
        pLinuxSysmanImp->pFsAccess = pfsAccess.get();
        pSysfsAccess->setValInt(maxLinkWidthFile, mockMaxLinkWidth);
        pfsAccess->setValInt(maxLinkWidthFile, mockMaxLinkWidth);

        ON_CALL(*pSysfsAccess.get(), read(_, Matcher<std::vector<std::string> &>(_)))
            .WillByDefault(::testing::Invoke(pSysfsAccess.get(), &Mock<PciSysfsAccess>::getValVector));
        ON_CALL(*pSysfsAccess.get(), read(_, Matcher<int32_t &>(_)))
            .WillByDefault(::testing::Invoke(pSysfsAccess.get(), &Mock<PciSysfsAccess>::getValInt));
        ON_CALL(*pSysfsAccess.get(), readSymLink(_, _))
            .WillByDefault(::testing::Invoke(pSysfsAccess.get(), &Mock<PciSysfsAccess>::getValStringSymLink));
        ON_CALL(*pSysfsAccess.get(), getRealPath(_, _))
            .WillByDefault(::testing::Invoke(pSysfsAccess.get(), &Mock<PciSysfsAccess>::getValStringRealPath));
        ON_CALL(*pSysfsAccess.get(), read(_, Matcher<double &>(_)))
            .WillByDefault(::testing::Invoke(pSysfsAccess.get(), &Mock<PciSysfsAccess>::getValDouble));
        ON_CALL(*pfsAccess.get(), read(_, Matcher<double &>(_)))
            .WillByDefault(::testing::Invoke(pfsAccess.get(), &Mock<PcifsAccess>::getValDouble));
        ON_CALL(*pfsAccess.get(), read(_, Matcher<int32_t &>(_)))
            .WillByDefault(::testing::Invoke(pfsAccess.get(), &Mock<PcifsAccess>::getValInt));
        pPciImp = static_cast<L0::PciImp *>(pSysmanDeviceImp->pPci);
        pOsPciPrev = pPciImp->pOsPci;
        pPciImp->pOsPci = nullptr;
        memoryManager->localMemorySupported[0] = 0;
        pPciImp->init();
    }

    void TearDown() override {
        if (nullptr != pPciImp->pOsPci) {
            delete pPciImp->pOsPci;
        }
        pPciImp->pOsPci = pOsPciPrev;
        pPciImp = nullptr;
        unsetenv("ZES_ENABLE_SYSMAN");
        pLinuxSysmanImp->pSysfsAccess = pOriginalSysfsAccess;
        pLinuxSysmanImp->pFsAccess = pOriginalFsAccess;
    }
    SysmanDevice *pSysmanDevice = nullptr;
    SysmanDeviceImp *pSysmanDeviceImp = nullptr;
    OsSysman *pOsSysman = nullptr;
    PublicLinuxSysmanImp *pLinuxSysmanImp = nullptr;
};

TEST_F(ZesPciFixture, GivenValidSysmanHandleWhenCallingzetSysmanPciGetPropertiesThenVerifyzetSysmanPciGetPropertiesCallSucceeds) {
    zes_pci_properties_t properties, propertiesBefore;

    memset(&properties.address.bus, std::numeric_limits<int>::max(), sizeof(properties.address.bus));
    memset(&properties.address.device, std::numeric_limits<int>::max(), sizeof(properties.address.device));
    memset(&properties.address.function, std::numeric_limits<int>::max(), sizeof(properties.address.function));
    memset(&properties.maxSpeed.gen, std::numeric_limits<int>::max(), sizeof(properties.maxSpeed.gen));
    memset(&properties.maxSpeed.width, std::numeric_limits<int>::max(), sizeof(properties.maxSpeed.width));
    memset(&properties.maxSpeed.maxBandwidth, std::numeric_limits<int>::max(), sizeof(properties.maxSpeed.maxBandwidth));
    propertiesBefore = properties;

    ze_result_t result = zesDevicePciGetProperties(device, &properties);

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

TEST_F(ZesPciFixture, GivenValidSysmanHandleWhenSettingLmemSupportAndCallingzetSysmanPciGetPropertiesThenVerifyzetSysmanPciGetPropertiesCallSucceeds) {
    zes_pci_properties_t properties, propertiesBefore;
    memoryManager->localMemorySupported[0] = 1;
    pPciImp->init();

    memset(&properties.address.bus, std::numeric_limits<int>::max(), sizeof(properties.address.bus));
    memset(&properties.address.device, std::numeric_limits<int>::max(), sizeof(properties.address.device));
    memset(&properties.address.function, std::numeric_limits<int>::max(), sizeof(properties.address.function));
    memset(&properties.maxSpeed.gen, std::numeric_limits<int>::max(), sizeof(properties.maxSpeed.gen));
    memset(&properties.maxSpeed.width, std::numeric_limits<int>::max(), sizeof(properties.maxSpeed.width));
    memset(&properties.maxSpeed.maxBandwidth, std::numeric_limits<int>::max(), sizeof(properties.maxSpeed.maxBandwidth));
    propertiesBefore = properties;

    ze_result_t result = zesDevicePciGetProperties(device, &properties);

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

TEST_F(ZesPciFixture, GivenValidSysmanHandleWhenGettingPCIWidthThenZeroWidthIsReturnedIfSystemProvidesInvalidValue) {
    int32_t width = 0;
    pSysfsAccess->setValInt(maxLinkWidthFile, mockMaxLinkWidthInvalid);
    pfsAccess->setValInt(maxLinkWidthFile, mockMaxLinkWidthInvalid);
    ON_CALL(*pSysfsAccess.get(), read(_, Matcher<int32_t &>(_)))
        .WillByDefault(::testing::Invoke(pSysfsAccess.get(), &Mock<PciSysfsAccess>::getValInt));
    ON_CALL(*pfsAccess.get(), read(_, Matcher<int32_t &>(_)))
        .WillByDefault(::testing::Invoke(pfsAccess.get(), &Mock<PcifsAccess>::getValInt));

    EXPECT_EQ(ZE_RESULT_SUCCESS, pPciImp->pOsPci->getMaxLinkWidth(width));
    EXPECT_EQ(width, -1);
}

TEST_F(ZesPciFixture, GivenValidSysmanHandleWhenCallingzetSysmanPciGetBarsThenVerifyzetSysmanPciGetBarsCallSucceeds) {
    uint32_t count = 0;
    ze_result_t result = zesDevicePciGetBars(device, &count, nullptr);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_GT(count, 0u);

    uint32_t testCount = count + 1;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDevicePciGetBars(device, &testCount, nullptr));
    EXPECT_EQ(count, testCount);

    std::vector<zes_pci_bar_properties_t> pciBarProps(count);
    result = zesDevicePciGetBars(device, &count, pciBarProps.data());
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    for (uint32_t i = 0; i < count; i++) {
        EXPECT_LE(pciBarProps[i].type, ZES_PCI_BAR_TYPE_MEM);
        EXPECT_NE(pciBarProps[i].base, 0u);
        EXPECT_NE(pciBarProps[i].size, 0u);
    }
}

TEST_F(ZesPciFixture, GivenValidPathWhileCallingchangeDirNLevelsUpThenReturnedPathIsNLevelUpThenTheCurrentPath) {
    PublicLinuxPciImp *pOsPci = static_cast<PublicLinuxPciImp *>(pPciImp->pOsPci);
    std::string testMockRealPath2LevelsUp = pOsPci->changeDirNLevelsUp(mockRealPath, 2);
    EXPECT_EQ(testMockRealPath2LevelsUp, mockRealPath2LevelsUp);
}

TEST_F(ZesPciFixture, GivenValidSysmanHandleWhenCallingzetSysmanPciGetStateThenVerifyzetSysmanPciGetStateCallReturnNotSupported) {
    zes_pci_state_t state;
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesDevicePciGetState(device, &state));
}

TEST_F(ZesPciFixture, TestLinkSpeedToGenAndBack) {
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
