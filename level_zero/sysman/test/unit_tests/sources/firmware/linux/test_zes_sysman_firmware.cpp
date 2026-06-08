/*
 * Copyright (C) 2020-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/drm_neo.h"
#include "shared/test/common/helpers/default_hw_info.h"

#include "level_zero/sysman/source/driver/sysman_os_driver.h"
#include "level_zero/sysman/source/shared/linux/product_helper/sysman_product_helper_hw.h"
#include "level_zero/sysman/test/unit_tests/sources/firmware/linux/mock_zes_sysman_firmware.h"
#include "level_zero/sysman/test/unit_tests/sources/linux/mock_sysman_survivability.h"
#include "level_zero/sysman/test/unit_tests/sources/linux/mocks/mock_sysman_product_helper.h"
#include "level_zero/sysman/test/unit_tests/sources/linux/mtd/mock_mtd.h"
#include "level_zero/sysman/test/unit_tests/sources/shared/linux/kmd_interface/mock_sysman_kmd_interface_xe.h"

#include <algorithm>

namespace L0 {
namespace Sysman {
namespace ult {

class ZesSysmanFirmwareFixture : public SysmanDeviceFixture {

  protected:
    zes_firmware_handle_t hSysmanFirmware = {};
    std::unique_ptr<MockFirmwareInterface> pMockFwInterface;
    L0::Sysman::FirmwareUtil *pFwUtilInterfaceOld = nullptr;
    std::unique_ptr<MockFirmwareFsAccess> pFsAccess;
    L0::Sysman::FsAccessInterface *pFsAccessOriginal = nullptr;
    L0::Sysman::SysFsAccessInterface *pSysFsAccessOriginal = nullptr;
    std::unique_ptr<MockFirmwareSysfsAccess> pMockSysfsAccess;
    L0::Sysman::SysmanDevice *device = nullptr;

    void SetUp() override {
        SysmanDeviceFixture::SetUp();
        pFsAccessOriginal = pLinuxSysmanImp->pFsAccess;
        pFsAccess = std::make_unique<MockFirmwareFsAccess>();
        pLinuxSysmanImp->pFsAccess = pFsAccess.get();

        pSysFsAccessOriginal = pLinuxSysmanImp->pSysfsAccess;
        pMockSysfsAccess = std::make_unique<MockFirmwareSysfsAccess>();
        pLinuxSysmanImp->pSysfsAccess = pMockSysfsAccess.get();

        pFwUtilInterfaceOld = pLinuxSysmanImp->pFwUtilInterface;
        pMockFwInterface = std::make_unique<MockFirmwareInterface>();
        pLinuxSysmanImp->pFwUtilInterface = pMockFwInterface.get();

        for (const auto &handle : pSysmanDeviceImp->pFirmwareHandleContext->handleList) {
            delete handle;
        }
        pSysmanDeviceImp->pFirmwareHandleContext->handleList.clear();
        device = pSysmanDeviceImp;
    }
    void initFirmware() {
        uint32_t count = 0;
        ze_result_t result = zesDeviceEnumFirmwares(device->toHandle(), &count, nullptr);
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    }
    void TearDown() override {
        pLinuxSysmanImp->pFwUtilInterface = pFwUtilInterfaceOld;
        pLinuxSysmanImp->pFsAccess = pFsAccessOriginal;
        pLinuxSysmanImp->pSysfsAccess = pSysFsAccessOriginal;
        SysmanDeviceFixture::TearDown();
    }

    std::vector<zes_firmware_handle_t> getFirmwareHandles(uint32_t count) {
        std::vector<zes_firmware_handle_t> handles(count, nullptr);
        EXPECT_EQ(zesDeviceEnumFirmwares(device->toHandle(), &count, handles.data()), ZE_RESULT_SUCCESS);
        return handles;
    }
};

TEST_F(ZesSysmanFirmwareFixture, GivenValidFirmwareHandleWhenFlashingUnkownFirmwareThenFailureIsReturned) {
    for (const auto &handle : pSysmanDeviceImp->pFirmwareHandleContext->handleList) {
        delete handle;
    }
    pSysmanDeviceImp->pFirmwareHandleContext->handleList.clear();
    L0::Sysman::FirmwareImp *ptestFirmwareImp = new L0::Sysman::FirmwareImp(pSysmanDeviceImp->pFirmwareHandleContext->pOsSysman, mockUnsupportedFwTypes[0]);
    pSysmanDeviceImp->pFirmwareHandleContext->handleList.push_back(ptestFirmwareImp);

    pMockFwInterface->flashFirmwareResult = ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;

    uint8_t testImage[ZES_STRING_PROPERTY_SIZE] = {};
    memset(testImage, 0xA, ZES_STRING_PROPERTY_SIZE);
    auto handle = pSysmanDeviceImp->pFirmwareHandleContext->handleList[0]->toHandle();
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesFirmwareFlash(handle, (void *)testImage, ZES_STRING_PROPERTY_SIZE));

    pSysmanDeviceImp->pFirmwareHandleContext->handleList.pop_back();
    delete ptestFirmwareImp;
}

using SysmanSurvivabilityDeviceTest = ::testing::Test;
struct dirent mockSurvivabilityDevEntries[] = {
    {0, 0, 0, 0, "0000:03:00.0"},
    {0, 0, 0, 0, "0000:09:00.0"},
};

TEST_F(SysmanSurvivabilityDeviceTest, GivenSurvivabilityDeviceWhenFirmwareEnumerationApiIsCalledThenFirmwareHandlesAreReturned) {
    const uint32_t numEntries = sizeof(mockSurvivabilityDevEntries) / sizeof(mockSurvivabilityDevEntries[0]);
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpendir)> mockOpendir(&NEO::SysCalls::sysCallsOpendir, [](const char *name) -> DIR * {
        return reinterpret_cast<DIR *>(0xc001);
    });

    constexpr int deviceFileFd = 5;
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> openBackup{&NEO::SysCalls::sysCallsOpen, [](const char *pathname, int flags) -> int {
                                                                         if (std::string(pathname).find("/device") != std::string::npos) {
                                                                             return deviceFileFd;
                                                                         }
                                                                         return 0;
                                                                     }};
    VariableBackup<decltype(NEO::SysCalls::sysCallsRead)> readBackup(&NEO::SysCalls::sysCallsRead, [](int fd, void *buf, size_t count) -> ssize_t {
        if (fd == deviceFileFd) {
            char deviceIdStr[16];
            snprintf(deviceIdStr, sizeof(deviceIdStr), "0x%04x", getValidDeviceIdForProduct());
            std::strcpy(static_cast<char *>(buf), deviceIdStr);
            return strlen(deviceIdStr);
        }
        return -1;
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsClose)> closeBackup(&NEO::SysCalls::sysCallsClose, [](int fd) -> int {
        return 0;
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsReaddir)> mockReaddir(
        &NEO::SysCalls::sysCallsReaddir, [](DIR *dir) -> struct dirent * {
            static uint32_t entryIndex = 0u;
            if (entryIndex >= numEntries) {
                entryIndex = 0;
                return nullptr;
            }
            return &mockSurvivabilityDevEntries[entryIndex++];
        });

    VariableBackup<decltype(NEO::SysCalls::sysCallsClosedir)> mockClosedir(&NEO::SysCalls::sysCallsClosedir, [](DIR *dir) -> int {
        return 0;
    });

    std::unique_ptr<OsDriver> pOsDriverInterface = OsDriver::create();
    uint32_t driverCount = 0;
    ze_result_t result;
    auto sysmanDriverHandle = pOsDriverInterface->initSurvivabilityDevicesWithDriver(&result, &driverCount);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);
    EXPECT_TRUE(sysmanDriverHandle != nullptr);

    SysmanDriverHandleImp *pSysmanDriverHandle = static_cast<SysmanDriverHandleImp *>(sysmanDriverHandle);
    auto pSysmanDevice = pSysmanDriverHandle->sysmanDevices[0];
    auto pSysmanDeviceImp = static_cast<L0::Sysman::SysmanDeviceImp *>(pSysmanDevice);
    auto pOsSysman = pSysmanDeviceImp->pOsSysman;
    auto pLinuxSysmanImp = static_cast<PublicLinuxSysmanImp *>(pOsSysman);
    pLinuxSysmanImp->pFwUtilInterface = new MockFirmwareInterface();
    std::unique_ptr<SysmanProductHelper> pSysmanProductHelper = std::make_unique<MockSysmanProductHelper>();
    std::swap(pLinuxSysmanImp->pSysmanProductHelper, pSysmanProductHelper);
    pLinuxSysmanImp->pSysmanKmdInterface.reset(new SysmanKmdInterfaceI915Prelim(pLinuxSysmanImp->getSysmanProductHelper()));

    uint32_t count = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, SysmanDevice::firmwareGet(pSysmanDevice->toHandle(), &count, nullptr));
    EXPECT_EQ(count, mockFwHandlesCount);
    delete pLinuxSysmanImp->pFwUtilInterface;
    pLinuxSysmanImp->pFwUtilInterface = nullptr;
    delete sysmanDriverHandle;
    globalSysmanDriver = nullptr;
}

class SysmanFirmwareFdoFixtureXe : public SysmanDeviceFixture {

  protected:
    zes_firmware_handle_t hSysmanFirmware = {};
    L0::Sysman::SysmanDevice *device = nullptr;
    MockSysmanKmdInterfaceXe *pSysmanKmdInterface = nullptr;
    MockFirmwareSysfsAccess *pMockSysfsAccess = nullptr;
    MockFirmwareFsAccess *pMockFsAccess = nullptr;
    MockFirmwareProcfsAccess *pMockProcfsAccess = nullptr;

    void SetUp() override {
        SysmanDeviceFixture::SetUp();

        pSysmanKmdInterface = new MockSysmanKmdInterfaceXe(pLinuxSysmanImp->getSysmanProductHelper());
        pMockSysfsAccess = new MockFirmwareSysfsAccess();
        pSysmanKmdInterface->pSysfsAccess.reset(pMockSysfsAccess);

        pMockFsAccess = new MockFirmwareFsAccess();
        pSysmanKmdInterface->pFsAccess.reset(pMockFsAccess);

        pMockProcfsAccess = new MockFirmwareProcfsAccess();
        pSysmanKmdInterface->pProcfsAccess.reset(pMockProcfsAccess);

        pLinuxSysmanImp->pSysmanKmdInterface.reset(pSysmanKmdInterface);

        // Update LinuxSysmanImp's cached pointers to point to the new interfaces
        pLinuxSysmanImp->pFsAccess = pSysmanKmdInterface->getFsAccess();
        pLinuxSysmanImp->pSysfsAccess = pSysmanKmdInterface->getSysFsAccess();
        pLinuxSysmanImp->pProcfsAccess = pSysmanKmdInterface->getProcFsAccess();

        for (const auto &handle : pSysmanDeviceImp->pFirmwareHandleContext->handleList) {
            delete handle;
        }
        pSysmanDeviceImp->pFirmwareHandleContext->handleList.clear();
        device = pSysmanDeviceImp;
        device->isDeviceInSurvivabilityMode = true;
    }

    void initFirmware() {
        uint32_t count = 0;
        ze_result_t result = zesDeviceEnumFirmwares(device->toHandle(), &count, nullptr);
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    }

    std::vector<zes_firmware_handle_t> getFirmwareHandles(uint32_t count) {
        std::vector<zes_firmware_handle_t> handles(count, nullptr);
        EXPECT_EQ(zesDeviceEnumFirmwares(device->toHandle(), &count, handles.data()), ZE_RESULT_SUCCESS);
        return handles;
    }
};

// New fixture specifically for FDO blocking tests - creates all 4 firmware handles manually
class SysmanFirmwareBlockingFdoFixtureXe : public SysmanFirmwareFdoFixtureXe {
  protected:
    void SetUp() override {
        SysmanFirmwareFdoFixtureXe::SetUp();
        // Manually create all 4 firmware handles (GSC, OptionROM, PSC, Flash_Override)
        pMockSysfsAccess->mockFdoModeValue = "disabled";

        // Create firmware handles for all types
        std::vector<std::string> fwTypes = {"GSC", "OptionROM", "PSC", "Flash_Override"};
        for (const auto &fwType : fwTypes) {
            auto *pFirmwareImp = new L0::Sysman::FirmwareImp(pSysmanDeviceImp->pFirmwareHandleContext->pOsSysman, fwType);
            pSysmanDeviceImp->pFirmwareHandleContext->handleList.push_back(pFirmwareImp);
        }
    }
};

TEST_F(SysmanFirmwareBlockingFdoFixtureXe, GivenDeviceInFdoModeWhenFlashingUsingNormalFirmwareHandleThenNotAvailableIsReturned) {

    EXPECT_EQ(4u, pSysmanDeviceImp->pFirmwareHandleContext->handleList.size());

    pMockSysfsAccess->mockFdoModeValue = "enabled";

    uint8_t testImage[ZES_STRING_PROPERTY_SIZE] = {};
    memset(testImage, 0xA, ZES_STRING_PROPERTY_SIZE);

    // Iterate through all handles and verify only non-FDO firmware returns NOT_AVAILABLE from blocking check
    for (auto *pFirmwareHandle : pSysmanDeviceImp->pFirmwareHandleContext->handleList) {
        auto handle = pFirmwareHandle->toHandle();
        zes_firmware_properties_t properties = {};
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesFirmwareGetProperties(handle, &properties));

        std::string fwName(properties.name);
        ze_result_t result = zesFirmwareFlash(handle, (void *)testImage, ZES_STRING_PROPERTY_SIZE);

        if (fwName != "Flash_Override") {
            EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, result);
        } else {
            EXPECT_NE(ZE_RESULT_ERROR_NOT_AVAILABLE, result);
        }
    }
}

TEST_F(SysmanFirmwareBlockingFdoFixtureXe, GivenDeviceInFdoModeWhenGettingFirmwareFlashProgressThenNotAvailableIsReturned) {
    EXPECT_EQ(4u, pSysmanDeviceImp->pFirmwareHandleContext->handleList.size());
    pMockSysfsAccess->mockFdoModeValue = "enabled";

    for (auto *pFirmwareHandle : pSysmanDeviceImp->pFirmwareHandleContext->handleList) {
        auto handle = pFirmwareHandle->toHandle();
        uint32_t completionPercent = 0;
        ze_result_t result = zesFirmwareGetFlashProgress(handle, &completionPercent);
        EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, result);
    }
}

TEST_F(SysmanFirmwareBlockingFdoFixtureXe, GivenDeviceInNormalConditionsWhenGettingFlashProgressWithFlashOverrideHandleThenNotAvailableIsReturned) {
    pMockSysfsAccess->mockFdoModeValue = "disabled";

    for (auto *pFirmwareHandle : pSysmanDeviceImp->pFirmwareHandleContext->handleList) {
        zes_firmware_properties_t properties = {};
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesFirmwareGetProperties(pFirmwareHandle->toHandle(), &properties));

        if (std::string(properties.name) == "Flash_Override") {
            uint32_t completionPercent = 0;
            ze_result_t result = zesFirmwareGetFlashProgress(pFirmwareHandle->toHandle(), &completionPercent);
            EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, result);
            break;
        }
    }
}

TEST_F(SysmanFirmwareFdoFixtureXe, GivenDeviceInFdoModeWhenGettingFirmwarePropertiesThenCorrectVersionIsReturned) {
    pMockSysfsAccess->mockFdoModeValue = "enabled";
    initFirmware();
    auto handles = getFirmwareHandles(mockFwHandlesCountFdo);
    ASSERT_NE(nullptr, handles[0]);
    zes_firmware_properties_t properties = {};
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesFirmwareGetProperties(handles[0], &properties));
    EXPECT_STREQ(mockSupportedFirmwareTypesFdo[0].c_str(), properties.name);
    EXPECT_STREQ(mockUnknownVersion.c_str(), properties.version);
}

TEST_F(SysmanFirmwareFdoFixtureXe, GivenValidParametersWhenFlashingExtendedFirmwareThenSuccessIsReturned) {
    pMockSysfsAccess->mockFdoModeValue = "enabled";
    initFirmware();
    auto handles = getFirmwareHandles(mockFwHandlesCountFdo);
    ASSERT_NE(nullptr, handles[0]);

    // Enable large firmware regions to use mtd4 (large fd) for 8MB firmware
    pMockFsAccess->regionMode = MockFirmwareFsAccess::MtdRegionMode::singleRegion;

    // Mock successful system calls for MTD operations
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, &mockOpenSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsClose)> mockClose(&NEO::SysCalls::sysCallsClose, &mockCloseSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsLseek)> mockLseek(&NEO::SysCalls::sysCallsLseek, &mockLseekSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsRead)> mockRead(&NEO::SysCalls::sysCallsRead, &mockReadSpiDescriptorSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsIoctl)> mockIoctl(&NEO::SysCalls::sysCallsIoctl, &mockIoctlEraseSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsWrite)> mockWrite(&NEO::SysCalls::sysCallsWrite, &mockWriteSuccess);

    // Set up mock PCI BDF info
    pLinuxSysmanImp->pciBdfInfo.pciBus = mockPciBus;
    pLinuxSysmanImp->pciBdfInfo.pciDevice = mockPciDevice;
    pLinuxSysmanImp->pciBdfInfo.pciFunction = mockPciFunction;

    // Create test firmware data
    std::vector<uint8_t> firmwareData(0x800000, 0xAA);

    ze_result_t result = zesFirmwareFlash(handles[0], firmwareData.data(), static_cast<uint32_t>(firmwareData.size()));
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

TEST_F(SysmanFirmwareFdoFixtureXe, GivenInvalidPciBdfInfoWhenFlashingExtendedFirmwareThenErrorIsReturned) {
    pMockSysfsAccess->mockFdoModeValue = "enabled";
    initFirmware();
    auto handles = getFirmwareHandles(mockFwHandlesCountFdo);
    ASSERT_NE(nullptr, handles[0]);

    // Set invalid PCI BDF info (all zeros or invalid values)
    pLinuxSysmanImp->pciBdfInfo.pciBus = 0;
    pLinuxSysmanImp->pciBdfInfo.pciDevice = 0;
    pLinuxSysmanImp->pciBdfInfo.pciFunction = 0;

    std::vector<uint8_t> firmwareData(1024, 0xAA);
    ze_result_t result = zesFirmwareFlash(handles[0], firmwareData.data(), static_cast<uint32_t>(firmwareData.size()));
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, result);
}

TEST_F(SysmanFirmwareFdoFixtureXe, GivenProcMtdReadFailsWhenFlashingExtendedFirmwareThenErrorIsReturned) {
    pMockSysfsAccess->mockFdoModeValue = "enabled";
    initFirmware();
    auto handles = getFirmwareHandles(mockFwHandlesCountFdo);
    ASSERT_NE(nullptr, handles[0]);

    // Mock fs access to fail reading /proc/mtd
    pMockFsAccess->readResult = ZE_RESULT_ERROR_UNKNOWN;

    pLinuxSysmanImp->pciBdfInfo.pciBus = mockPciBus;
    pLinuxSysmanImp->pciBdfInfo.pciDevice = mockPciDevice;
    pLinuxSysmanImp->pciBdfInfo.pciFunction = mockPciFunction;

    std::vector<uint8_t> firmwareData(1024, 0xAA);
    ze_result_t result = zesFirmwareFlash(handles[0], firmwareData.data(), static_cast<uint32_t>(firmwareData.size()));
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, result);
}

TEST_F(SysmanFirmwareFdoFixtureXe, GivenEmptyMtdLinesWhenFlashingExtendedFirmwareThenErrorIsReturned) {
    pMockSysfsAccess->mockFdoModeValue = "enabled";
    initFirmware();
    auto handles = getFirmwareHandles(mockFwHandlesCountFdo);
    ASSERT_NE(nullptr, handles[0]);

    // Mock fs access to return empty MTD lines
    pMockFsAccess->regionMode = MockFirmwareFsAccess::MtdRegionMode::noRegions;

    pLinuxSysmanImp->pciBdfInfo.pciBus = mockPciBus;
    pLinuxSysmanImp->pciBdfInfo.pciDevice = mockPciDevice;
    pLinuxSysmanImp->pciBdfInfo.pciFunction = mockPciFunction;

    std::vector<uint8_t> firmwareData(1024, 0xAA);
    ze_result_t result = zesFirmwareFlash(handles[0], firmwareData.data(), static_cast<uint32_t>(firmwareData.size()));
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, result);
}

TEST_F(SysmanFirmwareFdoFixtureXe, GivenEmptyMtdFileWhenFlashingExtendedFirmwareThenCoversEmptyCondition) {
    pMockSysfsAccess->mockFdoModeValue = "enabled";
    initFirmware();
    auto handles = getFirmwareHandles(mockFwHandlesCountFdo);
    ASSERT_NE(nullptr, handles[0]);

    // Mock fs access to return completely empty MTD file (no lines at all)
    // This tests the mtdLines.empty() condition in: if (result != ZE_RESULT_SUCCESS || mtdLines.empty())
    pMockFsAccess->regionMode = MockFirmwareFsAccess::MtdRegionMode::emptyMtdFile;

    pLinuxSysmanImp->pciBdfInfo.pciBus = mockPciBus;
    pLinuxSysmanImp->pciBdfInfo.pciDevice = mockPciDevice;
    pLinuxSysmanImp->pciBdfInfo.pciFunction = mockPciFunction;

    std::vector<uint8_t> firmwareData(1024, 0xAA);
    ze_result_t result = zesFirmwareFlash(handles[0], firmwareData.data(), static_cast<uint32_t>(firmwareData.size()));
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, result); // Should fail due to empty MTD file
}

TEST_F(SysmanFirmwareFdoFixtureXe, GivenNoMatchingMtdEntriesWhenFlashingExtendedFirmwareThenErrorIsReturned) {
    pMockSysfsAccess->mockFdoModeValue = "enabled";
    initFirmware();
    auto handles = getFirmwareHandles(mockFwHandlesCountFdo);
    ASSERT_NE(nullptr, handles[0]);

    // Mock fs access to return non-matching MTD entries
    pMockFsAccess->regionMode = MockFirmwareFsAccess::MtdRegionMode::bdfMisMatch;

    pLinuxSysmanImp->pciBdfInfo.pciBus = mockPciBus;
    pLinuxSysmanImp->pciBdfInfo.pciDevice = mockPciDevice;
    pLinuxSysmanImp->pciBdfInfo.pciFunction = mockPciFunction;

    std::vector<uint8_t> firmwareData(1024, 0xAA);
    ze_result_t result = zesFirmwareFlash(handles[0], firmwareData.data(), static_cast<uint32_t>(firmwareData.size()));
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, result);
}

TEST_F(SysmanFirmwareFdoFixtureXe, GivenNoDescriptorDeviceWhenFlashingExtendedFirmwareThenErrorIsReturned) {
    pMockSysfsAccess->mockFdoModeValue = "enabled";
    initFirmware();
    auto handles = getFirmwareHandles(mockFwHandlesCountFdo);
    ASSERT_NE(nullptr, handles[0]);

    // Mock successful system calls (although they won't be reached due to early failure)
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, &mockOpenSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsClose)> mockClose(&NEO::SysCalls::sysCallsClose, &mockCloseSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsLseek)> mockLseek(&NEO::SysCalls::sysCallsLseek, &mockLseekSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsRead)> mockRead(&NEO::SysCalls::sysCallsRead, &mockReadSpiDescriptorSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsIoctl)> mockIoctl(&NEO::SysCalls::sysCallsIoctl, &mockIoctlEraseSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsWrite)> mockWrite(&NEO::SysCalls::sysCallsWrite, &mockWriteSuccess);

    // Mock fs access to not have DESCRIPTOR device
    pMockFsAccess->regionMode = MockFirmwareFsAccess::MtdRegionMode::noDescriptorRegion;

    pLinuxSysmanImp->pciBdfInfo.pciBus = mockPciBus;
    pLinuxSysmanImp->pciBdfInfo.pciDevice = mockPciDevice;
    pLinuxSysmanImp->pciBdfInfo.pciFunction = mockPciFunction;

    std::vector<uint8_t> firmwareData(1024, 0xAA);
    ze_result_t result = zesFirmwareFlash(handles[0], firmwareData.data(), static_cast<uint32_t>(firmwareData.size()));
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, result);
}

TEST_F(SysmanFirmwareFdoFixtureXe, GivenMtdDeviceCreateFailsWhenFlashingExtendedFirmwareThenErrorIsReturned) {
    pMockSysfsAccess->mockFdoModeValue = "enabled";
    initFirmware();
    auto handles = getFirmwareHandles(mockFwHandlesCountFdo);
    ASSERT_NE(nullptr, handles[0]);

    // Mock system calls - fail the open call to simulate MTD device creation failure
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, [](const char *pathname, int flags) -> int {
        errno = ENOENT;
        return -1; // Fail open for MTD device
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsClose)> mockClose(&NEO::SysCalls::sysCallsClose, &mockCloseSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsLseek)> mockLseek(&NEO::SysCalls::sysCallsLseek, &mockLseekSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsRead)> mockRead(&NEO::SysCalls::sysCallsRead, &mockReadSpiDescriptorSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsIoctl)> mockIoctl(&NEO::SysCalls::sysCallsIoctl, &mockIoctlEraseSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsWrite)> mockWrite(&NEO::SysCalls::sysCallsWrite, &mockWriteSuccess);

    pLinuxSysmanImp->pciBdfInfo.pciBus = mockPciBus;
    pLinuxSysmanImp->pciBdfInfo.pciDevice = mockPciDevice;
    pLinuxSysmanImp->pciBdfInfo.pciFunction = mockPciFunction;

    std::vector<uint8_t> firmwareData(1024, 0xAA);
    ze_result_t result = zesFirmwareFlash(handles[0], firmwareData.data(), static_cast<uint32_t>(firmwareData.size()));
    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, result);
}

TEST_F(SysmanFirmwareFdoFixtureXe, GivenGetDeviceInfoSkipsRegionsWhenFlashingExtendedFirmwareThenSuccessIsReturned) {
    pMockSysfsAccess->mockFdoModeValue = "enabled";
    initFirmware();
    auto handles = getFirmwareHandles(mockFwHandlesCountFdo);
    ASSERT_NE(nullptr, handles[0]);

    // Mock successful system calls except for the read that gets device info (lseek will fail)
    // This will cause getDeviceInfo to skip regions but still return SUCCESS
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, &mockOpenSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsClose)> mockClose(&NEO::SysCalls::sysCallsClose, &mockCloseSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsLseek)> mockLseek(&NEO::SysCalls::sysCallsLseek, [](int fd, off_t offset, int whence) -> off_t {
        return -1; // Fail lseek to simulate device info read failure
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsRead)> mockRead(&NEO::SysCalls::sysCallsRead, &mockReadSpiDescriptorSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsIoctl)> mockIoctl(&NEO::SysCalls::sysCallsIoctl, &mockIoctlEraseSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsWrite)> mockWrite(&NEO::SysCalls::sysCallsWrite, &mockWriteSuccess);

    pLinuxSysmanImp->pciBdfInfo.pciBus = mockPciBus;
    pLinuxSysmanImp->pciBdfInfo.pciDevice = mockPciDevice;
    pLinuxSysmanImp->pciBdfInfo.pciFunction = mockPciFunction;

    std::vector<uint8_t> firmwareData(1024, 0xAA);
    ze_result_t result = zesFirmwareFlash(handles[0], firmwareData.data(), static_cast<uint32_t>(firmwareData.size()));
    EXPECT_EQ(ZE_RESULT_SUCCESS, result); // Driver continues with empty region map
}

TEST_F(SysmanFirmwareFdoFixtureXe, GivenEraseFailsWhenFlashingExtendedFirmwareThenErrorIsReturned) {
    pMockSysfsAccess->mockFdoModeValue = "enabled";
    initFirmware();
    auto handles = getFirmwareHandles(mockFwHandlesCountFdo);
    ASSERT_NE(nullptr, handles[0]);

    // Enable large firmware regions to use mtd4 (large fd)
    pMockFsAccess->regionMode = MockFirmwareFsAccess::MtdRegionMode::singleRegion;

    // Mock successful system calls except erase
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, &mockOpenSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsClose)> mockClose(&NEO::SysCalls::sysCallsClose, &mockCloseSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsLseek)> mockLseek(&NEO::SysCalls::sysCallsLseek, &mockLseekSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsRead)> mockRead(&NEO::SysCalls::sysCallsRead, &mockReadSpiDescriptorSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsIoctl)> mockIoctl(&NEO::SysCalls::sysCallsIoctl, [](int fd, unsigned long request, void *arg) -> int {
        if (request == memEraseCmd) {
            errno = ENOENT;
            return -1; // Fail only the erase operation
        }
        return 0; // Success for other ioctl calls
    });

    pLinuxSysmanImp->pciBdfInfo.pciBus = mockPciBus;
    pLinuxSysmanImp->pciBdfInfo.pciDevice = mockPciDevice;
    pLinuxSysmanImp->pciBdfInfo.pciFunction = mockPciFunction;

    std::vector<uint8_t> firmwareData(0x800000, 0xAA); // Full size needed for erase operation testing
    ze_result_t result = zesFirmwareFlash(handles[0], firmwareData.data(), static_cast<uint32_t>(firmwareData.size()));
    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, result);
}

TEST_F(SysmanFirmwareFdoFixtureXe, GivenWriteFailsWhenFlashingExtendedFirmwareThenErrorIsReturned) {
    pMockSysfsAccess->mockFdoModeValue = "enabled";
    initFirmware();
    auto handles = getFirmwareHandles(mockFwHandlesCountFdo);
    ASSERT_NE(nullptr, handles[0]);

    // Enable large firmware regions to use mtd4 (large fd)
    pMockFsAccess->regionMode = MockFirmwareFsAccess::MtdRegionMode::singleRegion;

    // Mock successful system calls except write
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, &mockOpenSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsClose)> mockClose(&NEO::SysCalls::sysCallsClose, &mockCloseSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsLseek)> mockLseek(&NEO::SysCalls::sysCallsLseek, &mockLseekSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsRead)> mockRead(&NEO::SysCalls::sysCallsRead, &mockReadSpiDescriptorSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsIoctl)> mockIoctl(&NEO::SysCalls::sysCallsIoctl, &mockIoctlEraseSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsWrite)> mockWrite(&NEO::SysCalls::sysCallsWrite, [](int fd, const void *buf, size_t count) -> ssize_t {
        errno = ENOENT;
        return -1; // Fail write operation
    });

    pLinuxSysmanImp->pciBdfInfo.pciBus = mockPciBus;
    pLinuxSysmanImp->pciBdfInfo.pciDevice = mockPciDevice;
    pLinuxSysmanImp->pciBdfInfo.pciFunction = mockPciFunction;

    std::vector<uint8_t> firmwareData(0x800000, 0xAA); // Full size needed for write operation testing
    ze_result_t result = zesFirmwareFlash(handles[0], firmwareData.data(), static_cast<uint32_t>(firmwareData.size()));
    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, result);
}

TEST_F(SysmanFirmwareFdoFixtureXe, GivenMultipleRegionsWhenFlashingExtendedFirmwareThenSuccessIsReturned) {
    pMockSysfsAccess->mockFdoModeValue = "enabled";
    initFirmware();
    auto handles = getFirmwareHandles(mockFwHandlesCountFdo);
    ASSERT_NE(nullptr, handles[0]);

    // Mock successful system calls for MTD operations
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, &mockOpenSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsClose)> mockClose(&NEO::SysCalls::sysCallsClose, &mockCloseSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsLseek)> mockLseek(&NEO::SysCalls::sysCallsLseek, &mockLseekSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsRead)> mockRead(&NEO::SysCalls::sysCallsRead, &mockReadSpiDescriptorSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsIoctl)> mockIoctl(&NEO::SysCalls::sysCallsIoctl, &mockIoctlEraseSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsWrite)> mockWrite(&NEO::SysCalls::sysCallsWrite, &mockWriteSuccess);

    // Mock sysfs access to return multiple regions with large firmware support
    pMockFsAccess->regionMode = MockFirmwareFsAccess::MtdRegionMode::multipleRegions;

    pLinuxSysmanImp->pciBdfInfo.pciBus = mockPciBus;
    pLinuxSysmanImp->pciBdfInfo.pciDevice = mockPciDevice;
    pLinuxSysmanImp->pciBdfInfo.pciFunction = mockPciFunction;

    // Create test firmware data for large regions
    std::vector<uint8_t> firmwareData(0x800000, 0xAA); // Full 8MB for multiple regions test

    ze_result_t result = zesFirmwareFlash(handles[0], firmwareData.data(), static_cast<uint32_t>(firmwareData.size()));
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    // Verify successful operation (we can't easily verify call counts with the current mock setup)
}

TEST_F(SysmanFirmwareFdoFixtureXe, GivenInsufficientFirmwareDataWhenFlashingExtendedFirmwareThenInvalidSizeErrorIsReturned) {
    pMockSysfsAccess->mockFdoModeValue = "enabled";
    initFirmware();
    auto handles = getFirmwareHandles(mockFwHandlesCountFdo);
    ASSERT_NE(nullptr, handles[0]);

    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, &mockOpenSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsClose)> mockClose(&NEO::SysCalls::sysCallsClose, &mockCloseSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsLseek)> mockLseek(&NEO::SysCalls::sysCallsLseek, &mockLseekSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsRead)> mockRead(&NEO::SysCalls::sysCallsRead, &mockReadSpiDescriptorSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsIoctl)> mockIoctl(&NEO::SysCalls::sysCallsIoctl, &mockIoctlEraseSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsWrite)> mockWrite(&NEO::SysCalls::sysCallsWrite, &mockWriteSuccess);

    pLinuxSysmanImp->pciBdfInfo.pciBus = mockPciBus;
    pLinuxSysmanImp->pciBdfInfo.pciDevice = mockPciDevice;
    pLinuxSysmanImp->pciBdfInfo.pciFunction = mockPciFunction;

    std::vector<uint8_t> firmwareData(100, 0xAA);

    ze_result_t result = zesFirmwareFlash(handles[0], firmwareData.data(), static_cast<uint32_t>(firmwareData.size()));
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_SIZE, result);
}

TEST_F(SysmanFirmwareFdoFixtureXe, GivenInsufficientDataForLaterRegionsWhenFlashingThenInvalidSizeErrorIsReturned) {
    pMockSysfsAccess->mockFdoModeValue = "enabled";
    initFirmware();
    auto handles = getFirmwareHandles(mockFwHandlesCountFdo);
    ASSERT_NE(nullptr, handles[0]);

    pMockFsAccess->regionMode = MockFirmwareFsAccess::MtdRegionMode::multipleRegions;

    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, &mockOpenSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsClose)> mockClose(&NEO::SysCalls::sysCallsClose, &mockCloseSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsLseek)> mockLseek(&NEO::SysCalls::sysCallsLseek, &mockLseekSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsRead)> mockRead(&NEO::SysCalls::sysCallsRead, &mockReadSpiDescriptorSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsIoctl)> mockIoctl(&NEO::SysCalls::sysCallsIoctl, &mockIoctlEraseSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsWrite)> mockWrite(&NEO::SysCalls::sysCallsWrite, &mockWriteSuccess);

    pLinuxSysmanImp->pciBdfInfo.pciBus = mockPciBus;
    pLinuxSysmanImp->pciBdfInfo.pciDevice = mockPciDevice;
    pLinuxSysmanImp->pciBdfInfo.pciFunction = mockPciFunction;

    std::vector<uint8_t> firmwareData(0x800, 0xAA);

    ze_result_t result = zesFirmwareFlash(handles[0], firmwareData.data(), static_cast<uint32_t>(firmwareData.size()));
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_SIZE, result);
}

TEST_F(SysmanFirmwareFdoFixtureXe, GivenRegionOffsetExceedsImageSizeWhenFlashingThenInvalidSizeErrorIsReturned) {
    pMockSysfsAccess->mockFdoModeValue = "enabled";
    initFirmware();
    auto handles = getFirmwareHandles(mockFwHandlesCountFdo);
    ASSERT_NE(nullptr, handles[0]);

    static thread_local off_t lastSeekOffset = 0;
    auto customLseek = [](int fd, off_t offset, int whence) -> off_t {
        if ((fd >= 3 && fd <= 7) && whence == SEEK_SET) {
            lastSeekOffset = offset;
            return offset;
        }
        return -1;
    };

    auto customRead = [](int fd, void *buf, size_t count) -> ssize_t {
        if (fd == 3 && count == sizeof(MtdSysman::SpiDescRegionBar)) {
            MtdSysman::SpiDescRegionBar *regionBar = reinterpret_cast<MtdSysman::SpiDescRegionBar *>(buf);
            regionBar->reserved0 = 0;
            regionBar->reserved1 = 0;

            if (lastSeekOffset == 0x40) {
                regionBar->base = 0x0 >> 12;
                regionBar->limit = 0x00000fff >> 12;
            } else if (lastSeekOffset == 0x48) {
                regionBar->base = 0x00083000 >> 12;
                regionBar->limit = 0x004a4fff >> 12;
            } else {
                regionBar->base = 0;
                regionBar->limit = 0;
            }
            return count;
        }
        return -1;
    };

    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, &mockOpenSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsClose)> mockClose(&NEO::SysCalls::sysCallsClose, &mockCloseSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsLseek)> mockLseek(&NEO::SysCalls::sysCallsLseek, customLseek);
    VariableBackup<decltype(NEO::SysCalls::sysCallsRead)> mockRead(&NEO::SysCalls::sysCallsRead, customRead);
    VariableBackup<decltype(NEO::SysCalls::sysCallsIoctl)> mockIoctl(&NEO::SysCalls::sysCallsIoctl, &mockIoctlEraseSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsWrite)> mockWrite(&NEO::SysCalls::sysCallsWrite, &mockWriteSuccess);

    pLinuxSysmanImp->pciBdfInfo.pciBus = mockPciBus;
    pLinuxSysmanImp->pciBdfInfo.pciDevice = mockPciDevice;
    pLinuxSysmanImp->pciBdfInfo.pciFunction = mockPciFunction;

    std::vector<uint8_t> firmwareData(0x1000, 0xAA);

    ze_result_t result = zesFirmwareFlash(handles[0], firmwareData.data(), static_cast<uint32_t>(firmwareData.size()));
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_SIZE, result);
}

TEST_F(SysmanFirmwareFdoFixtureXe, GivenMtdNumberWithoutColonWhenFlashingExtendedFirmwareThenCoversNoColonCondition) {
    pMockSysfsAccess->mockFdoModeValue = "enabled";
    initFirmware();
    auto handles = getFirmwareHandles(mockFwHandlesCountFdo);
    ASSERT_NE(nullptr, handles[0]);

    pMockFsAccess->regionMode = MockFirmwareFsAccess::MtdRegionMode::mtdNumberNoColon;

    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, &mockOpenSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsClose)> mockClose(&NEO::SysCalls::sysCallsClose, &mockCloseSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsLseek)> mockLseek(&NEO::SysCalls::sysCallsLseek, &mockLseekSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsRead)> mockRead(&NEO::SysCalls::sysCallsRead, &mockReadSpiDescriptorSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsIoctl)> mockIoctl(&NEO::SysCalls::sysCallsIoctl, &mockIoctlEraseSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsWrite)> mockWrite(&NEO::SysCalls::sysCallsWrite, &mockWriteSuccess);

    pLinuxSysmanImp->pciBdfInfo.pciBus = mockPciBus;
    pLinuxSysmanImp->pciBdfInfo.pciDevice = mockPciDevice;
    pLinuxSysmanImp->pciBdfInfo.pciFunction = mockPciFunction;

    std::vector<uint8_t> firmwareData(0x800000, 0xAA);

    ze_result_t result = zesFirmwareFlash(handles[0], firmwareData.data(), static_cast<uint32_t>(firmwareData.size()));
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

TEST_F(SysmanFirmwareFdoFixtureXe, GivenShortDeviceNameWhenFlashingExtendedFirmwareThenNoDescriptorFoundAndErrorReturned) {
    pMockSysfsAccess->mockFdoModeValue = "enabled";
    initFirmware();
    auto handles = getFirmwareHandles(mockFwHandlesCountFdo);
    ASSERT_NE(nullptr, handles[0]);

    // Use test mode with device names too short for region extraction
    pMockFsAccess->regionMode = MockFirmwareFsAccess::MtdRegionMode::shortDeviceName;

    // Set up mock PCI BDF info
    pLinuxSysmanImp->pciBdfInfo.pciBus = mockPciBus;
    pLinuxSysmanImp->pciBdfInfo.pciDevice = mockPciDevice;
    pLinuxSysmanImp->pciBdfInfo.pciFunction = mockPciFunction;

    // Create small test firmware data - no need for large image since we'll fail early
    std::vector<uint8_t> firmwareData(1024, 0xAA);

    ze_result_t result = zesFirmwareFlash(handles[0], firmwareData.data(), static_cast<uint32_t>(firmwareData.size()));
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, result); // Should fail as no valid DESCRIPTOR device found
}

TEST_F(SysmanFirmwareFdoFixtureXe, GivenMalformedQuotesInMtdNamesWhenFlashingExtendedFirmwareThenCoversBothQuoteConditions) {
    pMockSysfsAccess->mockFdoModeValue = "enabled";
    initFirmware();
    auto handles = getFirmwareHandles(mockFwHandlesCountFdo);
    ASSERT_NE(nullptr, handles[0]);

    pMockFsAccess->regionMode = MockFirmwareFsAccess::MtdRegionMode::malformedQuotes;

    // Mock successful system calls for MTD operations
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, &mockOpenSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsClose)> mockClose(&NEO::SysCalls::sysCallsClose, &mockCloseSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsLseek)> mockLseek(&NEO::SysCalls::sysCallsLseek, &mockLseekSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsRead)> mockRead(&NEO::SysCalls::sysCallsRead, &mockReadSpiDescriptorSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsIoctl)> mockIoctl(&NEO::SysCalls::sysCallsIoctl, &mockIoctlEraseSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsWrite)> mockWrite(&NEO::SysCalls::sysCallsWrite, &mockWriteSuccess);

    pLinuxSysmanImp->pciBdfInfo.pciBus = mockPciBus;
    pLinuxSysmanImp->pciBdfInfo.pciDevice = mockPciDevice;
    pLinuxSysmanImp->pciBdfInfo.pciFunction = mockPciFunction;

    std::vector<uint8_t> firmwareData(1024, 0xAA);

    ze_result_t result = zesFirmwareFlash(handles[0], firmwareData.data(), static_cast<uint32_t>(firmwareData.size()));
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, result);
}

TEST_F(SysmanFirmwareFdoFixtureXe, GivenMalformedMtdLinesWhenFlashingExtendedFirmwareThenCoversParsingFailureCondition) {
    pMockSysfsAccess->mockFdoModeValue = "enabled";
    initFirmware();
    auto handles = getFirmwareHandles(mockFwHandlesCountFdo);
    ASSERT_NE(nullptr, handles[0]);

    // Mock fs access to return MTD lines with insufficient fields
    // This tests the false path of: if (iss >> mtdNumber >> size >> eraseSize >> name)
    pMockFsAccess->regionMode = MockFirmwareFsAccess::MtdRegionMode::malformedMtdLine;

    pLinuxSysmanImp->pciBdfInfo.pciBus = mockPciBus;
    pLinuxSysmanImp->pciBdfInfo.pciDevice = mockPciDevice;
    pLinuxSysmanImp->pciBdfInfo.pciFunction = mockPciFunction;
    pLinuxSysmanImp->pciBdfInfo.pciFunction = mockPciFunction;

    std::vector<uint8_t> firmwareData(1024, 0xAA);

    ze_result_t result = zesFirmwareFlash(handles[0], firmwareData.data(), static_cast<uint32_t>(firmwareData.size()));
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, result);
}

TEST_F(SysmanFirmwareFdoFixtureXe, GivenDeviceInFdoModeWhenFlashingFdoFirmwareThenSuccessIsReturned) {
    pMockSysfsAccess->mockFdoModeValue = "enabled";
    initFirmware();
    auto handles = getFirmwareHandles(mockFwHandlesCountFdo);
    ASSERT_NE(nullptr, handles[0]);

    // Enable large firmware regions to use mtd4 (large fd) for 8MB firmware
    pMockFsAccess->regionMode = MockFirmwareFsAccess::MtdRegionMode::singleRegion;

    // Mock successful system calls for MTD operations
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, &mockOpenSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsClose)> mockClose(&NEO::SysCalls::sysCallsClose, &mockCloseSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsLseek)> mockLseek(&NEO::SysCalls::sysCallsLseek, &mockLseekSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsRead)> mockRead(&NEO::SysCalls::sysCallsRead, &mockReadSpiDescriptorSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsIoctl)> mockIoctl(&NEO::SysCalls::sysCallsIoctl, &mockIoctlEraseSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsWrite)> mockWrite(&NEO::SysCalls::sysCallsWrite, &mockWriteSuccess);

    // Set up mock PCI BDF info
    pLinuxSysmanImp->pciBdfInfo.pciBus = mockPciBus;
    pLinuxSysmanImp->pciBdfInfo.pciDevice = mockPciDevice;
    pLinuxSysmanImp->pciBdfInfo.pciFunction = mockPciFunction;

    // Create test firmware data
    std::vector<uint8_t> firmwareData(0x800000, 0xAA);

    // Flashing FDO firmware (Flash_Override) when device is in FDO mode should work
    ze_result_t result = zesFirmwareFlash(handles[0], firmwareData.data(), static_cast<uint32_t>(firmwareData.size()));
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

} // namespace ult
} // namespace Sysman
} // namespace L0
