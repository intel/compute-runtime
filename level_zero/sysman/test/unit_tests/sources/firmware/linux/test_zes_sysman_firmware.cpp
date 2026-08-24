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

TEST_F(ZesSysmanFirmwareFixture, GivenBdfChangedWhenCallingFirmwareContextReInitThenCachedFwInterfaceIsRefreshedOnEachHandle) {
    initFirmware();
    ASSERT_FALSE(pSysmanDeviceImp->pFirmwareHandleContext->handleList.empty());

    auto pNewFwInterface = std::make_unique<MockFirmwareInterface>();
    pLinuxSysmanImp->pFwUtilInterface = pNewFwInterface.get();

    pSysmanDeviceImp->pFirmwareHandleContext->reInit();

    for (const auto &handle : pSysmanDeviceImp->pFirmwareHandleContext->handleList) {
        auto pFirmwareImp = static_cast<L0::Sysman::FirmwareImp *>(handle);
        auto pOsFirmware = static_cast<PublicLinuxFirmwareImp *>(pFirmwareImp->pOsFirmware.get());
        EXPECT_EQ(pNewFwInterface.get(), pOsFirmware->pFwInterface);
    }

    pLinuxSysmanImp->pFwUtilInterface = pMockFwInterface.get();
}

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

    void setDevicePciBdfInfo(uint32_t pciDomain, uint32_t pciBus, uint32_t pciDevice, uint32_t pciFunction) {
        pLinuxSysmanImp->pciBdfInfo.pciDomain = pciDomain;
        pLinuxSysmanImp->pciBdfInfo.pciBus = pciBus;
        pLinuxSysmanImp->pciBdfInfo.pciDevice = pciDevice;
        pLinuxSysmanImp->pciBdfInfo.pciFunction = pciFunction;
    }

    // Installs the system calls performed on the MTD device and records their arguments
    struct MockMtdSysCallsBackup {
        VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen{&NEO::SysCalls::sysCallsOpen, &mockOpenSuccess};
        VariableBackup<decltype(NEO::SysCalls::sysCallsClose)> mockClose{&NEO::SysCalls::sysCallsClose, &mockCloseSuccess};
        VariableBackup<decltype(NEO::SysCalls::sysCallsLseek)> mockLseek{&NEO::SysCalls::sysCallsLseek, &mockLseekSuccess};
        VariableBackup<decltype(NEO::SysCalls::sysCallsIoctl)> mockIoctl{&NEO::SysCalls::sysCallsIoctl, &mockIoctlEraseSuccess};
        VariableBackup<decltype(NEO::SysCalls::sysCallsWrite)> mockWrite{&NEO::SysCalls::sysCallsWrite, &mockWriteSuccess};
        VariableBackup<MockMtdSysCalls> recordedSysCalls{&mockMtdSysCalls, {}};
        VariableBackup<int> syncCalled{&NEO::SysCalls::syncCalled, 0};
    };
};

// New fixture specifically for FDO blocking tests - creates all 4 firmware handles manually
class SysmanFirmwareBlockingFdoFixtureXe : public SysmanFirmwareFdoFixtureXe {
  protected:
    void SetUp() override {
        SysmanFirmwareFdoFixtureXe::SetUp();
        // Manually create all 4 firmware handles (GSC, OptionROM, PSC, Flash_Override)
        pMockFsAccess->mockFdoValue = "disabled";

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

    pMockFsAccess->mockFdoValue = "enabled";

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
    pMockFsAccess->mockFdoValue = "enabled";

    for (auto *pFirmwareHandle : pSysmanDeviceImp->pFirmwareHandleContext->handleList) {
        auto handle = pFirmwareHandle->toHandle();
        uint32_t completionPercent = 0;
        ze_result_t result = zesFirmwareGetFlashProgress(handle, &completionPercent);
        EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, result);
    }
}

TEST_F(SysmanFirmwareBlockingFdoFixtureXe, GivenDeviceInNormalConditionsWhenGettingFlashProgressWithFlashOverrideHandleThenNotAvailableIsReturned) {
    pMockFsAccess->mockFdoValue = "disabled";

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
    pMockFsAccess->mockFdoValue = "enabled";
    initFirmware();
    auto handles = getFirmwareHandles(mockFwHandlesCountFdo);
    ASSERT_NE(nullptr, handles[0]);
    zes_firmware_properties_t properties = {};
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesFirmwareGetProperties(handles[0], &properties));
    EXPECT_STREQ(mockSupportedFirmwareTypesFdo[0].c_str(), properties.name);
    EXPECT_STREQ(mockUnknownVersion.c_str(), properties.version);
}

TEST_F(SysmanFirmwareFdoFixtureXe, GivenValidParametersWhenFlashingExtendedFirmwareThenWholeDataDeviceIsErasedAndImageIsWritten) {
    pMockFsAccess->mockFdoValue = "enabled";
    initFirmware();
    auto handles = getFirmwareHandles(mockFwHandlesCountFdo);
    ASSERT_NE(nullptr, handles[0]);

    pMockFsAccess->mtdMode = MockFirmwareFsAccess::MtdMode::dataDevice;
    setDevicePciBdfInfo(mockPciDomain, mockPciBus, mockPciDevice, mockPciFunction);

    MockMtdSysCallsBackup mtdSysCallsBackup;

    std::vector<uint8_t> firmwareData(MockFirmwareFsAccess::mockMtdDataDeviceSize, 0xAA);

    ze_result_t result = zesFirmwareFlash(handles[0], firmwareData.data(), static_cast<uint32_t>(firmwareData.size()));
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_EQ(mockMtdDevicePath, mockMtdSysCalls.openedPath);
    EXPECT_EQ(2u, mockMtdSysCalls.openCount); // One open for the erase and one for the write
    EXPECT_EQ(0u, mockMtdSysCalls.eraseStart);
    EXPECT_EQ(MockFirmwareFsAccess::mockMtdDataDeviceSize, mockMtdSysCalls.eraseLength);
    EXPECT_EQ(0, mockMtdSysCalls.writeOffset);
    EXPECT_EQ(static_cast<const void *>(firmwareData.data()), mockMtdSysCalls.writeData);
    EXPECT_EQ(firmwareData.size(), mockMtdSysCalls.writeCount);
    EXPECT_EQ(1, NEO::SysCalls::syncCalled);
}

TEST_F(SysmanFirmwareFdoFixtureXe, GivenImageSmallerThanDataDeviceWhenFlashingExtendedFirmwareThenWholeDataDeviceIsErasedAndImageIsWritten) {
    pMockFsAccess->mockFdoValue = "enabled";
    initFirmware();
    auto handles = getFirmwareHandles(mockFwHandlesCountFdo);
    ASSERT_NE(nullptr, handles[0]);

    setDevicePciBdfInfo(mockPciDomain, mockPciBus, mockPciDevice, mockPciFunction);

    MockMtdSysCallsBackup mtdSysCallsBackup;

    std::vector<uint8_t> firmwareData(0x1000, 0xAA);

    ze_result_t result = zesFirmwareFlash(handles[0], firmwareData.data(), static_cast<uint32_t>(firmwareData.size()));
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_EQ(MockFirmwareFsAccess::mockMtdDataDeviceSize, mockMtdSysCalls.eraseLength);
    EXPECT_EQ(0, mockMtdSysCalls.writeOffset);
    EXPECT_EQ(firmwareData.size(), mockMtdSysCalls.writeCount);
}

TEST_F(SysmanFirmwareFdoFixtureXe, GivenMtdDevicesOfMultiplePciDevicesWhenFlashingExtendedFirmwareThenDataDeviceOfThisDeviceIsFlashed) {
    pMockFsAccess->mockFdoValue = "enabled";
    initFirmware();
    auto handles = getFirmwareHandles(mockFwHandlesCountFdo);
    ASSERT_NE(nullptr, handles[0]);

    pMockFsAccess->mtdMode = MockFirmwareFsAccess::MtdMode::multipleDevices;
    setDevicePciBdfInfo(mockPciDomain, mockPciBus, mockPciDevice, mockPciFunction);

    MockMtdSysCallsBackup mtdSysCallsBackup;

    std::vector<uint8_t> firmwareData(MockFirmwareFsAccess::mockMtdSecondDataDeviceSize, 0xAA);

    ze_result_t result = zesFirmwareFlash(handles[0], firmwareData.data(), static_cast<uint32_t>(firmwareData.size()));
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_EQ(mockMtdSecondDevicePath, mockMtdSysCalls.openedPath);
    EXPECT_EQ(MockFirmwareFsAccess::mockMtdSecondDataDeviceSize, mockMtdSysCalls.eraseLength);
    EXPECT_EQ(firmwareData.size(), mockMtdSysCalls.writeCount);
}

TEST_F(SysmanFirmwareFdoFixtureXe, GivenDeviceWithNonZeroPciDomainWhenFlashingExtendedFirmwareThenPciDomainIsNotPartOfMtdDeviceName) {
    pMockFsAccess->mockFdoValue = "enabled";
    initFirmware();
    auto handles = getFirmwareHandles(mockFwHandlesCountFdo);
    ASSERT_NE(nullptr, handles[0]);

    // The KMD does not include the PCI domain in the id of the auxiliary device, hence the same MTD
    // device name is expected as for the device in domain 0
    setDevicePciBdfInfo(1u, mockPciBus, mockPciDevice, mockPciFunction);

    MockMtdSysCallsBackup mtdSysCallsBackup;

    std::vector<uint8_t> firmwareData(MockFirmwareFsAccess::mockMtdDataDeviceSize, 0xAA);

    ze_result_t result = zesFirmwareFlash(handles[0], firmwareData.data(), static_cast<uint32_t>(firmwareData.size()));
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(mockMtdDevicePath, mockMtdSysCalls.openedPath);
}

TEST_F(SysmanFirmwareFdoFixtureXe, GivenNullImagePointerWhenFlashingExtendedFirmwareThenInvalidNullPointerIsReturned) {
    pMockFsAccess->mockFdoValue = "enabled";
    initFirmware();
    auto handles = getFirmwareHandles(mockFwHandlesCountFdo);
    ASSERT_NE(nullptr, handles[0]);

    setDevicePciBdfInfo(mockPciDomain, mockPciBus, mockPciDevice, mockPciFunction);

    MockMtdSysCallsBackup mtdSysCallsBackup;

    ze_result_t result = zesFirmwareFlash(handles[0], nullptr, MockFirmwareFsAccess::mockMtdDataDeviceSize);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_NULL_POINTER, result);

    EXPECT_EQ(0u, mockMtdSysCalls.openCount);
}

TEST_F(SysmanFirmwareFdoFixtureXe, GivenBdfOfThisDeviceNotMatchingAnyMtdDeviceWhenFlashingExtendedFirmwareThenErrorIsReturned) {
    pMockFsAccess->mockFdoValue = "enabled";
    initFirmware();
    auto handles = getFirmwareHandles(mockFwHandlesCountFdo);
    ASSERT_NE(nullptr, handles[0]);

    // The enumerated DATA device belongs to a device with a different BDF
    setDevicePciBdfInfo(0u, 0u, 0u, 0u);

    MockMtdSysCallsBackup mtdSysCallsBackup;

    std::vector<uint8_t> firmwareData(1024, 0xAA);
    ze_result_t result = zesFirmwareFlash(handles[0], firmwareData.data(), static_cast<uint32_t>(firmwareData.size()));
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, result);
    EXPECT_EQ(0u, mockMtdSysCalls.openCount);
}

TEST_F(SysmanFirmwareFdoFixtureXe, GivenProcMtdReadFailsWhenFlashingExtendedFirmwareThenErrorIsReturned) {
    pMockFsAccess->mockFdoValue = "enabled";
    initFirmware();
    auto handles = getFirmwareHandles(mockFwHandlesCountFdo);
    ASSERT_NE(nullptr, handles[0]);

    pMockFsAccess->readResult = ZE_RESULT_ERROR_UNKNOWN;
    setDevicePciBdfInfo(mockPciDomain, mockPciBus, mockPciDevice, mockPciFunction);

    std::vector<uint8_t> firmwareData(1024, 0xAA);
    ze_result_t result = zesFirmwareFlash(handles[0], firmwareData.data(), static_cast<uint32_t>(firmwareData.size()));
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, result);
}

TEST_F(SysmanFirmwareFdoFixtureXe, GivenNoMtdDevicesEnumeratedWhenFlashingExtendedFirmwareThenErrorIsReturned) {
    pMockFsAccess->mockFdoValue = "enabled";
    initFirmware();
    auto handles = getFirmwareHandles(mockFwHandlesCountFdo);
    ASSERT_NE(nullptr, handles[0]);

    pMockFsAccess->mtdMode = MockFirmwareFsAccess::MtdMode::noMtdDevices;
    setDevicePciBdfInfo(mockPciDomain, mockPciBus, mockPciDevice, mockPciFunction);

    std::vector<uint8_t> firmwareData(1024, 0xAA);
    ze_result_t result = zesFirmwareFlash(handles[0], firmwareData.data(), static_cast<uint32_t>(firmwareData.size()));
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, result);
}

TEST_F(SysmanFirmwareFdoFixtureXe, GivenEmptyProcMtdFileWhenFlashingExtendedFirmwareThenErrorIsReturned) {
    pMockFsAccess->mockFdoValue = "enabled";
    initFirmware();
    auto handles = getFirmwareHandles(mockFwHandlesCountFdo);
    ASSERT_NE(nullptr, handles[0]);

    pMockFsAccess->mtdMode = MockFirmwareFsAccess::MtdMode::emptyMtdFile;
    setDevicePciBdfInfo(mockPciDomain, mockPciBus, mockPciDevice, mockPciFunction);

    std::vector<uint8_t> firmwareData(1024, 0xAA);
    ze_result_t result = zesFirmwareFlash(handles[0], firmwareData.data(), static_cast<uint32_t>(firmwareData.size()));
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, result);
}

TEST_F(SysmanFirmwareFdoFixtureXe, GivenMtdDevicesOfOtherPciDeviceOnlyWhenFlashingExtendedFirmwareThenErrorIsReturned) {
    pMockFsAccess->mockFdoValue = "enabled";
    initFirmware();
    auto handles = getFirmwareHandles(mockFwHandlesCountFdo);
    ASSERT_NE(nullptr, handles[0]);

    pMockFsAccess->mtdMode = MockFirmwareFsAccess::MtdMode::otherDeviceOnly;
    setDevicePciBdfInfo(mockPciDomain, mockPciBus, mockPciDevice, mockPciFunction);

    MockMtdSysCallsBackup mtdSysCallsBackup;

    std::vector<uint8_t> firmwareData(1024, 0xAA);
    ze_result_t result = zesFirmwareFlash(handles[0], firmwareData.data(), static_cast<uint32_t>(firmwareData.size()));
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, result);

    EXPECT_EQ(0u, mockMtdSysCalls.openCount);
}

TEST_F(SysmanFirmwareFdoFixtureXe, GivenNoDataDeviceWhenFlashingExtendedFirmwareThenErrorIsReturned) {
    pMockFsAccess->mockFdoValue = "enabled";
    initFirmware();
    auto handles = getFirmwareHandles(mockFwHandlesCountFdo);
    ASSERT_NE(nullptr, handles[0]);

    pMockFsAccess->mtdMode = MockFirmwareFsAccess::MtdMode::noDataDevice;
    setDevicePciBdfInfo(mockPciDomain, mockPciBus, mockPciDevice, mockPciFunction);

    MockMtdSysCallsBackup mtdSysCallsBackup;

    std::vector<uint8_t> firmwareData(1024, 0xAA);
    ze_result_t result = zesFirmwareFlash(handles[0], firmwareData.data(), static_cast<uint32_t>(firmwareData.size()));
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, result);
    EXPECT_EQ(0u, mockMtdSysCalls.openCount);
}

TEST_F(SysmanFirmwareFdoFixtureXe, GivenMalformedDeviceSizeWhenFlashingExtendedFirmwareThenErrorIsReturned) {
    pMockFsAccess->mockFdoValue = "enabled";
    initFirmware();
    auto handles = getFirmwareHandles(mockFwHandlesCountFdo);
    ASSERT_NE(nullptr, handles[0]);

    pMockFsAccess->mtdMode = MockFirmwareFsAccess::MtdMode::malformedDeviceSize;
    setDevicePciBdfInfo(mockPciDomain, mockPciBus, mockPciDevice, mockPciFunction);

    MockMtdSysCallsBackup mtdSysCallsBackup;

    std::vector<uint8_t> firmwareData(1024, 0xAA);
    ze_result_t result = zesFirmwareFlash(handles[0], firmwareData.data(), static_cast<uint32_t>(firmwareData.size()));
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, result);
    EXPECT_EQ(0u, mockMtdSysCalls.openCount);
}

TEST_F(SysmanFirmwareFdoFixtureXe, GivenMismatchedQuotesInMtdDeviceNamesWhenFlashingExtendedFirmwareThenErrorIsReturned) {
    pMockFsAccess->mockFdoValue = "enabled";
    initFirmware();
    auto handles = getFirmwareHandles(mockFwHandlesCountFdo);
    ASSERT_NE(nullptr, handles[0]);

    pMockFsAccess->mtdMode = MockFirmwareFsAccess::MtdMode::mismatchedQuotes;
    setDevicePciBdfInfo(mockPciDomain, mockPciBus, mockPciDevice, mockPciFunction);

    MockMtdSysCallsBackup mtdSysCallsBackup;

    std::vector<uint8_t> firmwareData(1024, 0xAA);
    ze_result_t result = zesFirmwareFlash(handles[0], firmwareData.data(), static_cast<uint32_t>(firmwareData.size()));
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, result);
    EXPECT_EQ(0u, mockMtdSysCalls.openCount);
}

TEST_F(SysmanFirmwareFdoFixtureXe, GivenMalformedMtdEntryWhenFlashingExtendedFirmwareThenErrorIsReturned) {
    pMockFsAccess->mockFdoValue = "enabled";
    initFirmware();
    auto handles = getFirmwareHandles(mockFwHandlesCountFdo);
    ASSERT_NE(nullptr, handles[0]);

    pMockFsAccess->mtdMode = MockFirmwareFsAccess::MtdMode::malformedMtdLine;
    setDevicePciBdfInfo(mockPciDomain, mockPciBus, mockPciDevice, mockPciFunction);

    std::vector<uint8_t> firmwareData(1024, 0xAA);
    ze_result_t result = zesFirmwareFlash(handles[0], firmwareData.data(), static_cast<uint32_t>(firmwareData.size()));
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, result);
}

TEST_F(SysmanFirmwareFdoFixtureXe, GivenImageLargerThanDataDeviceWhenFlashingExtendedFirmwareThenInvalidSizeErrorIsReturned) {
    pMockFsAccess->mockFdoValue = "enabled";
    initFirmware();
    auto handles = getFirmwareHandles(mockFwHandlesCountFdo);
    ASSERT_NE(nullptr, handles[0]);

    setDevicePciBdfInfo(mockPciDomain, mockPciBus, mockPciDevice, mockPciFunction);

    MockMtdSysCallsBackup mtdSysCallsBackup;

    std::vector<uint8_t> firmwareData(MockFirmwareFsAccess::mockMtdDataDeviceSize + 0x1000, 0xAA);

    ze_result_t result = zesFirmwareFlash(handles[0], firmwareData.data(), static_cast<uint32_t>(firmwareData.size()));
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_SIZE, result);

    EXPECT_EQ(0u, mockMtdSysCalls.openCount);
}

TEST_F(SysmanFirmwareFdoFixtureXe, GivenZeroImageSizeWhenFlashingExtendedFirmwareThenInvalidSizeErrorIsReturned) {
    pMockFsAccess->mockFdoValue = "enabled";
    initFirmware();
    auto handles = getFirmwareHandles(mockFwHandlesCountFdo);
    ASSERT_NE(nullptr, handles[0]);

    setDevicePciBdfInfo(mockPciDomain, mockPciBus, mockPciDevice, mockPciFunction);

    MockMtdSysCallsBackup mtdSysCallsBackup;

    std::vector<uint8_t> firmwareData(1024, 0xAA);

    ze_result_t result = zesFirmwareFlash(handles[0], firmwareData.data(), 0u);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_SIZE, result);

    EXPECT_EQ(0u, mockMtdSysCalls.openCount);
}

TEST_F(SysmanFirmwareFdoFixtureXe, GivenMtdDeviceOpenFailsWhenFlashingExtendedFirmwareThenErrorIsReturned) {
    pMockFsAccess->mockFdoValue = "enabled";
    initFirmware();
    auto handles = getFirmwareHandles(mockFwHandlesCountFdo);
    ASSERT_NE(nullptr, handles[0]);

    setDevicePciBdfInfo(mockPciDomain, mockPciBus, mockPciDevice, mockPciFunction);

    MockMtdSysCallsBackup mtdSysCallsBackup;
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpenFailure(&NEO::SysCalls::sysCallsOpen, [](const char *pathname, int flags) -> int {
        errno = ENOENT;
        return -1;
    });

    std::vector<uint8_t> firmwareData(MockFirmwareFsAccess::mockMtdDataDeviceSize, 0xAA);
    ze_result_t result = zesFirmwareFlash(handles[0], firmwareData.data(), static_cast<uint32_t>(firmwareData.size()));
    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, result);
}

TEST_F(SysmanFirmwareFdoFixtureXe, GivenEraseFailsWhenFlashingExtendedFirmwareThenErrorIsReturnedAndNothingIsWritten) {
    pMockFsAccess->mockFdoValue = "enabled";
    initFirmware();
    auto handles = getFirmwareHandles(mockFwHandlesCountFdo);
    ASSERT_NE(nullptr, handles[0]);

    setDevicePciBdfInfo(mockPciDomain, mockPciBus, mockPciDevice, mockPciFunction);

    MockMtdSysCallsBackup mtdSysCallsBackup;
    VariableBackup<decltype(NEO::SysCalls::sysCallsIoctl)> mockIoctlFailure(&NEO::SysCalls::sysCallsIoctl, [](int fd, unsigned long request, void *arg) -> int {
        errno = ENOENT;
        return -1;
    });

    std::vector<uint8_t> firmwareData(MockFirmwareFsAccess::mockMtdDataDeviceSize, 0xAA);
    ze_result_t result = zesFirmwareFlash(handles[0], firmwareData.data(), static_cast<uint32_t>(firmwareData.size()));
    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, result);

    EXPECT_EQ(1u, mockMtdSysCalls.openCount);
    EXPECT_EQ(0u, mockMtdSysCalls.writeCount);
    EXPECT_EQ(0, NEO::SysCalls::syncCalled);
}

TEST_F(SysmanFirmwareFdoFixtureXe, GivenWriteFailsWhenFlashingExtendedFirmwareThenErrorIsReturned) {
    pMockFsAccess->mockFdoValue = "enabled";
    initFirmware();
    auto handles = getFirmwareHandles(mockFwHandlesCountFdo);
    ASSERT_NE(nullptr, handles[0]);

    setDevicePciBdfInfo(mockPciDomain, mockPciBus, mockPciDevice, mockPciFunction);

    MockMtdSysCallsBackup mtdSysCallsBackup;
    VariableBackup<decltype(NEO::SysCalls::sysCallsWrite)> mockWriteFailure(&NEO::SysCalls::sysCallsWrite, [](int fd, const void *buf, size_t count) -> ssize_t {
        errno = ENOENT;
        return -1;
    });

    std::vector<uint8_t> firmwareData(MockFirmwareFsAccess::mockMtdDataDeviceSize, 0xAA);
    ze_result_t result = zesFirmwareFlash(handles[0], firmwareData.data(), static_cast<uint32_t>(firmwareData.size()));
    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, result);

    EXPECT_EQ(0, NEO::SysCalls::syncCalled);
}

} // namespace ult
} // namespace Sysman
} // namespace L0
