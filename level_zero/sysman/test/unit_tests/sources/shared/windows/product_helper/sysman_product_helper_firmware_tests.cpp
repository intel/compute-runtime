/*
 * Copyright (C) 2023-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/sysman/test/unit_tests/sources/firmware/windows/mock_zes_sysman_firmware.h"

#include <algorithm>

namespace L0 {
namespace Sysman {
namespace ult {

HWTEST2_F(ZesFirmwareFixture, GivenComponentCountZeroWhenCallingZesFirmwareGetThenZeroCountIsReturnedAndVerifyZesFirmwareGetCallSucceeds, IsNotBMG) {
    std::vector<zes_firmware_handle_t> firmwareHandle{};
    uint32_t count = 0;

    ze_result_t result = zesDeviceEnumFirmwares(pSysmanDevice->toHandle(), &count, nullptr);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(count, mockHandleCount);

    uint32_t testCount = count + 1;

    result = zesDeviceEnumFirmwares(pSysmanDevice->toHandle(), &testCount, nullptr);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(testCount, count);

    firmwareHandle.resize(count);
    result = zesDeviceEnumFirmwares(pSysmanDevice->toHandle(), &count, firmwareHandle.data());

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(count, mockHandleCount);

    L0::Sysman::FirmwareImp *ptestFirmwareImp = new L0::Sysman::FirmwareImp(pSysmanDeviceImp->pFirmwareHandleContext->pOsSysman, mockSupportedFwTypes[0]);
    pSysmanDeviceImp->pFirmwareHandleContext->handleList.push_back(ptestFirmwareImp);
    result = zesDeviceEnumFirmwares(pSysmanDevice->toHandle(), &count, nullptr);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(count, mockHandleCount);

    testCount = count;

    firmwareHandle.resize(testCount);
    result = zesDeviceEnumFirmwares(pSysmanDevice->toHandle(), &testCount, firmwareHandle.data());

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, firmwareHandle.data());
    EXPECT_EQ(testCount, mockHandleCount);

    pSysmanDeviceImp->pFirmwareHandleContext->handleList.pop_back();
    delete ptestFirmwareImp;
}

HWTEST2_F(ZesFirmwareFixture, GivenComponentCountZeroAndLateBindingIsSupportedThenWhenCallingZesFirmwareGetProperCountIsReturned, IsBMG) {
    constexpr uint32_t mockFwHandlesCount = 4;
    std::vector<zes_firmware_handle_t> firmwareHandle{};
    uint32_t count = 0;

    ze_result_t result = zesDeviceEnumFirmwares(pSysmanDevice->toHandle(), &count, nullptr);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(count, mockFwHandlesCount);

    uint32_t testCount = count + 1;

    result = zesDeviceEnumFirmwares(pSysmanDevice->toHandle(), &testCount, nullptr);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(testCount, count);

    firmwareHandle.resize(count);
    result = zesDeviceEnumFirmwares(pSysmanDevice->toHandle(), &count, firmwareHandle.data());

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(count, mockFwHandlesCount);
}

HWTEST2_F(ZesFirmwareFixture, GivenValidLateBindingFirmwareHandleWhenFlashingFirmwareThenSuccessIsReturned, IsBMG) {
    constexpr uint32_t mockFwHandlesCount = 4;
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

HWTEST2_F(ZesFirmwareFixture, GivenValidLateBindingFirmwareHandleWhenGettingFirmwarePropertiesThenValidVersionIsReturned, IsBMG) {

    constexpr uint32_t mockFwHandlesCount = 4;
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
