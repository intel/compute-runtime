/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/sysman/source/driver/sysman_os_driver.h"
#include "level_zero/sysman/source/shared/linux/product_helper/sysman_product_helper_hw.h"
#include "level_zero/sysman/test/unit_tests/sources/firmware/linux/mock_zes_sysman_firmware.h"

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

TEST_F(ZesSysmanFirmwareFixture, GivenValidDeviceHandleAndFirmwareUnSupportedWhenCallingEnumerateThenVerifyFirmwareDomainsAreZero) {

    struct MockSysmanProductHelperFirmware : L0::Sysman::SysmanProductHelperHw<IGFX_UNKNOWN> {
        MockSysmanProductHelperFirmware() = default;
        void getDeviceSupportedFwTypes(FirmwareUtil *pFwInterface, std::vector<std::string> &fwTypes) override {
            return;
        }
    };

    std::unique_ptr<SysmanProductHelper> pSysmanProductHelper = std::make_unique<MockSysmanProductHelperFirmware>();
    std::swap(pLinuxSysmanImp->pSysmanProductHelper, pSysmanProductHelper);

    uint32_t count = 0;
    ze_result_t result = zesDeviceEnumFirmwares(device->toHandle(), &count, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(count, 0u);
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

inline static int openMockReturnSuccess(const char *pathname, int flags) {
    NEO::SysCalls::closeFuncCalled = 0;
    return 0;
}

TEST_F(SysmanSurvivabilityDeviceTest, GivenSurvivabilityDeviceWhenFirmwareEnumerationApiIsCalledThenFirmwareHandlesAreReturned) {
    const uint32_t numEntries = sizeof(mockSurvivabilityDevEntries) / sizeof(mockSurvivabilityDevEntries[0]);
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpendir)> mockOpendir(&NEO::SysCalls::sysCallsOpendir, [](const char *name) -> DIR * {
        return reinterpret_cast<DIR *>(0xc001);
    });

    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> openBackup{&NEO::SysCalls::sysCallsOpen, openMockReturnSuccess};
    VariableBackup<decltype(NEO::SysCalls::sysCallsReaddir)> mockReaddir(
        &NEO::SysCalls::sysCallsReaddir, [](DIR * dir) -> struct dirent * {
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

    SysmanDriverHandleImp *pSysmanDriverHandleImp = static_cast<SysmanDriverHandleImp *>(sysmanDriverHandle);
    auto pSysmanDevice = pSysmanDriverHandleImp->sysmanDevices[0];
    auto pSysmanDeviceImp = static_cast<L0::Sysman::SysmanDeviceImp *>(pSysmanDevice);
    auto pOsSysman = pSysmanDeviceImp->pOsSysman;
    auto pLinuxSysmanImp = static_cast<PublicLinuxSysmanImp *>(pOsSysman);
    pLinuxSysmanImp->pFwUtilInterface = new MockFirmwareInterface();

    uint32_t count = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, SysmanDevice::firmwareGet(pSysmanDevice->toHandle(), &count, nullptr));
    EXPECT_EQ(count, mockFwHandlesCount);
    delete pLinuxSysmanImp->pFwUtilInterface;
    pLinuxSysmanImp->pFwUtilInterface = nullptr;
    delete sysmanDriverHandle;
    globalSysmanDriver = nullptr;
}

} // namespace ult
} // namespace Sysman
} // namespace L0
