/*
 * Copyright (C) 2023-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/sysman/source/api/pci/sysman_pci_utils.h"
#include "level_zero/sysman/test/unit_tests/sources/linux/mock_sysman_fixture.h"
#include "level_zero/sysman/test/unit_tests/sources/linux/mocks/mock_sysman_product_helper.h"
#include "level_zero/sysman/test/unit_tests/sources/pci/linux/mock_sysfs_pci.h"

#include <string>

namespace L0 {
namespace Sysman {
namespace ult {

constexpr uint32_t expectedBus = 0u;
constexpr uint32_t expectedDevice = 2u;
constexpr uint32_t expectedFunction = 0u;
constexpr int32_t expectedWidth = 1u;
constexpr int32_t expectedGen = 1u;
constexpr int64_t expectedBandwidth = 250000000u;
constexpr int convertMegabitsPerSecondToBytesPerSecond = 125000;
constexpr int convertGigabitToMegabit = 1000;
constexpr double encodingGen1Gen2 = 0.8;
constexpr double encodingGen3andAbove = 0.98461538461;

inline static int openMockReturnFailure(const char *pathname, int flags) {
    return -1;
}

inline static int openMockReturnSuccess(const char *pathname, int flags) {
    NEO::SysCalls::closeFuncCalled = 0;
    return 0;
}

ssize_t preadMock(int fd, void *buf, size_t count, off_t offset) {
    EXPECT_EQ(0u, NEO::SysCalls::closeFuncCalled);
    uint8_t *mockBuf = static_cast<uint8_t *>(buf);
    // Sample config values
    if (count == PCI_CFG_SPACE_EXP_SIZE) {
        mockBuf[0x006] = 0x10;
        mockBuf[0x034] = 0x40;
        mockBuf[0x040] = 0x0d;
        mockBuf[0x041] = 0x50;
        mockBuf[0x050] = 0x10;
        mockBuf[0x051] = 0x70;
        mockBuf[0x052] = 0x90;
        mockBuf[0x070] = 0x10;
        mockBuf[0x071] = 0xac;
        mockBuf[0x072] = 0xa0;
        mockBuf[0x0ac] = 0x10;
        mockBuf[0x0b8] = 0x11;
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
        return PCI_CFG_SPACE_EXP_SIZE;
    } else if (count == PCI_CFG_SPACE_SIZE) {
        mockBuf[0x006] = 0x10;
        mockBuf[0x034] = 0x40;
        mockBuf[0x040] = 0x0d;
        mockBuf[0x041] = 0x50;
        mockBuf[0x050] = 0x10;
        mockBuf[0x051] = 0x70;
        mockBuf[0x052] = 0x90;
        mockBuf[0x070] = 0x10;
        mockBuf[0x071] = 0xac;
        mockBuf[0x072] = 0xa0;
        mockBuf[0x0ac] = 0x10;
        mockBuf[0x0b8] = 0x11;
        return PCI_CFG_SPACE_SIZE;
    }
    return -1;
}

ssize_t preadMockHeaderFailure(int fd, void *buf, size_t count, off_t offset) {
    if (count == PCI_CFG_SPACE_EXP_SIZE) {
        return PCI_CFG_SPACE_EXP_SIZE;
    } else if (count == PCI_CFG_SPACE_SIZE) {
        return PCI_CFG_SPACE_SIZE;
    }
    return -1;
}

ssize_t preadMockInvalidPos(int fd, void *buf, size_t count, off_t offset) {
    uint8_t *mockBuf = static_cast<uint8_t *>(buf);
    // Sample config values
    if (count == PCI_CFG_SPACE_EXP_SIZE) {
        mockBuf[0x006] = 0x10;
        mockBuf[0x034] = 0x40;
        mockBuf[0x040] = 0x0d;
        mockBuf[0x041] = 0x50;
        mockBuf[0x050] = 0x10;
        mockBuf[0x051] = 0x70;
        mockBuf[0x052] = 0x90;
        mockBuf[0x070] = 0x10;
        mockBuf[0x071] = 0xac;
        mockBuf[0x072] = 0xa0;
        mockBuf[0x0ac] = 0x11;
        mockBuf[0x0ad] = 0x00;
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
        return PCI_CFG_SPACE_EXP_SIZE;
    } else if (count == PCI_CFG_SPACE_SIZE) {
        mockBuf[0x006] = 0x10;
        mockBuf[0x034] = 0x40;
        mockBuf[0x040] = 0x0d;
        mockBuf[0x041] = 0x50;
        mockBuf[0x050] = 0x10;
        mockBuf[0x051] = 0x70;
        mockBuf[0x052] = 0x90;
        mockBuf[0x070] = 0x10;
        mockBuf[0x071] = 0xac;
        mockBuf[0x072] = 0xa0;
        mockBuf[0x0ac] = 0x11;
        mockBuf[0x0ad] = 0x00;
        return PCI_CFG_SPACE_SIZE;
    }
    return -1;
}

ssize_t preadMockLoop(int fd, void *buf, size_t count, off_t offset) {
    uint8_t *mockBuf = static_cast<uint8_t *>(buf);
    // Sample config values
    if (count == PCI_CFG_SPACE_EXP_SIZE) {
        mockBuf[0x006] = 0x10;
        mockBuf[0x034] = 0x40;
        mockBuf[0x040] = 0x0d;
        mockBuf[0x041] = 0x50;
        mockBuf[0x050] = 0x10;
        mockBuf[0x051] = 0x70;
        mockBuf[0x052] = 0x90;
        mockBuf[0x070] = 0x10;
        mockBuf[0x071] = 0xac;
        mockBuf[0x072] = 0xa0;
        mockBuf[0x0ac] = 0x0d;
        mockBuf[0x0ad] = 0x40;
        mockBuf[0x0b8] = 0x11;
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
        return PCI_CFG_SPACE_EXP_SIZE;
    } else if (count == PCI_CFG_SPACE_SIZE) {
        mockBuf[0x006] = 0x10;
        mockBuf[0x034] = 0x40;
        mockBuf[0x040] = 0x0d;
        mockBuf[0x041] = 0x50;
        mockBuf[0x050] = 0x10;
        mockBuf[0x051] = 0x70;
        mockBuf[0x052] = 0x90;
        mockBuf[0x070] = 0x0d;
        mockBuf[0x071] = 0xac;
        mockBuf[0x072] = 0xa0;
        mockBuf[0x0ac] = 0x0d;
        mockBuf[0x0ad] = 0x40;
        mockBuf[0x0b8] = 0x11;
        return PCI_CFG_SPACE_SIZE;
    }
    return -1;
}

ssize_t preadMockFailure(int fd, void *buf, size_t count, off_t offset) {
    return -1;
}

class ZesPciFixture : public SysmanDeviceFixture {

  protected:
    std::unique_ptr<MockPciSysfsAccess> pSysfsAccess;
    L0::Sysman::SysmanDevice *device = nullptr;
    L0::Sysman::SysFsAccessInterface *pOriginalSysfsAccess = nullptr;
    L0::Sysman::FsAccessInterface *pOriginalFsAccess = nullptr;
    L0::Sysman::PciImp *pPciImp;
    L0::Sysman::OsPci *pOsPciPrev;
    std::unique_ptr<L0::ult::Mock<L0::DriverHandleImp>> driverHandle;
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> openBackup{&NEO::SysCalls::sysCallsOpen, openMockReturnSuccess};

    void SetUp() override {
        SysmanDeviceFixture::SetUp();
        device = pSysmanDevice;
        pSysfsAccess = std::make_unique<MockPciSysfsAccess>();
        pOriginalSysfsAccess = pLinuxSysmanImp->pSysfsAccess;
        pLinuxSysmanImp->pSysfsAccess = pSysfsAccess.get();

        pSysmanDeviceImp->getRootDeviceEnvironment().getMutableHardwareInfo()->capabilityTable.isIntegratedDevice = false;
        pPciImp = static_cast<L0::Sysman::PciImp *>(pSysmanDeviceImp->pPci);
        pOsPciPrev = pPciImp->pOsPci;
        pPciImp->pOsPci = nullptr;
        PublicLinuxPciImp *pLinuxPciImp = new PublicLinuxPciImp(pOsSysman);
        pLinuxPciImp->preadFunction = preadMock;

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
    L0::Sysman::OsPci *pOsPciOriginal = pPciImp->pOsPci;
    PublicLinuxPciImp *pLinuxPciImpTemp = new PublicLinuxPciImp(pOsSysman);
    pLinuxPciImpTemp->preadFunction = preadMock;

    pPciImp->pOsPci = static_cast<L0::Sysman::OsPci *>(pLinuxPciImpTemp);
    pPciImp->pciGetStaticFields();

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

    delete pLinuxPciImpTemp;
    pPciImp->pOsPci = pOsPciOriginal;
}

TEST_F(ZesPciFixture, GivenValidSysmanHandleWhenCallingzetSysmanPciGetPropertiesAndBdfStringIsEmptyThenVerifyApiCallSucceeds) {
    zes_pci_properties_t properties;

    pSysfsAccess->isStringSymLinkEmpty = true;

    ze_result_t result = zesDevicePciGetProperties(device, &properties);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(properties.address.bus, 0u);
    EXPECT_EQ(properties.address.device, 0u);
    EXPECT_EQ(properties.address.function, 0u);
}

TEST_F(ZesPciFixture, GivenSysmanHandleWhenGettingPCIWidthAndSpeedAndCapabilityLinkListIsBrokenThenInvalidValuesAreReturned) {
    int32_t width = 0;
    double speed = 0;
    L0::Sysman::OsPci *pOsPciOriginal = pPciImp->pOsPci;
    PublicLinuxPciImp *pLinuxPciImpTemp = new PublicLinuxPciImp(pOsSysman);
    pLinuxPciImpTemp->preadFunction = preadMockInvalidPos;

    pPciImp->pOsPci = static_cast<L0::Sysman::OsPci *>(pLinuxPciImpTemp);
    pPciImp->pciGetStaticFields();
    pPciImp->pOsPci->getMaxLinkCaps(speed, width);
    EXPECT_EQ(width, -1);
    EXPECT_EQ(speed, 0);

    delete pLinuxPciImpTemp;
    pPciImp->pOsPci = pOsPciOriginal;
}

TEST_F(ZesPciFixture, GivenSysmanHandleWhenGettingPCIWidthAndSpeedForIntegratedDevicesThenInvalidValuesAreReturned) {
    int32_t width = 0;
    double speed = 0;
    pSysmanDeviceImp->getRootDeviceEnvironment().getMutableHardwareInfo()->capabilityTable.isIntegratedDevice = true;
    L0::Sysman::OsPci *pOsPciOriginal = pPciImp->pOsPci;
    PublicLinuxPciImp *pLinuxPciImpTemp = new PublicLinuxPciImp(pOsSysman);
    pLinuxPciImpTemp->preadFunction = preadMock;

    pPciImp->pOsPci = static_cast<L0::Sysman::OsPci *>(pLinuxPciImpTemp);
    pPciImp->pciGetStaticFields();
    pPciImp->pOsPci->getMaxLinkCaps(speed, width);
    EXPECT_EQ(width, -1);
    EXPECT_EQ(speed, 0);

    delete pLinuxPciImpTemp;
    pPciImp->pOsPci = pOsPciOriginal;
}

TEST_F(ZesPciFixture, GivenSysmanHandleWhenGettingPCIWidthAndSpeedAndPCIExpressCapabilityIsNotPresentThenInvalidValuesAreReturned) {
    int32_t width = 0;
    double speed = 0;
    L0::Sysman::OsPci *pOsPciOriginal = pPciImp->pOsPci;
    PublicLinuxPciImp *pLinuxPciImpTemp = new PublicLinuxPciImp(pOsSysman);
    pLinuxPciImpTemp->preadFunction = preadMockLoop;

    pPciImp->pOsPci = static_cast<L0::Sysman::OsPci *>(pLinuxPciImpTemp);
    pPciImp->pciGetStaticFields();
    pPciImp->pOsPci->getMaxLinkCaps(speed, width);
    EXPECT_EQ(width, -1);
    EXPECT_EQ(speed, 0);

    delete pLinuxPciImpTemp;
    pPciImp->pOsPci = pOsPciOriginal;
}

TEST_F(ZesPciFixture, GivenSysmanHandleWhenGettingPCIWidthAndSpeedAndUserIsNonRootThenInvalidValuesAreReturned) {
    int32_t width = 0;
    double speed = 0;
    L0::Sysman::OsPci *pOsPciOriginal = pPciImp->pOsPci;
    PublicLinuxPciImp *pLinuxPciImpTemp = new PublicLinuxPciImp(pOsSysman);
    pLinuxPciImpTemp->preadFunction = preadMock;

    pSysfsAccess->isRootUserResult = false;

    pPciImp->pOsPci = static_cast<L0::Sysman::OsPci *>(pLinuxPciImpTemp);
    pPciImp->pciGetStaticFields();
    pPciImp->pOsPci->getMaxLinkCaps(speed, width);
    EXPECT_EQ(width, -1);
    EXPECT_EQ(speed, 0);

    delete pLinuxPciImpTemp;
    pPciImp->pOsPci = pOsPciOriginal;
}

TEST_F(ZesPciFixture, GivenSysmanHandleWhenInitializingPciAndPciConfigOpenFailsThenInvalidSpeedAndWidthAreReturned) {
    int32_t width = 0;
    double speed = 0;
    L0::Sysman::OsPci *pOsPciOriginal = pPciImp->pOsPci;
    PublicLinuxPciImp *pLinuxPciImpTemp = new PublicLinuxPciImp(pOsSysman);
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> openBackup(&NEO::SysCalls::sysCallsOpen, openMockReturnFailure);
    pLinuxPciImpTemp->preadFunction = preadMock;

    pPciImp->pOsPci = static_cast<L0::Sysman::OsPci *>(pLinuxPciImpTemp);
    pPciImp->pciGetStaticFields();
    pPciImp->pOsPci->getMaxLinkCaps(speed, width);
    EXPECT_EQ(width, -1);
    EXPECT_EQ(speed, 0);

    delete pLinuxPciImpTemp;
    pPciImp->pOsPci = pOsPciOriginal;
}

TEST_F(ZesPciFixture, GivenSysmanHandleWhenGettingPCIWidthAndSpeedAndPCIHeaderIsAbsentThenInvalidValuesAreReturned) {
    int32_t width = 0;
    double speed = 0;
    L0::Sysman::OsPci *pOsPciOriginal = pPciImp->pOsPci;
    PublicLinuxPciImp *pLinuxPciImpTemp = new PublicLinuxPciImp(pOsSysman);
    pLinuxPciImpTemp->preadFunction = preadMockHeaderFailure;

    pPciImp->pOsPci = static_cast<L0::Sysman::OsPci *>(pLinuxPciImpTemp);
    pPciImp->pciGetStaticFields();
    pPciImp->pOsPci->getMaxLinkCaps(speed, width);
    EXPECT_EQ(width, -1);
    EXPECT_EQ(speed, 0);

    delete pLinuxPciImpTemp;
    pPciImp->pOsPci = pOsPciOriginal;
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
    L0::Sysman::OsPci *pOsPciOriginal = pPciImp->pOsPci;
    PublicLinuxPciImp *pLinuxPciImpTemp = new PublicLinuxPciImp(pOsSysman);
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> openBackup(&NEO::SysCalls::sysCallsOpen, openMockReturnFailure);
    pLinuxPciImpTemp->preadFunction = preadMock;

    pPciImp->pOsPci = static_cast<L0::Sysman::OsPci *>(pLinuxPciImpTemp);
    pPciImp->pciGetStaticFields();
    EXPECT_FALSE(pPciImp->pOsPci->resizableBarSupported());
    uint32_t barIndex = 2u;
    EXPECT_FALSE(pPciImp->pOsPci->resizableBarEnabled(barIndex));
    delete pLinuxPciImpTemp;
    pPciImp->pOsPci = pOsPciOriginal;
}

TEST_F(ZesPciFixture, GivenValidSysmanHandleWhenInitializingPciAndPciConfigReadFailsThenResizableBarSupportWillBeFalse) {
    L0::Sysman::OsPci *pOsPciOriginal = pPciImp->pOsPci;
    PublicLinuxPciImp *pLinuxPciImpTemp = new PublicLinuxPciImp(pOsSysman);
    pLinuxPciImpTemp->preadFunction = preadMockFailure;

    pPciImp->pOsPci = static_cast<L0::Sysman::OsPci *>(pLinuxPciImpTemp);
    pPciImp->pciGetStaticFields();
    EXPECT_FALSE(pPciImp->pOsPci->resizableBarSupported());
    uint32_t barIndex = 2u;
    EXPECT_FALSE(pPciImp->pOsPci->resizableBarEnabled(barIndex));
    delete pLinuxPciImpTemp;
    pPciImp->pOsPci = pOsPciOriginal;
}

TEST_F(ZesPciFixture, GivenSysmanHandleWhenCheckForResizableBarSupportAndHeaderFieldNotPresentThenResizableBarSupportFalseReturned) {
    L0::Sysman::OsPci *pOsPciOriginal = pPciImp->pOsPci;
    PublicLinuxPciImp *pLinuxPciImpTemp = new PublicLinuxPciImp(pOsSysman);
    pLinuxPciImpTemp->preadFunction = preadMockHeaderFailure;

    pPciImp->pOsPci = static_cast<L0::Sysman::OsPci *>(pLinuxPciImpTemp);
    pPciImp->pciGetStaticFields();
    EXPECT_FALSE(pPciImp->pOsPci->resizableBarSupported());
    uint32_t barIndex = 2u;
    EXPECT_FALSE(pPciImp->pOsPci->resizableBarEnabled(barIndex));
    delete pLinuxPciImpTemp;
    pPciImp->pOsPci = pOsPciOriginal;
}

TEST_F(ZesPciFixture, GivenSysmanHandleWhenCheckForResizableBarSupportAndCapabilityLinkListIsBrokenThenResizableBarSupportFalseReturned) {
    L0::Sysman::OsPci *pOsPciOriginal = pPciImp->pOsPci;
    PublicLinuxPciImp *pLinuxPciImpTemp = new PublicLinuxPciImp(pOsSysman);
    pLinuxPciImpTemp->preadFunction = preadMockInvalidPos;

    pPciImp->pOsPci = static_cast<L0::Sysman::OsPci *>(pLinuxPciImpTemp);
    pPciImp->pciGetStaticFields();
    EXPECT_FALSE(pPciImp->pOsPci->resizableBarSupported());
    uint32_t barIndex = 2u;
    EXPECT_FALSE(pPciImp->pOsPci->resizableBarEnabled(barIndex));
    delete pLinuxPciImpTemp;
    pPciImp->pOsPci = pOsPciOriginal;
}

TEST_F(ZesPciFixture, GivenSysmanHandleWhenCheckForResizableBarSupportAndIfRebarCapabilityNotPresentThenResizableBarSupportFalseReturned) {
    L0::Sysman::OsPci *pOsPciOriginal = pPciImp->pOsPci;
    PublicLinuxPciImp *pLinuxPciImpTemp = new PublicLinuxPciImp(pOsSysman);
    pLinuxPciImpTemp->preadFunction = preadMockLoop;

    pPciImp->pOsPci = static_cast<L0::Sysman::OsPci *>(pLinuxPciImpTemp);
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
    std::vector<zes_pci_bar_properties_1_2_t> props12(count);
    for (uint32_t i = 0; i < count; i++) {
        props12[i].stype = ZES_STRUCTURE_TYPE_PCI_BAR_PROPERTIES_1_2;
        props12[i].pNext = nullptr;
        pBarProps[i].stype = ZES_STRUCTURE_TYPE_PCI_BAR_PROPERTIES;
        pBarProps[i].pNext = static_cast<void *>(&props12[i]);
    }

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDevicePciGetBars(device, &count, pBarProps.data()));

    for (uint32_t i = 0; i < count; i++) {
        EXPECT_EQ(pBarProps[i].stype, zes_structure_type_t::ZES_STRUCTURE_TYPE_PCI_BAR_PROPERTIES);
        EXPECT_LE(pBarProps[i].type, ZES_PCI_BAR_TYPE_MEM);
        EXPECT_NE(pBarProps[i].base, 0u);
        EXPECT_NE(pBarProps[i].size, 0u);
        EXPECT_EQ(props12[i].stype, zes_structure_type_t::ZES_STRUCTURE_TYPE_PCI_BAR_PROPERTIES_1_2);
        EXPECT_EQ(props12[i].resizableBarSupported, true);
        if (props12[i].index == 2) {
            EXPECT_EQ(props12[i].resizableBarEnabled, true);
        } else {
            EXPECT_EQ(props12[i].resizableBarEnabled, false);
        }
        EXPECT_LE(props12[i].type, ZES_PCI_BAR_TYPE_MEM);
        EXPECT_NE(props12[i].base, 0u);
        EXPECT_NE(props12[i].size, 0u);
    }
}

TEST_F(ZesPciFixture, GivenValidSysmanHandleWhenCallingzetSysmanPciGetBarsThenVerifyzetSysmanPciGetBarsCallSucceedsWith1_2ExtensionWrongType) {
    uint32_t count = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDevicePciGetBars(device, &count, nullptr));
    EXPECT_NE(count, 0u);

    std::vector<zes_pci_bar_properties_t> pBarProps(count);
    std::vector<zes_pci_bar_properties_1_2_t> props12(count);
    for (uint32_t i = 0; i < count; i++) {
        props12[i].stype = ZES_STRUCTURE_TYPE_PCI_STATE;
        props12[i].pNext = nullptr;
        pBarProps[i].stype = ZES_STRUCTURE_TYPE_PCI_BAR_PROPERTIES;
        pBarProps[i].pNext = static_cast<void *>(&props12[i]);
    }

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDevicePciGetBars(device, &count, pBarProps.data()));

    for (uint32_t i = 0; i < count; i++) {
        EXPECT_EQ(pBarProps[i].stype, zes_structure_type_t::ZES_STRUCTURE_TYPE_PCI_BAR_PROPERTIES);
        EXPECT_LE(pBarProps[i].type, ZES_PCI_BAR_TYPE_MEM);
        EXPECT_NE(pBarProps[i].base, 0u);
        EXPECT_NE(pBarProps[i].size, 0u);
        EXPECT_EQ(props12[i].stype, zes_structure_type_t::ZES_STRUCTURE_TYPE_PCI_STATE);
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

TEST_F(ZesPciFixture, GivenValidLinuxPciImpInstanceAndGetPciStatsFailsFromSysmanProductHelperWhenGetStatsIsCalledThenCallFails) {
    std::unique_ptr<MockSysmanProductHelper> pMockSysmanProductHelper = std::make_unique<MockSysmanProductHelper>();
    pMockSysmanProductHelper->mockGetPciStatsResult = ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    std::unique_ptr<PublicLinuxPciImp> pMockLinuxPciImp = std::make_unique<PublicLinuxPciImp>(pOsSysman);
    pLinuxSysmanImp->pSysmanProductHelper = std::move(pMockSysmanProductHelper);

    auto pOsPciPrev = pPciImp->pOsPci;
    pPciImp->pOsPci = pMockLinuxPciImp.get();

    zes_pci_stats_t stats;
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesDevicePciGetStats(device, &stats));

    pPciImp->pOsPci = pOsPciPrev;
}

TEST_F(ZesPciFixture, GivenValidLinuxPciImpInstanceWhenGetStatsIsCalledThenCallSucceeds) {
    std::unique_ptr<SysmanProductHelper> pMockSysmanProductHelper = std::make_unique<MockSysmanProductHelper>();
    std::unique_ptr<PublicLinuxPciImp> pMockLinuxPciImp = std::make_unique<PublicLinuxPciImp>(pOsSysman);
    pLinuxSysmanImp->pSysmanProductHelper = std::move(pMockSysmanProductHelper);

    auto pOsPciPrev = pPciImp->pOsPci;
    pPciImp->pOsPci = pMockLinuxPciImp.get();

    zes_pci_stats_t stats;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDevicePciGetStats(device, &stats));

    pPciImp->pOsPci = pOsPciPrev;
}

TEST_F(ZesPciFixture, WhenConvertingLinkSpeedThenResultIsCorrect) {
    for (int32_t i = PciGenerations::PciGen1; i <= PciGenerations::PciGen5; i++) {
        double speed = L0::Sysman::convertPciGenToLinkSpeed(i);
        int32_t gen = L0::Sysman::convertLinkSpeedToPciGen(speed);
        EXPECT_EQ(i, gen);
    }

    EXPECT_EQ(-1, L0::Sysman::convertLinkSpeedToPciGen(0.0));
    EXPECT_EQ(0.0, L0::Sysman::convertPciGenToLinkSpeed(0));
}

// This test validates convertPcieSpeedFromGTsToBs method.
// convertPcieSpeedFromGTsToBs(double maxLinkSpeedInGt) method will
// return real PCIe speed in bytes per second as per below formula:
// maxLinkSpeedInGt * (Gigabit to Megabit) * Encoding * (Mb/s to bytes/second) =
// maxLinkSpeedInGt * convertGigabitToMegabit * Encoding * convertMegabitsPerSecondToBytesPerSecond;

TEST_F(ZesPciFixture, WhenConvertingLinkSpeedFromGigaTransfersPerSecondToBytesPerSecondThenResultIsCorrect) {
    int64_t speedPci320 = L0::Sysman::convertPcieSpeedFromGTsToBs(PciLinkSpeeds::pci32GigaTransfersPerSecond);
    EXPECT_EQ(speedPci320, static_cast<int64_t>(PciLinkSpeeds::pci32GigaTransfersPerSecond * convertMegabitsPerSecondToBytesPerSecond * convertGigabitToMegabit * encodingGen3andAbove));
    int64_t speedPci160 = L0::Sysman::convertPcieSpeedFromGTsToBs(PciLinkSpeeds::pci16GigaTransfersPerSecond);
    EXPECT_EQ(speedPci160, static_cast<int64_t>(PciLinkSpeeds::pci16GigaTransfersPerSecond * convertMegabitsPerSecondToBytesPerSecond * convertGigabitToMegabit * encodingGen3andAbove));
    int64_t speedPci80 = L0::Sysman::convertPcieSpeedFromGTsToBs(PciLinkSpeeds::pci8GigaTransfersPerSecond);
    EXPECT_EQ(speedPci80, static_cast<int64_t>(PciLinkSpeeds::pci8GigaTransfersPerSecond * convertMegabitsPerSecondToBytesPerSecond * convertGigabitToMegabit * encodingGen3andAbove));
    int64_t speedPci50 = L0::Sysman::convertPcieSpeedFromGTsToBs(PciLinkSpeeds::pci5GigaTransfersPerSecond);
    EXPECT_EQ(speedPci50, static_cast<int64_t>(PciLinkSpeeds::pci5GigaTransfersPerSecond * convertMegabitsPerSecondToBytesPerSecond * convertGigabitToMegabit * encodingGen1Gen2));
    int64_t speedPci25 = L0::Sysman::convertPcieSpeedFromGTsToBs(PciLinkSpeeds::pci2Dot5GigaTransfersPerSecond);
    EXPECT_EQ(speedPci25, static_cast<int64_t>(PciLinkSpeeds::pci2Dot5GigaTransfersPerSecond * convertMegabitsPerSecondToBytesPerSecond * convertGigabitToMegabit * encodingGen1Gen2));
    EXPECT_EQ(0, L0::Sysman::convertPcieSpeedFromGTsToBs(0.0));
}

TEST_F(ZesPciFixture, GivenValidConfigMemoryDataWhenCallingGetRebarCapabilityPosThenTrueValueIsReturned) {
    std::unique_ptr<PublicLinuxPciImp> pLinuxPciImp = std::make_unique<PublicLinuxPciImp>(pOsSysman);
    std::vector<uint8_t> configMemory(PCI_CFG_SPACE_EXP_SIZE);
    uint8_t *mockBuf = configMemory.data();
    mockBuf[0x006] = 0x10;
    mockBuf[0x034] = 0x40;
    mockBuf[0x040] = 0x0d;
    mockBuf[0x041] = 0x50;
    mockBuf[0x050] = 0x10;
    mockBuf[0x051] = 0x70;
    mockBuf[0x052] = 0x90;
    mockBuf[0x070] = 0x10;
    mockBuf[0x071] = 0xac;
    mockBuf[0x072] = 0xa0;
    mockBuf[0x0ac] = 0x10;
    mockBuf[0x0b8] = 0x11;
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
    EXPECT_TRUE(pLinuxPciImp->getRebarCapabilityPos(mockBuf, true));
}

} // namespace ult
} // namespace Sysman
} // namespace L0
