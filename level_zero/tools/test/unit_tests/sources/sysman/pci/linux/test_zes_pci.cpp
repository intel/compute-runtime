/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/test/unit_tests/mocks/mock_memory_manager.h"
#include "level_zero/tools/test/unit_tests/sources/sysman/linux/mock_sysman_fixture.h"

#include "mock_sysfs_pci.h"

#include <string>

extern bool sysmanUltsEnable;

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
constexpr int convertMegabitsPerSecondToBytesPerSecond = 125000;
constexpr int convertGigabitToMegabit = 1000;
constexpr double encodingGen1Gen2 = 0.8;
constexpr double encodingGen3andAbove = 0.98461538461;
constexpr int pciExtendedConfigSpaceSize = 4096;
static int fakeFileDescriptor = 123;

inline static int openMock(const char *pathname, int flags) {
    if (strcmp(pathname, mockRealPathConfig.c_str()) == 0) {
        return fakeFileDescriptor;
    }
    return -1;
}

inline static int openMockReturnFailure(const char *pathname, int flags) {
    return -1;
}

inline static int closeMock(int fd) {
    if (fd == fakeFileDescriptor) {
        return 0;
    }
    return -1;
}

ssize_t preadMock(int fd, void *buf, size_t count, off_t offset) {
    uint8_t *mockBuf = static_cast<uint8_t *>(buf);
    // Sample config values
    mockBuf[0x100] = 0x0e;
    mockBuf[0x102] = 0x01;
    mockBuf[0x103] = 0x42;
    mockBuf[0x420] = 0x15;
    mockBuf[0x422] = 0x01;
    mockBuf[0x423] = 0x22;
    mockBuf[0x425] = 0xf0;
    mockBuf[0x426] = 0x3f;
    mockBuf[0x428] = 0x22;
    mockBuf[0x429] = 0x11;
    mockBuf[0x220] = 0x24;
    mockBuf[0x222] = 0x01;
    mockBuf[0x223] = 0x32;
    mockBuf[0x320] = 0x10;
    mockBuf[0x322] = 0x01;
    mockBuf[0x323] = 0x40;
    mockBuf[0x400] = 0x18;
    mockBuf[0x402] = 0x01;
    return pciExtendedConfigSpaceSize;
}

ssize_t preadMockHeaderFailure(int fd, void *buf, size_t count, off_t offset) {
    return pciExtendedConfigSpaceSize;
}

ssize_t preadMockInvalidPos(int fd, void *buf, size_t count, off_t offset) {
    uint8_t *mockBuf = static_cast<uint8_t *>(buf);
    // Sample config values
    mockBuf[0x100] = 0x0e;
    mockBuf[0x102] = 0x01;
    mockBuf[0x420] = 0x15;
    mockBuf[0x422] = 0x01;
    mockBuf[0x423] = 0x22;
    mockBuf[0x220] = 0x24;
    mockBuf[0x222] = 0x01;
    mockBuf[0x223] = 0x32;
    mockBuf[0x320] = 0x10;
    mockBuf[0x322] = 0x01;
    mockBuf[0x323] = 0x40;
    mockBuf[0x400] = 0x18;
    mockBuf[0x402] = 0x01;
    return pciExtendedConfigSpaceSize;
}

ssize_t preadMockLoop(int fd, void *buf, size_t count, off_t offset) {
    uint8_t *mockBuf = static_cast<uint8_t *>(buf);
    // Sample config values
    mockBuf[0x100] = 0x0e;
    mockBuf[0x102] = 0x01;
    mockBuf[0x103] = 0x42;
    mockBuf[0x420] = 0x16;
    mockBuf[0x422] = 0x01;
    mockBuf[0x423] = 0x42;
    mockBuf[0x220] = 0x24;
    mockBuf[0x222] = 0x01;
    mockBuf[0x223] = 0x32;
    mockBuf[0x320] = 0x10;
    mockBuf[0x322] = 0x01;
    mockBuf[0x323] = 0x40;
    mockBuf[0x400] = 0x18;
    mockBuf[0x402] = 0x01;
    return pciExtendedConfigSpaceSize;
}

ssize_t preadMockFailure(int fd, void *buf, size_t count, off_t offset) {
    return -1;
}

struct MockMemoryManagerPci : public MemoryManagerMock {
    MockMemoryManagerPci(NEO::ExecutionEnvironment &executionEnvironment) : MemoryManagerMock(const_cast<NEO::ExecutionEnvironment &>(executionEnvironment)) {}
};

class ZesPciFixture : public SysmanDeviceFixture {

  protected:
    std::unique_ptr<Mock<PciSysfsAccess>> pSysfsAccess;
    std::unique_ptr<Mock<PcifsAccess>> pfsAccess;
    MockMemoryManagerPci *memoryManager = nullptr;
    SysfsAccess *pOriginalSysfsAccess = nullptr;
    FsAccess *pOriginalFsAccess = nullptr;
    L0::PciImp *pPciImp;
    OsPci *pOsPciPrev;
    std::unique_ptr<Mock<L0::DriverHandleImp>> driverHandle;
    MemoryManager *pMemoryManagerOld;

    void SetUp() override {
        if (!sysmanUltsEnable) {
            GTEST_SKIP();
        }
        SysmanDeviceFixture::SetUp();

        pMemoryManagerOld = device->getDriverHandle()->getMemoryManager();
        memoryManager = new ::testing::NiceMock<MockMemoryManagerPci>(*neoDevice->getExecutionEnvironment());
        memoryManager->localMemorySupported[0] = false;
        device->getDriverHandle()->setMemoryManager(memoryManager);

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
        ON_CALL(*pSysfsAccess.get(), isRootUser())
            .WillByDefault(::testing::Invoke(pSysfsAccess.get(), &Mock<PciSysfsAccess>::checkRootUser));
        ON_CALL(*pfsAccess.get(), read(_, Matcher<double &>(_)))
            .WillByDefault(::testing::Invoke(pfsAccess.get(), &Mock<PcifsAccess>::getValDouble));
        ON_CALL(*pfsAccess.get(), read(_, Matcher<int32_t &>(_)))
            .WillByDefault(::testing::Invoke(pfsAccess.get(), &Mock<PcifsAccess>::getValInt));
        pPciImp = static_cast<L0::PciImp *>(pSysmanDeviceImp->pPci);
        pOsPciPrev = pPciImp->pOsPci;
        pPciImp->pOsPci = nullptr;
        memoryManager->localMemorySupported[0] = 0;
        PublicLinuxPciImp *pLinuxPciImp = new PublicLinuxPciImp(pOsSysman);
        pLinuxPciImp->openFunction = openMock;
        pLinuxPciImp->closeFunction = closeMock;
        pLinuxPciImp->preadFunction = preadMock;

        pLinuxPciImp->pciExtendedConfigRead();
        pPciImp->pOsPci = static_cast<OsPci *>(pLinuxPciImp);
        pPciImp->pciGetStaticFields();
    }

    void TearDown() override {
        if (!sysmanUltsEnable) {
            GTEST_SKIP();
        }
        device->getDriverHandle()->setMemoryManager(pMemoryManagerOld);
        SysmanDeviceFixture::TearDown();
        if (nullptr != pPciImp->pOsPci) {
            delete pPciImp->pOsPci;
        }
        pPciImp->pOsPci = pOsPciPrev;
        pPciImp = nullptr;
        unsetenv("ZES_ENABLE_SYSMAN");
        pLinuxSysmanImp->pSysfsAccess = pOriginalSysfsAccess;
        pLinuxSysmanImp->pFsAccess = pOriginalFsAccess;
        if (memoryManager != nullptr) {
            delete memoryManager;
            memoryManager = nullptr;
        }
    }
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

TEST_F(ZesPciFixture, GivenValidSysmanHandleWhenSettingLmemSupportAndCallingzetSysmanPciGetPropertiesThenVerifyApiCallSucceeds) {
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

TEST_F(ZesPciFixture, GivenValidSysmanHandleWhenCallingzetSysmanPciGetPropertiesAndBdfStringIsEmptyThenVerifyApiCallSucceeds) {
    zes_pci_properties_t properties;
    ON_CALL(*pSysfsAccess.get(), readSymLink(_, _))
        .WillByDefault(::testing::Invoke(pSysfsAccess.get(), &Mock<PciSysfsAccess>::getValStringSymLinkEmpty));
    pPciImp->init();

    ze_result_t result = zesDevicePciGetProperties(device, &properties);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(properties.address.bus, 0u);
    EXPECT_EQ(properties.address.device, 0u);
    EXPECT_EQ(properties.address.function, 0u);
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

TEST_F(ZesPciFixture, GivenValidSysmanHandleWhenInitializingPciAndPciConfigOpenFailsThenResizableBarSupportWillBeFalse) {
    OsPci *pOsPciOriginal = pPciImp->pOsPci;
    PublicLinuxPciImp *pLinuxPciImpTemp = new PublicLinuxPciImp(pOsSysman);
    pLinuxPciImpTemp->openFunction = openMockReturnFailure;
    pLinuxPciImpTemp->closeFunction = closeMock;
    pLinuxPciImpTemp->preadFunction = preadMock;

    pLinuxPciImpTemp->pciExtendedConfigRead();
    pPciImp->pOsPci = static_cast<OsPci *>(pLinuxPciImpTemp);
    pPciImp->pciGetStaticFields();
    EXPECT_FALSE(pPciImp->pOsPci->resizableBarSupported());
    uint32_t barIndex = 2u;
    EXPECT_FALSE(pPciImp->pOsPci->resizableBarEnabled(barIndex));
    delete pLinuxPciImpTemp;
    pPciImp->pOsPci = pOsPciOriginal;
}

TEST_F(ZesPciFixture, GivenValidSysmanHandleWhenInitializingPciAndPciConfigReadFailsThenResizableBarSupportWillBeFalse) {
    OsPci *pOsPciOriginal = pPciImp->pOsPci;
    PublicLinuxPciImp *pLinuxPciImpTemp = new PublicLinuxPciImp(pOsSysman);
    pLinuxPciImpTemp->openFunction = openMock;
    pLinuxPciImpTemp->closeFunction = closeMock;
    pLinuxPciImpTemp->preadFunction = preadMockFailure;

    pLinuxPciImpTemp->pciExtendedConfigRead();
    pPciImp->pOsPci = static_cast<OsPci *>(pLinuxPciImpTemp);
    pPciImp->pciGetStaticFields();
    EXPECT_FALSE(pPciImp->pOsPci->resizableBarSupported());
    uint32_t barIndex = 2u;
    EXPECT_FALSE(pPciImp->pOsPci->resizableBarEnabled(barIndex));
    delete pLinuxPciImpTemp;
    pPciImp->pOsPci = pOsPciOriginal;
}

TEST_F(ZesPciFixture, GivenSysmanHandleWhenCheckForResizableBarSupportAndHeaderFieldNotPresentThenResizableBarSupportFalseReturned) {
    OsPci *pOsPciOriginal = pPciImp->pOsPci;
    PublicLinuxPciImp *pLinuxPciImpTemp = new PublicLinuxPciImp(pOsSysman);
    pLinuxPciImpTemp->openFunction = openMock;
    pLinuxPciImpTemp->closeFunction = closeMock;
    pLinuxPciImpTemp->preadFunction = preadMockHeaderFailure;

    pLinuxPciImpTemp->pciExtendedConfigRead();
    pPciImp->pOsPci = static_cast<OsPci *>(pLinuxPciImpTemp);
    pPciImp->pciGetStaticFields();
    EXPECT_FALSE(pPciImp->pOsPci->resizableBarSupported());
    uint32_t barIndex = 2u;
    EXPECT_FALSE(pPciImp->pOsPci->resizableBarEnabled(barIndex));
    delete pLinuxPciImpTemp;
    pPciImp->pOsPci = pOsPciOriginal;
}

TEST_F(ZesPciFixture, GivenSysmanHandleWhenCheckForResizableBarSupportAndCapabilityLinkListIsBrokenThenResizableBarSupportFalseReturned) {
    OsPci *pOsPciOriginal = pPciImp->pOsPci;
    PublicLinuxPciImp *pLinuxPciImpTemp = new PublicLinuxPciImp(pOsSysman);
    pLinuxPciImpTemp->openFunction = openMock;
    pLinuxPciImpTemp->closeFunction = closeMock;
    pLinuxPciImpTemp->preadFunction = preadMockInvalidPos;

    pLinuxPciImpTemp->pciExtendedConfigRead();
    pPciImp->pOsPci = static_cast<OsPci *>(pLinuxPciImpTemp);
    pPciImp->pciGetStaticFields();
    EXPECT_FALSE(pPciImp->pOsPci->resizableBarSupported());
    uint32_t barIndex = 2u;
    EXPECT_FALSE(pPciImp->pOsPci->resizableBarEnabled(barIndex));
    delete pLinuxPciImpTemp;
    pPciImp->pOsPci = pOsPciOriginal;
}

TEST_F(ZesPciFixture, GivenSysmanHandleWhenCheckForResizableBarSupportAndIfRebarCapabilityNotPresentThenResizableBarSupportFalseReturned) {
    OsPci *pOsPciOriginal = pPciImp->pOsPci;
    PublicLinuxPciImp *pLinuxPciImpTemp = new PublicLinuxPciImp(pOsSysman);
    pLinuxPciImpTemp->openFunction = openMock;
    pLinuxPciImpTemp->closeFunction = closeMock;
    pLinuxPciImpTemp->preadFunction = preadMockLoop;

    pLinuxPciImpTemp->pciExtendedConfigRead();
    pPciImp->pOsPci = static_cast<OsPci *>(pLinuxPciImpTemp);
    pPciImp->pciGetStaticFields();
    EXPECT_FALSE(pPciImp->pOsPci->resizableBarSupported());
    uint32_t barIndex = 2u;
    EXPECT_FALSE(pPciImp->pOsPci->resizableBarEnabled(barIndex));
    delete pLinuxPciImpTemp;
    pPciImp->pOsPci = pOsPciOriginal;
}

TEST_F(ZesPciFixture, GivenValidSysmanHandleWhenCallingzetSysmanPciGetBarsThenVerifyzetSysmanPciGetBarsCallSucceedsWith1_2Extension) {
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
        EXPECT_LE(pBarProps[i].type, ZES_PCI_BAR_TYPE_MEM);
        EXPECT_NE(pBarProps[i].base, 0u);
        EXPECT_NE(pBarProps[i].size, 0u);
        EXPECT_EQ(props1_2[i].stype, zes_structure_type_t::ZES_STRUCTURE_TYPE_PCI_BAR_PROPERTIES_1_2);
        EXPECT_EQ(props1_2[i].resizableBarSupported, true);
        if (props1_2[i].index == 2) {
            EXPECT_EQ(props1_2[i].resizableBarEnabled, true);
        } else {
            EXPECT_EQ(props1_2[i].resizableBarEnabled, false);
        }
        EXPECT_LE(props1_2[i].type, ZES_PCI_BAR_TYPE_MEM);
        EXPECT_NE(props1_2[i].base, 0u);
        EXPECT_NE(props1_2[i].size, 0u);
    }
}

TEST_F(ZesPciFixture, GivenValidSysmanHandleWhenCallingzetSysmanPciGetBarsThenVerifyzetSysmanPciGetBarsCallSucceedsWith1_2ExtensionWrongType) {
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
        EXPECT_NE(pBarProps[i].base, 0u);
        EXPECT_NE(pBarProps[i].size, 0u);
        EXPECT_EQ(props1_2[i].stype, zes_structure_type_t::ZES_STRUCTURE_TYPE_PCI_STATE);
    }
}

TEST_F(ZesPciFixture, GivenValidSysmanHandleWhenCallingzetSysmanPciGetBarsThenVerifyzetSysmanPciGetBarsCallSucceedsWith1_2ExtensionWithNullPtr) {
    uint32_t count = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDevicePciGetBars(device, &count, nullptr));
    EXPECT_NE(count, 0u);

    zes_pci_bar_properties_t *pBarProps = new zes_pci_bar_properties_t[count];

    for (uint32_t i = 0; i < count; i++) {
        pBarProps[i].pNext = nullptr;
        pBarProps[i].stype = zes_structure_type_t::ZES_STRUCTURE_TYPE_PCI_BAR_PROPERTIES;
    }

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDevicePciGetBars(device, &count, pBarProps));

    for (uint32_t i = 0; i < count; i++) {
        EXPECT_LE(pBarProps[i].type, ZES_PCI_BAR_TYPE_MEM);
        EXPECT_NE(pBarProps[i].base, 0u);
        EXPECT_NE(pBarProps[i].size, 0u);
    }

    delete[] pBarProps;
}

TEST_F(ZesPciFixture, GivenValidSysmanHandleWhenCallingzetSysmanPciGetStateThenVerifyzetSysmanPciGetStateCallReturnNotSupported) {
    zes_pci_state_t state;
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesDevicePciGetState(device, &state));
}

TEST_F(ZesPciFixture, GivenValidSysmanHandleWhenCallingzetSysmanPciGetStatsThenVerifyzetSysmanPciGetStatsCallReturnNotSupported) {
    zes_pci_stats_t stats;
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesDevicePciGetStats(device, &stats));
}

TEST_F(ZesPciFixture, WhenConvertingLinkSpeedThenResultIsCorrect) {
    for (int32_t i = PciGenerations::PciGen1; i <= PciGenerations::PciGen5; i++) {
        double speed = convertPciGenToLinkSpeed(i);
        int32_t gen = convertLinkSpeedToPciGen(speed);
        EXPECT_EQ(i, gen);
    }

    EXPECT_EQ(-1, convertLinkSpeedToPciGen(0.0));
    EXPECT_EQ(0.0, convertPciGenToLinkSpeed(0));
}

// This test validates convertPcieSpeedFromGTsToBs method.
// convertPcieSpeedFromGTsToBs(double maxLinkSpeedInGt) method will
// return real PCIe speed in bytes per second as per below formula:
// maxLinkSpeedInGt * (Gigabit to Megabit) * Encoding * (Mb/s to bytes/second) =
// maxLinkSpeedInGt * convertGigabitToMegabit * Encoding * convertMegabitsPerSecondToBytesPerSecond;

TEST_F(ZesPciFixture, WhenConvertingLinkSpeedFromGigatransfersPerSecondToBytesPerSecondThenResultIsCorrect) {
    int64_t speedPci320 = convertPcieSpeedFromGTsToBs(PciLinkSpeeds::Pci32_0GigatransfersPerSecond);
    EXPECT_EQ(speedPci320, static_cast<int64_t>(PciLinkSpeeds::Pci32_0GigatransfersPerSecond * convertMegabitsPerSecondToBytesPerSecond * convertGigabitToMegabit * encodingGen3andAbove));
    int64_t speedPci160 = convertPcieSpeedFromGTsToBs(PciLinkSpeeds::Pci16_0GigatransfersPerSecond);
    EXPECT_EQ(speedPci160, static_cast<int64_t>(PciLinkSpeeds::Pci16_0GigatransfersPerSecond * convertMegabitsPerSecondToBytesPerSecond * convertGigabitToMegabit * encodingGen3andAbove));
    int64_t speedPci80 = convertPcieSpeedFromGTsToBs(PciLinkSpeeds::Pci8_0GigatransfersPerSecond);
    EXPECT_EQ(speedPci80, static_cast<int64_t>(PciLinkSpeeds::Pci8_0GigatransfersPerSecond * convertMegabitsPerSecondToBytesPerSecond * convertGigabitToMegabit * encodingGen3andAbove));
    int64_t speedPci50 = convertPcieSpeedFromGTsToBs(PciLinkSpeeds::Pci5_0GigatransfersPerSecond);
    EXPECT_EQ(speedPci50, static_cast<int64_t>(PciLinkSpeeds::Pci5_0GigatransfersPerSecond * convertMegabitsPerSecondToBytesPerSecond * convertGigabitToMegabit * encodingGen1Gen2));
    int64_t speedPci25 = convertPcieSpeedFromGTsToBs(PciLinkSpeeds::Pci2_5GigatransfersPerSecond);
    EXPECT_EQ(speedPci25, static_cast<int64_t>(PciLinkSpeeds::Pci2_5GigatransfersPerSecond * convertMegabitsPerSecondToBytesPerSecond * convertGigabitToMegabit * encodingGen1Gen2));
    EXPECT_EQ(0, convertPcieSpeedFromGTsToBs(0.0));
}

} // namespace ult
} // namespace L0
