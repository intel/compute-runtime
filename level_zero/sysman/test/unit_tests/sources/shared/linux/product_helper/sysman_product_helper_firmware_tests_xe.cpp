/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/sysman/source/driver/sysman_os_driver.h"
#include "level_zero/sysman/source/shared/linux/product_helper/sysman_product_helper_hw.h"
#include "level_zero/sysman/test/unit_tests/sources/firmware/linux/mock_zes_sysman_firmware.h"
#include "level_zero/sysman/test/unit_tests/sources/shared/linux/kmd_interface/mock_sysman_kmd_interface_xe.h"

#include <algorithm>

namespace L0 {
namespace Sysman {
namespace ult {

class ZesSysmanProductHelperFirmwareFixtureXe : public SysmanDeviceFixture {

  protected:
    zes_firmware_handle_t hSysmanFirmware = {};
    std::unique_ptr<MockFirmwareInterface> pMockFwInterface;
    MockFirmwareSysfsAccess *pSysfsAccess = nullptr;
    MockSysmanKmdInterfaceXe *pSysmanKmdInterface = nullptr;
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
        pSysmanKmdInterface = new MockSysmanKmdInterfaceXe(pLinuxSysmanImp->getSysmanProductHelper());
        pSysmanKmdInterface->pSysfsAccess = std::make_unique<MockFirmwareSysfsAccess>();
        pLinuxSysmanImp->pSysfsAccess = pSysmanKmdInterface->pSysfsAccess.get();
        pLinuxSysmanImp->pSysmanKmdInterface.reset(pSysmanKmdInterface);
        pSysfsAccess = static_cast<MockFirmwareSysfsAccess *>(pSysmanKmdInterface->pSysfsAccess.get());

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

HWTEST2_F(ZesSysmanProductHelperFirmwareFixtureXe, GivenComponentCountZeroAndLateBindingIsSupportedThenWhenCallingZesFirmwareGetProperCountIsReturned, IsBMG) {
    constexpr uint32_t mockFwHandlesCount = 5;
    std::vector<zes_firmware_handle_t> firmwareHandle{};
    uint32_t count = 0;

    ze_result_t result = zesDeviceEnumFirmwares(device->toHandle(), &count, nullptr);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(count, mockFwHandlesCount);

    uint32_t testCount = count + 1;

    result = zesDeviceEnumFirmwares(device->toHandle(), &testCount, nullptr);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(testCount, count);

    firmwareHandle.resize(count);
    result = zesDeviceEnumFirmwares(device->toHandle(), &count, firmwareHandle.data());

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(count, mockFwHandlesCount);
}

HWTEST2_F(ZesSysmanProductHelperFirmwareFixtureXe, GivenLateBindingFirmwareIsNotSupportedThenValidCountIsReturned, IsBMG) {

    constexpr uint32_t mockFwHandlesCount = 3;
    pSysfsAccess->canReadResult = ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    std::vector<zes_firmware_handle_t> firmwareHandle{};
    uint32_t count = 0;

    ze_result_t result = zesDeviceEnumFirmwares(device->toHandle(), &count, nullptr);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(count, mockFwHandlesCount);
}

HWTEST2_F(ZesSysmanProductHelperFirmwareFixtureXe, GivenValidLateBindingFirmwareHandleWhenFlashingFirmwareThenSuccessIsReturned, IsBMG) {
    constexpr uint32_t mockFwHandlesCount = 5;
    initFirmware();

    auto handles = getFirmwareHandles(mockFwHandlesCount);
    for (auto handle : handles) {
        zes_firmware_properties_t properties = {};
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesFirmwareGetProperties(handle, &properties));
        if (std::find(lateBindingFirmwareTypes.begin(), lateBindingFirmwareTypes.end(), properties.name) != lateBindingFirmwareTypes.end()) {
            uint8_t testImage[ZES_STRING_PROPERTY_SIZE] = {};
            memset(testImage, 0xA, ZES_STRING_PROPERTY_SIZE);
            EXPECT_EQ(ZE_RESULT_SUCCESS, zesFirmwareFlash(handle, (void *)testImage, ZES_STRING_PROPERTY_SIZE));
        }
    }
}

HWTEST2_F(ZesSysmanProductHelperFirmwareFixtureXe, GivenValidLateBindingFirmwareHandleWhenGettingFirmwarePropertiesThenValidVersionIsReturned, IsBMG) {

    constexpr uint32_t mockFwHandlesCount = 5;
    initFirmware();

    auto handles = getFirmwareHandles(mockFwHandlesCount);
    for (auto handle : handles) {
        zes_firmware_properties_t properties = {};
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesFirmwareGetProperties(handle, &properties));
        if (std::find(lateBindingFirmwareTypes.begin(), lateBindingFirmwareTypes.end(), properties.name) != lateBindingFirmwareTypes.end()) {
            EXPECT_STREQ(mocklateBindingVersion.c_str(), properties.version);
        }
    }
}

HWTEST2_F(ZesSysmanProductHelperFirmwareFixtureXe, GivenValidLateBindingFirmwareHandleWhenGettingFirmwarePropertiesAndSysFsReadFailsThenVersionIsUnknown, IsBMG) {

    constexpr uint32_t mockFwHandlesCount = 5;
    pSysfsAccess->readResult = ZE_RESULT_ERROR_UNKNOWN;
    initFirmware();

    auto handles = getFirmwareHandles(mockFwHandlesCount);
    for (auto handle : handles) {
        zes_firmware_properties_t properties = {};
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesFirmwareGetProperties(handle, &properties));
        if (std::find(lateBindingFirmwareTypes.begin(), lateBindingFirmwareTypes.end(), properties.name) != lateBindingFirmwareTypes.end()) {
            EXPECT_STREQ(mockUnknownVersion.c_str(), properties.version);
        }
    }
}

} // namespace ult
} // namespace Sysman
} // namespace L0
