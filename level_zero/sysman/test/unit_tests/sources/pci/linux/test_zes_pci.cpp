/*
 * Copyright (C) 2023-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/sysman/source/api/pci/sysman_pci_utils.h"
#include "level_zero/sysman/test/unit_tests/sources/linux/mocks/mock_sysman_product_helper.h"
#include "level_zero/sysman/test/unit_tests/sources/pci/linux/mock_sysfs_pci.h"
#include "level_zero/sysman/test/unit_tests/sources/shared/linux/kmd_interface/mock_sysman_kmd_interface_xe.h"

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

constexpr ssize_t pciStandardHeaderSize = 64;

template <ssize_t (*readMockFunction)(int fd, void *buf, size_t count)>
ssize_t readMockTruncatedToStandardHeader(int fd, void *buf, size_t count) {
    ssize_t bytesRead = readMockFunction(fd, buf, count);
    if (bytesRead <= pciStandardHeaderSize) {
        return bytesRead;
    }
    memset(static_cast<uint8_t *>(buf) + pciStandardHeaderSize, 0, count - pciStandardHeaderSize);
    return pciStandardHeaderSize;
}

ssize_t readMock(int fd, void *buf, size_t count) {
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

ssize_t readMockHeaderFailure(int fd, void *buf, size_t count) {
    if (count == PCI_CFG_SPACE_EXP_SIZE) {
        return PCI_CFG_SPACE_EXP_SIZE;
    } else if (count == PCI_CFG_SPACE_SIZE) {
        return PCI_CFG_SPACE_SIZE;
    }
    return -1;
}

ssize_t readMockInvalidPos(int fd, void *buf, size_t count) {
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

ssize_t readMockLoop(int fd, void *buf, size_t count) {
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

ssize_t readMockFailure(int fd, void *buf, size_t count) {
    return -1;
}

class ZesPciFixture : public SysmanDeviceFixture {

  protected:
    std::unique_ptr<MockPciSysfsAccess> pSysfsAccess;
    std::unique_ptr<MockPciFsAccess> pFsAccess;
    L0::Sysman::SysmanDevice *device = nullptr;
    L0::Sysman::SysFsAccessInterface *pOriginalSysfsAccess = nullptr;
    L0::Sysman::FsAccessInterface *pOriginalFsAccess = nullptr;
    L0::Sysman::PciImp *pPciImp;
    L0::Sysman::OsPci *pOsPciPrev;
    std::unique_ptr<L0::ult::Mock<L0::DriverHandle>> driverHandle;
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> openBackup{&NEO::SysCalls::sysCallsOpen, openMockReturnSuccess};
    VariableBackup<decltype(NEO::SysCalls::sysCallsRead)> readBackup{&NEO::SysCalls::sysCallsRead};

    void SetUp() override {
        SysmanDeviceFixture::SetUp();
        device = pSysmanDevice;
        pSysfsAccess = std::make_unique<MockPciSysfsAccess>();
        pOriginalSysfsAccess = pLinuxSysmanImp->pSysfsAccess;
        pLinuxSysmanImp->pSysfsAccess = pSysfsAccess.get();

        pFsAccess = std::make_unique<MockPciFsAccess>();
        pOriginalFsAccess = pLinuxSysmanImp->pFsAccess;
        pLinuxSysmanImp->pFsAccess = pFsAccess.get();

        pSysmanDeviceImp->getRootDeviceEnvironment().getMutableHardwareInfo()->capabilityTable.isIntegratedDevice = false;
        pPciImp = static_cast<L0::Sysman::PciImp *>(pSysmanDeviceImp->pPci);
        pOsPciPrev = pPciImp->pOsPci;
        pPciImp->pOsPci = nullptr;
        PublicLinuxPciImp *pLinuxPciImp = new PublicLinuxPciImp(pOsSysman);
        readBackup = readMock;

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
        pLinuxSysmanImp->pFsAccess = pOriginalFsAccess;
        SysmanDeviceFixture::TearDown();
    }
};

TEST_F(ZesPciFixture, GivenValidSysmanHandleWhenCallingzetSysmanPciGetPropertiesThenVerifyzetSysmanPciGetPropertiesCallSucceeds) {
    zes_pci_properties_t properties = {};
    zes_pci_properties_t propertiesBefore = {};

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
    zes_pci_properties_t properties = {};
    zes_pci_properties_t propertiesBefore = {};
    L0::Sysman::OsPci *pOsPciOriginal = pPciImp->pOsPci;
    PublicLinuxPciImp *pLinuxPciImpTemp = new PublicLinuxPciImp(pOsSysman);
    VariableBackup<decltype(NEO::SysCalls::sysCallsRead)> readMockBackup(&NEO::SysCalls::sysCallsRead, readMock);

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
    zes_pci_properties_t properties = {};

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
    VariableBackup<decltype(NEO::SysCalls::sysCallsRead)> readMockBackup(&NEO::SysCalls::sysCallsRead, readMockInvalidPos);

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
    VariableBackup<decltype(NEO::SysCalls::sysCallsRead)> readMockBackup(&NEO::SysCalls::sysCallsRead, readMock);

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
    VariableBackup<decltype(NEO::SysCalls::sysCallsRead)> readMockBackup(&NEO::SysCalls::sysCallsRead, readMockLoop);

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
    VariableBackup<decltype(NEO::SysCalls::sysCallsRead)> readMockBackup(&NEO::SysCalls::sysCallsRead, readMockTruncatedToStandardHeader<readMock>);

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
    VariableBackup<decltype(NEO::SysCalls::sysCallsRead)> readMockBackup(&NEO::SysCalls::sysCallsRead, readMock);

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
    VariableBackup<decltype(NEO::SysCalls::sysCallsRead)> readMockBackup(&NEO::SysCalls::sysCallsRead, readMockHeaderFailure);

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

TEST_F(ZesPciFixture, GivenValidSysmanHandleWhenInitializingBarPropertiesWithInsufficientDataThenErrorIsReturned) {
    pSysfsAccess->mockResourceReadEmpty = true;
    auto pLinuxPciImpTemp = std::make_unique<PublicLinuxPciImp>(pOsSysman);
    VariableBackup<decltype(NEO::SysCalls::sysCallsRead)> readMockBackup(&NEO::SysCalls::sysCallsRead, readMock);
    pLinuxPciImpTemp->pSysfsAccess = pSysfsAccess.get();
    std::vector<zes_pci_bar_properties_t *> barProps;
    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, pLinuxPciImpTemp->initializeBarProperties(barProps));
}

TEST_F(ZesPciFixture, GivenValidSysmanHandleWhenInitializingPciAndPciConfigOpenFailsThenResizableBarSupportWillBeFalse) {
    L0::Sysman::OsPci *pOsPciOriginal = pPciImp->pOsPci;
    PublicLinuxPciImp *pLinuxPciImpTemp = new PublicLinuxPciImp(pOsSysman);
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> openBackup(&NEO::SysCalls::sysCallsOpen, openMockReturnFailure);
    VariableBackup<decltype(NEO::SysCalls::sysCallsRead)> readMockBackup(&NEO::SysCalls::sysCallsRead, readMock);

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
    VariableBackup<decltype(NEO::SysCalls::sysCallsRead)> readMockBackup(&NEO::SysCalls::sysCallsRead, readMockFailure);

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
    VariableBackup<decltype(NEO::SysCalls::sysCallsRead)> readMockBackup(&NEO::SysCalls::sysCallsRead, readMockHeaderFailure);

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
    VariableBackup<decltype(NEO::SysCalls::sysCallsRead)> readMockBackup(&NEO::SysCalls::sysCallsRead, readMockInvalidPos);

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
    VariableBackup<decltype(NEO::SysCalls::sysCallsRead)> readMockBackup(&NEO::SysCalls::sysCallsRead, readMockLoop);

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

TEST_F(ZesPciFixture, GivenValidSysmanHandleWhenCallingZesDevicePciGetPropertiesWithExtensionStructureOni915KmdThenVerifyApiCallSuceedsWithProperValue) {
    zes_pci_properties_t properties = {};
    zes_intel_pci_link_speed_downgrade_exp_properties_t extProps = {};
    extProps.stype = ZES_INTEL_PCI_LINK_SPEED_DOWNGRADE_EXP_PROPERTIES;
    properties.pNext = &extProps;

    ze_result_t result = zesDevicePciGetProperties(device, &properties);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(extProps.maxPciGenSupported, -1);
}

namespace PciConfigExpMock {
constexpr int deviceConfigFd = 1;
constexpr int cardBusConfigFd = 2;

constexpr uint16_t expectedVendorId = 0x8086;
constexpr uint16_t expectedDeviceId = 0xe221;
constexpr uint16_t expectedSubsystemVendorId = 0x8086;
constexpr uint16_t expectedSubsystemDeviceId = 0x1600;
constexpr uint32_t expectedCapabilityVersion = 2u;
constexpr uint32_t expectedDeviceSpeedsVector = ZES_INTEL_PCI_LINK_SPEED_EXP_FLAG_GEN1;
constexpr uint32_t expectedCardBusSpeedsVector = ZES_INTEL_PCI_LINK_SPEED_EXP_FLAG_GEN1 | ZES_INTEL_PCI_LINK_SPEED_EXP_FLAG_GEN2 |
                                                 ZES_INTEL_PCI_LINK_SPEED_EXP_FLAG_GEN3 | ZES_INTEL_PCI_LINK_SPEED_EXP_FLAG_GEN4;

constexpr uint8_t defaultPcieCapPos = 0x70;
constexpr uint8_t truncatedPcieCapPos = 0xf0;
constexpr uint8_t unrelatedCapPos = 0x40;
constexpr uint8_t unrelatedCapId = 0x01;

uint16_t vendorId = expectedVendorId;
uint8_t status = PCI_STATUS_CAP_LIST;
uint8_t pcieCapPos = defaultPcieCapPos;
bool pcieCapabilityInChain = true;
bool capabilityChainSelfReferences = false;
uint16_t capRegister = 0x0002;          // capability version 2, endpoint
uint32_t linkCaps2 = 0x00000002;        // supported link speeds vector 0x01
uint32_t cardBusLinkCaps2 = 0x0000001e; // supported link speeds vector 0x0f
bool readFailure = false;
bool cardBusReadFailure = false;

void reset() {
    vendorId = expectedVendorId;
    status = PCI_STATUS_CAP_LIST;
    pcieCapPos = defaultPcieCapPos;
    pcieCapabilityInChain = true;
    capabilityChainSelfReferences = false;
    capRegister = 0x0002;
    linkCaps2 = 0x00000002;
    cardBusLinkCaps2 = 0x0000001e;
    readFailure = false;
    cardBusReadFailure = false;
}

void writeByte(uint8_t *mockBuf, size_t configSpaceSize, uint32_t pos, uint8_t value) {
    if ((pos + sizeof(uint8_t)) > configSpaceSize) {
        return;
    }
    mockBuf[pos] = value;
}

void writeWord(uint8_t *mockBuf, size_t configSpaceSize, uint32_t pos, uint16_t value) {
    if ((pos + sizeof(uint16_t)) > configSpaceSize) {
        return;
    }
    mockBuf[pos] = value & 0xff;
    mockBuf[pos + 1] = (value >> 8) & 0xff;
}

void writeDword(uint8_t *mockBuf, size_t configSpaceSize, uint32_t pos, uint32_t value) {
    if ((pos + sizeof(uint32_t)) > configSpaceSize) {
        return;
    }
    writeWord(mockBuf, configSpaceSize, pos, value & 0xffff);
    writeWord(mockBuf, configSpaceSize, pos + 2, (value >> 16) & 0xffff);
}

void writeCapabilityChain(uint8_t *mockBuf, size_t configSpaceSize) {
    if (pcieCapabilityInChain) {
        writeByte(mockBuf, configSpaceSize, PCI_CAPABILITY_LIST, pcieCapPos);
        writeByte(mockBuf, configSpaceSize, pcieCapPos + PCI_CAP_LIST_ID, PCI_CAP_ID_EXP);
        writeByte(mockBuf, configSpaceSize, pcieCapPos + PCI_CAP_LIST_NEXT, 0);
        return;
    }
    writeByte(mockBuf, configSpaceSize, PCI_CAPABILITY_LIST, unrelatedCapPos);
    writeByte(mockBuf, configSpaceSize, unrelatedCapPos + PCI_CAP_LIST_ID, unrelatedCapId);
    writeByte(mockBuf, configSpaceSize, unrelatedCapPos + PCI_CAP_LIST_NEXT, capabilityChainSelfReferences ? unrelatedCapPos : 0);
}
} // namespace PciConfigExpMock

inline static int openMockPciConfigNodes(const char *pathname, int flags) {
    NEO::SysCalls::closeFuncCalled = 0;
    if (std::string(pathname) == mockRealPath2LevelsUpConfig) {
        return PciConfigExpMock::cardBusConfigFd;
    }
    return PciConfigExpMock::deviceConfigFd;
}

ssize_t readMockPciConfig(int fd, void *buf, size_t count) {
    if (PciConfigExpMock::readFailure || (PciConfigExpMock::cardBusReadFailure && fd == PciConfigExpMock::cardBusConfigFd)) {
        errno = ENOENT;
        return -1;
    }

    uint8_t *mockBuf = static_cast<uint8_t *>(buf);
    const uint32_t pcieCapPos = PciConfigExpMock::pcieCapPos;

    PciConfigExpMock::writeByte(mockBuf, count, PCI_STATUS, PciConfigExpMock::status);
    PciConfigExpMock::writeCapabilityChain(mockBuf, count);

    if (fd == PciConfigExpMock::cardBusConfigFd) {
        PciConfigExpMock::writeWord(mockBuf, count, pcieCapPos + PCI_CAP_FLAGS, 0x0052); // capability version 2, switch upstream port
        PciConfigExpMock::writeDword(mockBuf, count, pcieCapPos + PCI_EXP_LNKCAP2, PciConfigExpMock::cardBusLinkCaps2);
        return count;
    }

    PciConfigExpMock::writeWord(mockBuf, count, PCI_VENDOR_ID, PciConfigExpMock::vendorId);
    PciConfigExpMock::writeWord(mockBuf, count, PCI_DEVICE_ID, PciConfigExpMock::expectedDeviceId);
    PciConfigExpMock::writeWord(mockBuf, count, PCI_SUBSYSTEM_VENDOR_ID, PciConfigExpMock::expectedSubsystemVendorId);
    PciConfigExpMock::writeWord(mockBuf, count, PCI_SUBSYSTEM_DEVICE_ID, PciConfigExpMock::expectedSubsystemDeviceId);
    PciConfigExpMock::writeWord(mockBuf, count, pcieCapPos + PCI_CAP_FLAGS, PciConfigExpMock::capRegister);
    PciConfigExpMock::writeDword(mockBuf, count, pcieCapPos + PCI_EXP_LNKCAP2, PciConfigExpMock::linkCaps2);
    return count;
}

class ZesPciConfigExpFixtureXe : public ZesPciFixture {
  protected:
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> configOpenBackup{&NEO::SysCalls::sysCallsOpen, openMockPciConfigNodes};
    std::unique_ptr<SysmanProductHelper> pOriginalProductHelper;

    void SetUp() override {
        ZesPciFixture::SetUp();
        PciConfigExpMock::reset();

        auto pSysmanKmdInterface = new MockSysmanKmdInterfaceXe(pLinuxSysmanImp->getSysmanProductHelper());
        pSysmanKmdInterface->pSysfsAccess = std::make_unique<MockPciSysfsAccess>();
        pLinuxSysmanImp->pSysmanKmdInterface.reset(pSysmanKmdInterface);

        readBackup = readMockPciConfig;
    }

    void TearDown() override {
        PciConfigExpMock::reset();
        ZesPciFixture::TearDown();
    }

    void setUpstreamPortConnected(bool connected) {
        std::unique_ptr<MockSysmanProductHelper> pMockSysmanProductHelper = std::make_unique<MockSysmanProductHelper>();
        pMockSysmanProductHelper->isUpstreamPortConnectedResult = connected;
        std::unique_ptr<SysmanProductHelper> pProductHelper = std::move(pMockSysmanProductHelper);
        std::swap(pLinuxSysmanImp->pSysmanProductHelper, pProductHelper);
        pOriginalProductHelper = std::move(pProductHelper);
    }

    void setSurvivabilityMode() {
        device->isDeviceInSurvivabilityMode = true;
        pLinuxSysmanImp->pciBdfInfo = NEO::PhysicalDevicePciBusInfo(0u, expectedBus, expectedDevice, expectedFunction);
    }

    ze_result_t getPciConfigProperties(zes_intel_pci_config_exp_properties_t &configProps) {
        zes_pci_properties_t properties = {};
        configProps.stype = ZES_INTEL_STRUCTURE_TYPE_PCI_CONFIG_EXP_PROPERTIES;
        properties.pNext = &configProps;
        return zesDevicePciGetProperties(device, &properties);
    }
};

TEST_F(ZesPciConfigExpFixtureXe, GivenPciConfigExtensionWhenDeviceIsNotBehindAnOnCardSwitchThenEveryRegisterIsReadFromTheDeviceFunction) {
    setUpstreamPortConnected(false);

    zes_intel_pci_config_exp_properties_t configProps = {};
    EXPECT_EQ(ZE_RESULT_SUCCESS, getPciConfigProperties(configProps));

    EXPECT_EQ(PciConfigExpMock::expectedVendorId, configProps.vendorId);
    EXPECT_EQ(PciConfigExpMock::expectedDeviceId, configProps.deviceId);
    EXPECT_EQ(PciConfigExpMock::expectedSubsystemVendorId, configProps.subsystemVendorId);
    EXPECT_EQ(PciConfigExpMock::expectedSubsystemDeviceId, configProps.subsystemDeviceId);
    EXPECT_EQ(PciConfigExpMock::expectedCapabilityVersion, configProps.pcieCapabilityVersion);
    EXPECT_EQ(PciConfigExpMock::expectedDeviceSpeedsVector, configProps.supportedLinkSpeeds);
}

TEST_F(ZesPciConfigExpFixtureXe, GivenPciConfigExtensionWhenDeviceIsBehindAnOnCardSwitchThenLinkRegistersComeFromTheCardBusAndIdsFromTheDeviceFunction) {
    setUpstreamPortConnected(true);

    zes_intel_pci_config_exp_properties_t configProps = {};
    EXPECT_EQ(ZE_RESULT_SUCCESS, getPciConfigProperties(configProps));

    EXPECT_EQ(PciConfigExpMock::expectedVendorId, configProps.vendorId);
    EXPECT_EQ(PciConfigExpMock::expectedDeviceId, configProps.deviceId);
    EXPECT_EQ(PciConfigExpMock::expectedSubsystemVendorId, configProps.subsystemVendorId);
    EXPECT_EQ(PciConfigExpMock::expectedSubsystemDeviceId, configProps.subsystemDeviceId);
    EXPECT_EQ(PciConfigExpMock::expectedCapabilityVersion, configProps.pcieCapabilityVersion);
    EXPECT_EQ(PciConfigExpMock::expectedCardBusSpeedsVector, configProps.supportedLinkSpeeds);
}

TEST_F(ZesPciConfigExpFixtureXe, GivenPciConfigExtensionWhenCallerIsNotRootThenOnlyTheIdentificationRegistersAreReported) {
    setUpstreamPortConnected(false);
    VariableBackup<decltype(NEO::SysCalls::sysCallsRead)> readMockBackup(&NEO::SysCalls::sysCallsRead, readMockTruncatedToStandardHeader<readMockPciConfig>);

    zes_intel_pci_config_exp_properties_t configProps = {};
    EXPECT_EQ(ZE_RESULT_SUCCESS, getPciConfigProperties(configProps));

    EXPECT_EQ(PciConfigExpMock::expectedVendorId, configProps.vendorId);
    EXPECT_EQ(PciConfigExpMock::expectedDeviceId, configProps.deviceId);
    EXPECT_EQ(PciConfigExpMock::expectedSubsystemVendorId, configProps.subsystemVendorId);
    EXPECT_EQ(PciConfigExpMock::expectedSubsystemDeviceId, configProps.subsystemDeviceId);

    EXPECT_EQ(0u, configProps.pcieCapabilityVersion);
    EXPECT_EQ(0u, configProps.supportedLinkSpeeds);
}

TEST_F(ZesPciConfigExpFixtureXe, GivenPciConfigExtensionWhenCallerIsNotRootAndDeviceIsBehindAnOnCardSwitchThenTheIdentificationRegistersAreStillReported) {
    setUpstreamPortConnected(true);
    VariableBackup<decltype(NEO::SysCalls::sysCallsRead)> readMockBackup(&NEO::SysCalls::sysCallsRead, readMockTruncatedToStandardHeader<readMockPciConfig>);

    zes_intel_pci_config_exp_properties_t configProps = {};
    EXPECT_EQ(ZE_RESULT_SUCCESS, getPciConfigProperties(configProps));

    EXPECT_EQ(PciConfigExpMock::expectedVendorId, configProps.vendorId);
    EXPECT_EQ(0u, configProps.pcieCapabilityVersion);
    EXPECT_EQ(0u, configProps.supportedLinkSpeeds);
}

TEST_F(ZesPciConfigExpFixtureXe, GivenPciConfigExtensionWhenDeviceStoppedRespondingThenTheRegistersAreReportedAsRead) {
    setUpstreamPortConnected(false);
    PciConfigExpMock::vendorId = PCI_INVALID_VENDOR_ID;

    zes_intel_pci_config_exp_properties_t configProps = {};
    EXPECT_EQ(ZE_RESULT_SUCCESS, getPciConfigProperties(configProps));

    EXPECT_EQ(PCI_INVALID_VENDOR_ID, configProps.vendorId);
}

TEST_F(ZesPciConfigExpFixtureXe, GivenPciConfigExtensionWhenPciExpressCapabilityIsAbsentThenIdentificationRegistersAreStillReturned) {
    setUpstreamPortConnected(false);
    PciConfigExpMock::status = 0; // capability list not supported

    zes_intel_pci_config_exp_properties_t configProps = {};
    EXPECT_EQ(ZE_RESULT_SUCCESS, getPciConfigProperties(configProps));

    EXPECT_EQ(PciConfigExpMock::expectedVendorId, configProps.vendorId);
    EXPECT_EQ(PciConfigExpMock::expectedDeviceId, configProps.deviceId);
    EXPECT_EQ(0u, configProps.pcieCapabilityVersion);
    EXPECT_EQ(0u, configProps.supportedLinkSpeeds);
}

TEST_F(ZesPciConfigExpFixtureXe, GivenPciConfigExtensionWhenCapabilityChainEndsWithoutThePciExpressCapabilityThenIdentificationRegistersAreStillReturned) {
    setUpstreamPortConnected(false);
    PciConfigExpMock::pcieCapabilityInChain = false;

    zes_intel_pci_config_exp_properties_t configProps = {};
    EXPECT_EQ(ZE_RESULT_SUCCESS, getPciConfigProperties(configProps));

    EXPECT_EQ(PciConfigExpMock::expectedVendorId, configProps.vendorId);
    EXPECT_EQ(PciConfigExpMock::expectedDeviceId, configProps.deviceId);
    EXPECT_EQ(0u, configProps.pcieCapabilityVersion);
    EXPECT_EQ(0u, configProps.supportedLinkSpeeds);
}

TEST_F(ZesPciConfigExpFixtureXe, GivenPciConfigExtensionWhenCapabilityChainNeverTerminatesThenTheWalkIsBoundedAndIdentificationRegistersAreStillReturned) {
    setUpstreamPortConnected(false);
    PciConfigExpMock::pcieCapabilityInChain = false;
    PciConfigExpMock::capabilityChainSelfReferences = true;

    zes_intel_pci_config_exp_properties_t configProps = {};
    EXPECT_EQ(ZE_RESULT_SUCCESS, getPciConfigProperties(configProps));

    EXPECT_EQ(PciConfigExpMock::expectedVendorId, configProps.vendorId);
    EXPECT_EQ(PciConfigExpMock::expectedDeviceId, configProps.deviceId);
    EXPECT_EQ(0u, configProps.pcieCapabilityVersion);
    EXPECT_EQ(0u, configProps.supportedLinkSpeeds);
}

TEST_F(ZesPciConfigExpFixtureXe, GivenPciConfigExtensionWhenLinkCapabilities2RegisterFallsOutsideConfigSpaceThenIdentificationRegistersAreStillReturned) {
    setUpstreamPortConnected(false);
    PciConfigExpMock::pcieCapPos = PciConfigExpMock::truncatedPcieCapPos;

    zes_intel_pci_config_exp_properties_t configProps = {};
    EXPECT_EQ(ZE_RESULT_SUCCESS, getPciConfigProperties(configProps));

    EXPECT_EQ(PciConfigExpMock::expectedVendorId, configProps.vendorId);
    EXPECT_EQ(PciConfigExpMock::expectedDeviceId, configProps.deviceId);
    EXPECT_EQ(0u, configProps.pcieCapabilityVersion);
    EXPECT_EQ(0u, configProps.supportedLinkSpeeds);
}

TEST_F(ZesPciConfigExpFixtureXe, GivenPciConfigExtensionWhenCapabilityVersionPredatesLinkCapabilities2ThenTheVersionIsReturnedWithoutTheSpeedsVector) {
    setUpstreamPortConnected(false);
    PciConfigExpMock::capRegister = 0x0001; // capability version 1

    zes_intel_pci_config_exp_properties_t configProps = {};
    EXPECT_EQ(ZE_RESULT_SUCCESS, getPciConfigProperties(configProps));

    EXPECT_EQ(PciConfigExpMock::expectedVendorId, configProps.vendorId);
    EXPECT_EQ(1u, configProps.pcieCapabilityVersion);
    EXPECT_EQ(0u, configProps.supportedLinkSpeeds);
}

TEST_F(ZesPciConfigExpFixtureXe, GivenPciConfigExtensionWhensupportedLinkSpeedsIsNotImplementedThenItIsReportedAsZero) {
    setUpstreamPortConnected(false);
    PciConfigExpMock::linkCaps2 = 0;

    zes_intel_pci_config_exp_properties_t configProps = {};
    EXPECT_EQ(ZE_RESULT_SUCCESS, getPciConfigProperties(configProps));

    EXPECT_EQ(PciConfigExpMock::expectedVendorId, configProps.vendorId);
    EXPECT_EQ(PciConfigExpMock::expectedCapabilityVersion, configProps.pcieCapabilityVersion);
    EXPECT_EQ(0u, configProps.supportedLinkSpeeds);
}

TEST_F(ZesPciConfigExpFixtureXe, GivenPciConfigExtensionWhenDeviceIsIntegratedThenLinkCapabilitiesAreNotReadAndIdentificationRegistersAreReturned) {
    setUpstreamPortConnected(false);
    pSysmanDeviceImp->getRootDeviceEnvironment().getMutableHardwareInfo()->capabilityTable.isIntegratedDevice = true;

    zes_intel_pci_config_exp_properties_t configProps = {};
    EXPECT_EQ(ZE_RESULT_SUCCESS, getPciConfigProperties(configProps));

    EXPECT_EQ(PciConfigExpMock::expectedVendorId, configProps.vendorId);
    EXPECT_EQ(PciConfigExpMock::expectedDeviceId, configProps.deviceId);
    EXPECT_EQ(0u, configProps.pcieCapabilityVersion);
    EXPECT_EQ(0u, configProps.supportedLinkSpeeds);
}

TEST_F(ZesPciConfigExpFixtureXe, GivenPciConfigExtensionWhenCardBusConfigSpaceCannotBeReadThenNotAvailableIsReturned) {
    setUpstreamPortConnected(true);
    PciConfigExpMock::cardBusReadFailure = true;
    VariableBackup<int> mockErrno(&errno);

    zes_intel_pci_config_exp_properties_t configProps = {};
    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, getPciConfigProperties(configProps));
    EXPECT_EQ(0u, configProps.vendorId);
}

TEST_F(ZesPciConfigExpFixtureXe, GivenPciConfigExtensionWhenConfigSpaceCannotBeReadThenNotAvailableIsReturned) {
    setUpstreamPortConnected(false);
    PciConfigExpMock::readFailure = true;
    VariableBackup<int> mockErrno(&errno);

    zes_intel_pci_config_exp_properties_t configProps = {};
    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, getPciConfigProperties(configProps));
    EXPECT_EQ(0u, configProps.vendorId);
}

TEST_F(ZesPciConfigExpFixtureXe, GivenPciConfigExtensionChainedAfterAnotherExtensionThenBothStructuresAreFilled) {
    setUpstreamPortConnected(false);

    zes_intel_pci_config_exp_properties_t configProps = {};
    configProps.stype = ZES_INTEL_STRUCTURE_TYPE_PCI_CONFIG_EXP_PROPERTIES;
    zes_pci_link_speed_downgrade_ext_properties_t downgradeProps = {};
    downgradeProps.stype = ZES_STRUCTURE_TYPE_PCI_LINK_SPEED_DOWNGRADE_EXT_PROPERTIES;
    downgradeProps.pNext = &configProps;
    zes_pci_properties_t properties = {};
    properties.pNext = &downgradeProps;

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDevicePciGetProperties(device, &properties));

    EXPECT_TRUE(downgradeProps.pciLinkSpeedUpdateCapable);
    EXPECT_EQ(PciConfigExpMock::expectedVendorId, configProps.vendorId);
    EXPECT_EQ(PciConfigExpMock::expectedDeviceSpeedsVector, configProps.supportedLinkSpeeds);
}

TEST_F(ZesPciConfigExpFixtureXe, GivenPciConfigExtensionChainedAfterAnUnknownExtensionThenInvalidArgumentIsReturnedWithoutFillingIt) {
    setUpstreamPortConnected(false);

    zes_intel_pci_config_exp_properties_t configProps = {};
    configProps.stype = ZES_INTEL_STRUCTURE_TYPE_PCI_CONFIG_EXP_PROPERTIES;
    zes_base_properties_t unknownProps = {};
    unknownProps.stype = ZES_STRUCTURE_TYPE_FORCE_UINT32;
    unknownProps.pNext = &configProps;
    zes_pci_properties_t properties = {};
    properties.pNext = &unknownProps;

    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, zesDevicePciGetProperties(device, &properties));
    EXPECT_EQ(0u, configProps.vendorId);
}

TEST_F(ZesPciConfigExpFixtureXe, GivenPciConfigExtensionCannotBeReadWhenAnotherExtensionIsChainedAfterItThenTheFailureIsReportedWithoutFillingIt) {
    setUpstreamPortConnected(false);
    PciConfigExpMock::readFailure = true;
    VariableBackup<int> mockErrno(&errno);

    zes_pci_link_speed_downgrade_ext_properties_t downgradeProps = {};
    downgradeProps.stype = ZES_STRUCTURE_TYPE_PCI_LINK_SPEED_DOWNGRADE_EXT_PROPERTIES;
    zes_intel_pci_config_exp_properties_t configProps = {};
    configProps.stype = ZES_INTEL_STRUCTURE_TYPE_PCI_CONFIG_EXP_PROPERTIES;
    configProps.pNext = &downgradeProps;
    zes_pci_properties_t properties = {};
    properties.pNext = &configProps;

    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, zesDevicePciGetProperties(device, &properties));
    EXPECT_FALSE(downgradeProps.pciLinkSpeedUpdateCapable);
}

TEST_F(ZesPciFixture, GivenPciBdfInfoPointerIsNotInitializedWhenPciGetPropertiesIsInvokedThenErrorIsReturned) {
    device->isDeviceInSurvivabilityMode = true;

    auto pOriginalOsSysman = pSysmanDeviceImp->pOsSysman;

    auto pMockSysman = std::make_unique<PciLinuxSysmanImp>(pSysmanDeviceImp);
    pMockSysman->isPciBdfInfoPointerNull = true;

    pSysmanDeviceImp->pOsSysman = pMockSysman.get();

    auto testPciImp = std::make_unique<L0::Sysman::PciImp>(pMockSysman.get());

    zes_pci_properties_t properties = {};
    ze_result_t result = testPciImp->pciStaticProperties(&properties);
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, result);

    // Restore the original pOsSysman and clean up
    pSysmanDeviceImp->pOsSysman = pOriginalOsSysman;
    pMockSysman.reset();
}

TEST_F(ZesPciFixture, GivenPciBdfInfoValuesAreInvalidWhenPciGetPropertiesIsInvokedThenErrorIsReturned) {
    device->isDeviceInSurvivabilityMode = true;

    auto pOriginalOsSysman = pSysmanDeviceImp->pOsSysman;

    auto pMockSysman = std::make_unique<PciLinuxSysmanImp>(pSysmanDeviceImp);
    pMockSysman->isPciBdfInfoObjectInitialized = false;

    pSysmanDeviceImp->pOsSysman = pMockSysman.get();

    auto testPciImp = std::make_unique<L0::Sysman::PciImp>(pMockSysman.get());

    zes_pci_properties_t properties = {};
    ze_result_t result = testPciImp->pciStaticProperties(&properties);
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, result);

    // Restore the original pOsSysman and clean up
    pSysmanDeviceImp->pOsSysman = pOriginalOsSysman;
    pMockSysman.reset();
}

TEST_F(ZesPciFixture, GivenProperPciBdfInfoObjectWhenPciGetPropertiesIsInvokedThenCorrectBdfValuesAreReturned) {
    device->isDeviceInSurvivabilityMode = true;

    auto pOriginalOsSysman = pSysmanDeviceImp->pOsSysman;

    auto pMockSysman = std::make_unique<PciLinuxSysmanImp>(pSysmanDeviceImp);

    pSysmanDeviceImp->pOsSysman = pMockSysman.get();

    auto testPciImp = std::make_unique<L0::Sysman::PciImp>(pMockSysman.get());

    zes_pci_properties_t properties = {};
    ze_result_t result = testPciImp->pciStaticProperties(&properties);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(pMockSysman->testPciBus, properties.address.bus);
    EXPECT_EQ(pMockSysman->testPciDomain, properties.address.domain);
    EXPECT_EQ(pMockSysman->testPciFunction, properties.address.function);
    EXPECT_EQ(pMockSysman->testPciDevice, properties.address.device);

    // Restore the original pOsSysman and clean up
    pSysmanDeviceImp->pOsSysman = pOriginalOsSysman;
    pMockSysman.reset();
}

TEST_F(ZesPciConfigExpFixtureXe, GivenPciConfigExtensionWhenTopologyPathCannotBeResolvedFromThePciDevicePathThenLinkRegistersComeFromTheDeviceFunction) {
    setUpstreamPortConnected(true);
    pFsAccess->mockGetRealPathResult = ZE_RESULT_ERROR_NOT_AVAILABLE;

    zes_intel_pci_config_exp_properties_t configProps = {};
    EXPECT_EQ(ZE_RESULT_SUCCESS, getPciConfigProperties(configProps));

    EXPECT_EQ(PciConfigExpMock::expectedVendorId, configProps.vendorId);
    EXPECT_EQ(PciConfigExpMock::expectedDeviceSpeedsVector, configProps.supportedLinkSpeeds);
}

TEST_F(ZesPciConfigExpFixtureXe, GivenDeviceInSurvivabilityModeWhenPciConfigExtensionIsRequestedThenLinkRegistersStillComeFromTheCardBus) {
    setSurvivabilityMode();
    setUpstreamPortConnected(true);

    delete pPciImp->pOsPci;
    pPciImp->pOsPci = nullptr;

    zes_intel_pci_config_exp_properties_t configProps = {};
    zes_pci_properties_t properties = {};
    configProps.stype = ZES_INTEL_STRUCTURE_TYPE_PCI_CONFIG_EXP_PROPERTIES;
    properties.pNext = &configProps;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDevicePciGetProperties(device, &properties));

    EXPECT_EQ(expectedBus, properties.address.bus);
    EXPECT_EQ(expectedDevice, properties.address.device);
    EXPECT_EQ(expectedFunction, properties.address.function);
    EXPECT_EQ(PciConfigExpMock::expectedVendorId, configProps.vendorId);
    EXPECT_EQ(PciConfigExpMock::expectedDeviceId, configProps.deviceId);
    EXPECT_EQ(PciConfigExpMock::expectedSubsystemVendorId, configProps.subsystemVendorId);
    EXPECT_EQ(PciConfigExpMock::expectedSubsystemDeviceId, configProps.subsystemDeviceId);
    EXPECT_EQ(PciConfigExpMock::expectedCapabilityVersion, configProps.pcieCapabilityVersion);
    EXPECT_EQ(PciConfigExpMock::expectedCardBusSpeedsVector, configProps.supportedLinkSpeeds);
}

TEST_F(ZesPciConfigExpFixtureXe, GivenDeviceInSurvivabilityModeWhenPciConfigSpaceCannotBeReadThenTheFailureIsReported) {
    setSurvivabilityMode();
    setUpstreamPortConnected(false);
    PciConfigExpMock::readFailure = true;
    VariableBackup<int> mockErrno(&errno);

    zes_intel_pci_config_exp_properties_t configProps = {};
    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, getPciConfigProperties(configProps));
    EXPECT_EQ(0u, configProps.vendorId);
}

TEST_F(ZesPciConfigExpFixtureXe, GivenDeviceInSurvivabilityModeWhenAnUnsupportedExtensionIsChainedThenItIsLeftUnfilledAndSuccessIsReturned) {
    setSurvivabilityMode();

    zes_pci_link_speed_downgrade_ext_properties_t downgradeProps = {};
    downgradeProps.stype = ZES_STRUCTURE_TYPE_PCI_LINK_SPEED_DOWNGRADE_EXT_PROPERTIES;
    zes_pci_properties_t properties = {};
    properties.pNext = &downgradeProps;

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDevicePciGetProperties(device, &properties));

    EXPECT_EQ(expectedBus, properties.address.bus);
    EXPECT_EQ(&downgradeProps, properties.pNext);
    EXPECT_FALSE(downgradeProps.pciLinkSpeedUpdateCapable);
    EXPECT_EQ(0, downgradeProps.maxPciGenSupported);
}

} // namespace ult
} // namespace Sysman
} // namespace L0
