/*
 * Copyright (C) 2023-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/sysman/test/unit_tests/sources/firmware/windows/mock_zes_sysman_firmware.h"

namespace L0 {
namespace Sysman {
namespace ult {

TEST_F(ZesFirmwareFixture, GivenValidFirmwareHandleWhenGettingFirmwarePropertiesThenVersionIsReturned) {
    initFirmware();

    L0::Sysman::FirmwareImp *ptestFirmwareImp = new L0::Sysman::FirmwareImp(pSysmanDeviceImp->pFirmwareHandleContext->pOsSysman, mockSupportedFwTypes[0]);
    pSysmanDeviceImp->pFirmwareHandleContext->handleList.push_back(ptestFirmwareImp);

    auto handles = getFirmwareHandles(mockHandleCount);

    zes_firmware_properties_t properties = {};
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesFirmwareGetProperties(handles[0], &properties));
    EXPECT_STREQ("GFX", properties.name);
    EXPECT_STREQ(mockFwVersion.c_str(), properties.version);

    pSysmanDeviceImp->pFirmwareHandleContext->handleList.pop_back();
    delete ptestFirmwareImp;
}

TEST_F(ZesFirmwareFixture, GivenValidFirmwareHandleWhenGettingOptionRomPropertiesThenVersionIsReturned) {
    initFirmware();

    L0::Sysman::FirmwareImp *pTestFirmwareImp = new L0::Sysman::FirmwareImp(pSysmanDeviceImp->pFirmwareHandleContext->pOsSysman, mockSupportedFwTypes[1]);
    pSysmanDeviceImp->pFirmwareHandleContext->handleList.push_back(pTestFirmwareImp);

    auto handles = getFirmwareHandles(mockHandleCount);

    zes_firmware_properties_t properties = {};
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesFirmwareGetProperties(handles[1], &properties));
    EXPECT_STREQ(mockSupportedFwTypes[1].c_str(), properties.name);
    EXPECT_STREQ(mockOpromVersion.c_str(), properties.version);

    pSysmanDeviceImp->pFirmwareHandleContext->handleList.pop_back();
    delete pTestFirmwareImp;
}

TEST_F(ZesFirmwareFixture, GivenValidFirmwareHandleWhenGettingOpromPropertiesThenVersionIsReturned) {
    initFirmware();

    L0::Sysman::FirmwareImp *ptestFirmwareImp = new L0::Sysman::FirmwareImp(pSysmanDeviceImp->pFirmwareHandleContext->pOsSysman, mockSupportedFwTypes[1]);
    pSysmanDeviceImp->pFirmwareHandleContext->handleList.push_back(ptestFirmwareImp);

    auto handles = getFirmwareHandles(mockHandleCount);

    zes_firmware_properties_t properties = {};
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesFirmwareGetProperties(handles[1], &properties));
    EXPECT_STREQ(mockSupportedFwTypes[1].c_str(), properties.name);
    EXPECT_STREQ(mockOpromVersion.c_str(), properties.version);

    pSysmanDeviceImp->pFirmwareHandleContext->handleList.pop_back();
    delete ptestFirmwareImp;
}

TEST_F(ZesFirmwareFixture, GivenValidFirmwareHandleWhenFlashingGscFirmwareThenSuccessIsReturned) {
    initFirmware();

    L0::Sysman::FirmwareImp *ptestFirmwareImp = new L0::Sysman::FirmwareImp(pSysmanDeviceImp->pFirmwareHandleContext->pOsSysman, mockSupportedFwTypes[0]);
    pSysmanDeviceImp->pFirmwareHandleContext->handleList.push_back(ptestFirmwareImp);

    auto handles = getFirmwareHandles(mockHandleCount);
    uint8_t testImage[ZES_STRING_PROPERTY_SIZE] = {};
    memset(testImage, 0xA, ZES_STRING_PROPERTY_SIZE);
    for (auto handle : handles) {
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesFirmwareFlash(handle, (void *)testImage, ZES_STRING_PROPERTY_SIZE));
    }
    pSysmanDeviceImp->pFirmwareHandleContext->handleList.pop_back();
    delete ptestFirmwareImp;
}

TEST_F(ZesFirmwareFixture, GivenValidFirmwareHandleWhenFlashingUnkownFirmwareThenFailureIsReturned) {
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

TEST_F(ZesFirmwareFixture, GivenNewFirmwareContextWithHandleSizeZeroWhenFirmwareEnumerateIsCalledThenSuccessResultIsReturned) {
    uint32_t count = 0;
    L0::Sysman::OsSysman *pOsSysman = pSysmanDeviceImp->pFirmwareHandleContext->pOsSysman;

    for (const auto &handle : pSysmanDeviceImp->pFirmwareHandleContext->handleList) {
        delete handle;
    }
    pSysmanDeviceImp->pFirmwareHandleContext->handleList.clear();
    delete pSysmanDeviceImp->pFirmwareHandleContext;

    pSysmanDeviceImp->pFirmwareHandleContext = new L0::Sysman::FirmwareHandleContext(pOsSysman);
    EXPECT_EQ(0u, pSysmanDeviceImp->pFirmwareHandleContext->handleList.size());
    ze_result_t result = zesDeviceEnumFirmwares(pSysmanDevice->toHandle(), &count, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(true, (count > 0));
}

TEST_F(ZesFirmwareFixture, GivenValidFirmwareHandleFirmwareLibraryCallFailureWhenGettingFirmwarePropertiesThenUnknownIsReturned) {
    for (const auto &handle : pSysmanDeviceImp->pFirmwareHandleContext->handleList) {
        delete handle;
    }
    pSysmanDeviceImp->pFirmwareHandleContext->handleList.clear();
    pMockFwInterface->getFwVersionResult = ZE_RESULT_ERROR_UNINITIALIZED;

    pSysmanDeviceImp->pFirmwareHandleContext->init();
    auto handles = getFirmwareHandles(mockHandleCount);

    zes_firmware_properties_t properties = {};
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesFirmwareGetProperties(handles[0], &properties));
    EXPECT_STREQ("GFX", properties.name);
    EXPECT_STREQ("unknown", properties.version);

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesFirmwareGetProperties(handles[1], &properties));
    EXPECT_STREQ(mockSupportedFwTypes[1].c_str(), properties.name);
    EXPECT_STREQ("unknown", properties.version);
}

TEST_F(ZesFirmwareFixture, GivenValidFirmwareHandleWhenGettingFirmwareFlashProgressThenSuccessIsReturned) {
    initFirmware();

    L0::Sysman::FirmwareImp *ptestFirmwareImp = new L0::Sysman::FirmwareImp(pSysmanDeviceImp->pFirmwareHandleContext->pOsSysman, mockSupportedFwTypes[0]);
    pSysmanDeviceImp->pFirmwareHandleContext->handleList.push_back(ptestFirmwareImp);
    auto handles = getFirmwareHandles(mockHandleCount);

    uint32_t completionPercent = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesFirmwareGetFlashProgress(handles[1], &completionPercent));
}

TEST_F(ZesFirmwareFixture, GivenValidFirmwareHandleWhenGettingFirmwareSecurityVersionThenUnSupportedIsReturned) {
    initFirmware();

    L0::Sysman::FirmwareImp *ptestFirmwareImp = new L0::Sysman::FirmwareImp(pSysmanDeviceImp->pFirmwareHandleContext->pOsSysman, mockSupportedFwTypes[0]);
    pSysmanDeviceImp->pFirmwareHandleContext->handleList.push_back(ptestFirmwareImp);
    auto handles = getFirmwareHandles(mockHandleCount);

    char pVersion[100];
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesFirmwareGetSecurityVersionExp(handles[1], pVersion));
}

TEST_F(ZesFirmwareFixture, GivenValidFirmwareHandleWhenSettingFirmwareSecurityVersionThenUnSupportedIsReturned) {
    initFirmware();

    L0::Sysman::FirmwareImp *ptestFirmwareImp = new L0::Sysman::FirmwareImp(pSysmanDeviceImp->pFirmwareHandleContext->pOsSysman, mockSupportedFwTypes[0]);
    pSysmanDeviceImp->pFirmwareHandleContext->handleList.push_back(ptestFirmwareImp);
    auto handles = getFirmwareHandles(mockHandleCount);

    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesFirmwareSetSecurityVersionExp(handles[1]));
}

TEST_F(ZesFirmwareFixture, GivenValidFirmwareHandleWhenGettingFirmwareConsoleLogsThenUnSupportedIsReturned) {
    initFirmware();

    L0::Sysman::FirmwareImp *ptestFirmwareImp = new L0::Sysman::FirmwareImp(pSysmanDeviceImp->pFirmwareHandleContext->pOsSysman, mockSupportedFwTypes[0]);
    pSysmanDeviceImp->pFirmwareHandleContext->handleList.push_back(ptestFirmwareImp);
    auto handles = getFirmwareHandles(mockHandleCount);

    size_t pSize;
    char *pFirmwareLog = nullptr;
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesFirmwareGetConsoleLogs(handles[1], &pSize, pFirmwareLog));
}

} // namespace ult
} // namespace Sysman
} // namespace L0
