/*
 * Copyright (C) 2022-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/sysman/test/unit_tests/sources/firmware/linux/mock_zes_sysman_firmware_prelim.h"

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

TEST_F(ZesSysmanFirmwareFixture, GivenComponentCountZeroWhenCallingzesFirmwareGetThenZeroCountIsReturnedAndVerifyzesFirmwareGetCallSucceeds) {
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

    L0::Sysman::FirmwareImp *ptestFirmwareImp = new L0::Sysman::FirmwareImp(pSysmanDeviceImp->pFirmwareHandleContext->pOsSysman, mockSupportedFirmwareTypes[0]);
    pSysmanDeviceImp->pFirmwareHandleContext->handleList.push_back(ptestFirmwareImp);
    result = zesDeviceEnumFirmwares(device->toHandle(), &count, nullptr);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(count, mockFwHandlesCount);

    testCount = count;

    firmwareHandle.resize(testCount);
    result = zesDeviceEnumFirmwares(device->toHandle(), &testCount, firmwareHandle.data());

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, firmwareHandle.data());
    EXPECT_EQ(testCount, mockFwHandlesCount);

    pSysmanDeviceImp->pFirmwareHandleContext->handleList.pop_back();
    delete ptestFirmwareImp;
}

TEST_F(ZesSysmanFirmwareFixture, GivenValidFirmwareHandleWhenGettingFirmwarePropertiesThenVersionIsReturned) {
    initFirmware();

    L0::Sysman::FirmwareImp *ptestFirmwareImp = new L0::Sysman::FirmwareImp(pSysmanDeviceImp->pFirmwareHandleContext->pOsSysman, mockSupportedFirmwareTypes[0]);
    pSysmanDeviceImp->pFirmwareHandleContext->handleList.push_back(ptestFirmwareImp);

    auto handles = getFirmwareHandles(mockFwHandlesCount);

    zes_firmware_properties_t properties = {};
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesFirmwareGetProperties(handles[0], &properties));
    EXPECT_STREQ("GFX", properties.name);
    EXPECT_STREQ(mockFwVersion.c_str(), properties.version);

    pSysmanDeviceImp->pFirmwareHandleContext->handleList.pop_back();
    delete ptestFirmwareImp;
}

TEST_F(ZesSysmanFirmwareFixture, GivenValidFirmwareHandleWhenGettingPscPropertiesThenVersionIsReturned) {
    initFirmware();

    L0::Sysman::FirmwareImp *ptestFirmwareImp = new L0::Sysman::FirmwareImp(pSysmanDeviceImp->pFirmwareHandleContext->pOsSysman, mockSupportedFirmwareTypes[2]);
    pSysmanDeviceImp->pFirmwareHandleContext->handleList.push_back(ptestFirmwareImp);

    auto handles = getFirmwareHandles(mockFwHandlesCount);

    zes_firmware_properties_t properties = {};
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesFirmwareGetProperties(handles[2], &properties));
    EXPECT_STREQ(mockSupportedFirmwareTypes[2].c_str(), properties.name);
    EXPECT_STREQ(mockPscVersion.c_str(), properties.version);

    pSysmanDeviceImp->pFirmwareHandleContext->handleList.pop_back();
    delete ptestFirmwareImp;
}

TEST_F(ZesSysmanFirmwareFixture, GivenNullDirEntriesWhenGettingPscPropertiesThenUnknownVersionIsReturned) {
    initFirmware();

    L0::Sysman::FirmwareImp *ptestFirmwareImp = new L0::Sysman::FirmwareImp(pSysmanDeviceImp->pFirmwareHandleContext->pOsSysman, mockSupportedFirmwareTypes[2]);
    pSysmanDeviceImp->pFirmwareHandleContext->handleList.push_back(ptestFirmwareImp);

    pMockSysfsAccess->isNullDirEntries = true;
    auto handles = getFirmwareHandles(mockFwHandlesCount);

    zes_firmware_properties_t properties = {};
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesFirmwareGetProperties(handles[2], &properties));
    EXPECT_STREQ(mockSupportedFirmwareTypes[2].c_str(), properties.name);
    EXPECT_STREQ(mockUnknownVersion.c_str(), properties.version);

    pSysmanDeviceImp->pFirmwareHandleContext->handleList.pop_back();
    delete ptestFirmwareImp;
}

TEST_F(ZesSysmanFirmwareFixture, GivenValidFirmwareHandleWhenFailedToReadPSCVersionFromSysfsThenUnknownVersionIsReturned) {
    initFirmware();

    L0::Sysman::FirmwareImp *ptestFirmwareImp = new L0::Sysman::FirmwareImp(pSysmanDeviceImp->pFirmwareHandleContext->pOsSysman, mockSupportedFirmwareTypes[2]);
    pSysmanDeviceImp->pFirmwareHandleContext->handleList.push_back(ptestFirmwareImp);

    pMockSysfsAccess->readResult = ZE_RESULT_ERROR_NOT_AVAILABLE;
    auto handles = getFirmwareHandles(mockFwHandlesCount);

    zes_firmware_properties_t properties = {};
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesFirmwareGetProperties(handles[2], &properties));
    EXPECT_STREQ(mockSupportedFirmwareTypes[2].c_str(), properties.name);
    EXPECT_STREQ(mockUnknownVersion.c_str(), properties.version);

    pSysmanDeviceImp->pFirmwareHandleContext->handleList.pop_back();
    delete ptestFirmwareImp;
}

TEST_F(ZesSysmanFirmwareFixture, GivenRepeatedFWTypesWhenInitializingFirmwareContextThenexpectNoHandles) {
    for (const auto &handle : pSysmanDeviceImp->pFirmwareHandleContext->handleList) {
        delete handle;
    }
    pSysmanDeviceImp->pFirmwareHandleContext->handleList.clear();
    pFsAccess->isReadFwTypes = false;

    pSysmanDeviceImp->pFirmwareHandleContext->init();

    EXPECT_EQ(1u, pSysmanDeviceImp->pFirmwareHandleContext->handleList.size());
}

TEST_F(ZesSysmanFirmwareFixture, GivenValidFirmwareHandleWhenFlashingGscFirmwareThenSuccessIsReturned) {
    initFirmware();

    L0::Sysman::FirmwareImp *ptestFirmwareImp = new L0::Sysman::FirmwareImp(pSysmanDeviceImp->pFirmwareHandleContext->pOsSysman, mockSupportedFirmwareTypes[0]);
    pSysmanDeviceImp->pFirmwareHandleContext->handleList.push_back(ptestFirmwareImp);

    auto handles = getFirmwareHandles(mockFwHandlesCount);
    uint8_t testImage[ZES_STRING_PROPERTY_SIZE] = {};
    memset(testImage, 0xA, ZES_STRING_PROPERTY_SIZE);
    for (auto handle : handles) {
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesFirmwareFlash(handle, (void *)testImage, ZES_STRING_PROPERTY_SIZE));
    }
    pSysmanDeviceImp->pFirmwareHandleContext->handleList.pop_back();
    delete ptestFirmwareImp;
}

TEST_F(ZesSysmanFirmwareFixture, GivenValidFirmwareHandleWhenFlashingPscFirmwareThenSuccessIsReturned) {
    initFirmware();

    L0::Sysman::FirmwareImp *ptestFirmwareImp = new L0::Sysman::FirmwareImp(pSysmanDeviceImp->pFirmwareHandleContext->pOsSysman, mockSupportedFirmwareTypes[1]);
    pSysmanDeviceImp->pFirmwareHandleContext->handleList.push_back(ptestFirmwareImp);

    auto handles = getFirmwareHandles(mockFwHandlesCount);
    uint8_t testImage[ZES_STRING_PROPERTY_SIZE] = {};
    memset(testImage, 0xA, ZES_STRING_PROPERTY_SIZE);
    for (auto handle : handles) {
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesFirmwareFlash(handle, (void *)testImage, ZES_STRING_PROPERTY_SIZE));
    }
    pSysmanDeviceImp->pFirmwareHandleContext->handleList.pop_back();
    delete ptestFirmwareImp;
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

TEST_F(ZesSysmanFirmwareFixture, GivenValidFirmwareHandleFirmwareLibraryCallFailureWhenGettingFirmwarePropertiesThenUnknownIsReturned) {
    initFirmware();

    for (const auto &handle : pSysmanDeviceImp->pFirmwareHandleContext->handleList) {
        delete handle;
    }
    pSysmanDeviceImp->pFirmwareHandleContext->handleList.clear();
    pMockSysfsAccess->scanDirEntriesResult = ZE_RESULT_ERROR_UNKNOWN;
    pMockFwInterface->getFwVersionResult = ZE_RESULT_ERROR_UNINITIALIZED;

    pSysmanDeviceImp->pFirmwareHandleContext->init();
    auto handles = getFirmwareHandles(mockFwHandlesCount);

    zes_firmware_properties_t properties = {};
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesFirmwareGetProperties(handles[0], &properties));
    EXPECT_STREQ("GFX", properties.name);
    EXPECT_STREQ("unknown", properties.version);

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesFirmwareGetProperties(handles[1], &properties));
    EXPECT_STREQ(mockSupportedFirmwareTypes[1].c_str(), properties.name);
    EXPECT_STREQ("unknown", properties.version);

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesFirmwareGetProperties(handles[2], &properties));
    EXPECT_STREQ(mockSupportedFirmwareTypes[2].c_str(), properties.name);
    EXPECT_STREQ("unknown", properties.version);
}

TEST_F(ZesSysmanFirmwareFixture, GivenNewFirmwareContextWithHandleSizeZeroWhenFirmwareEnumerateIsCalledThenSuccessResultIsReturned) {
    uint32_t count = 0;
    L0::Sysman::OsSysman *pOsSysman = pSysmanDeviceImp->pFirmwareHandleContext->pOsSysman;

    for (const auto &handle : pSysmanDeviceImp->pFirmwareHandleContext->handleList) {
        delete handle;
    }
    pSysmanDeviceImp->pFirmwareHandleContext->handleList.clear();
    delete pSysmanDeviceImp->pFirmwareHandleContext;

    pSysmanDeviceImp->pFirmwareHandleContext = new L0::Sysman::FirmwareHandleContext(pOsSysman);
    EXPECT_EQ(0u, pSysmanDeviceImp->pFirmwareHandleContext->handleList.size());
    ze_result_t result = zesDeviceEnumFirmwares(device->toHandle(), &count, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(3u, count);
}

TEST_F(ZesSysmanFirmwareFixture, GivenValidFirmwareHandleWhenGettingFirmwareFlashProgressThenUnsupportedIsReturned) {
    initFirmware();

    L0::Sysman::FirmwareImp *ptestFirmwareImp = new L0::Sysman::FirmwareImp(pSysmanDeviceImp->pFirmwareHandleContext->pOsSysman, mockSupportedFirmwareTypes[1]);
    pSysmanDeviceImp->pFirmwareHandleContext->handleList.push_back(ptestFirmwareImp);

    auto handles = getFirmwareHandles(mockFwHandlesCount);
    uint32_t completionPercent = 0;

    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesFirmwareGetFlashProgress(handles[1], &completionPercent));
}

class ZesFirmwareUninitializedFixture : public SysmanDeviceFixture {

  protected:
    zes_firmware_handle_t hSysmanFirmware = {};
    std::unique_ptr<MockFirmwareInterface> pMockFwInterface;
    L0::Sysman::FirmwareUtil *pFwUtilInterfaceOld = nullptr;
    std::unique_ptr<MockFirmwareFsAccess> pFsAccess;
    L0::Sysman::FsAccessInterface *pFsAccessOriginal = nullptr;

    void SetUp() override {
        SysmanDeviceFixture::SetUp();
        pFsAccessOriginal = pLinuxSysmanImp->pFsAccess;
        pFsAccess = std::make_unique<MockFirmwareFsAccess>();
        pLinuxSysmanImp->pFsAccess = pFsAccess.get();

        pFwUtilInterfaceOld = pLinuxSysmanImp->pFwUtilInterface;
        pLinuxSysmanImp->pFwUtilInterface = nullptr;
    }
    void TearDown() override {
        SysmanDeviceFixture::TearDown();
        pLinuxSysmanImp->pFwUtilInterface = pFwUtilInterfaceOld;
        pLinuxSysmanImp->pFsAccess = pFsAccessOriginal;
    }
};

} // namespace ult
} // namespace Sysman
} // namespace L0
