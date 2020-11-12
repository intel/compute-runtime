/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/test/unit_tests/sources/sysman/firmware/linux/mock_zes_sysman_firmware.h"
using ::testing::_;
namespace L0 {
namespace ult {

class ZesFirmwareFixture : public SysmanDeviceFixture {

  protected:
    zes_firmware_handle_t hSysmanFirmware = {};
    std::unique_ptr<Mock<FirmwareInterface>> pMockFwInterface;
    FirmwareUtil *pFwUtilInterfaceOld = nullptr;

    void SetUp() override {
        SysmanDeviceFixture::SetUp();
        pFwUtilInterfaceOld = pLinuxSysmanImp->pFwUtilInterface;
        pMockFwInterface = std::make_unique<NiceMock<Mock<FirmwareInterface>>>();
        pLinuxSysmanImp->pFwUtilInterface = pMockFwInterface.get();
        ON_CALL(*pMockFwInterface.get(), fwDeviceInit())
            .WillByDefault(::testing::Invoke(pMockFwInterface.get(), &Mock<FirmwareInterface>::mockFwDeviceInit));
        ON_CALL(*pMockFwInterface.get(), fwGetVersion(_))
            .WillByDefault(::testing::Invoke(pMockFwInterface.get(), &Mock<FirmwareInterface>::mockFwGetVersion));
        ON_CALL(*pMockFwInterface.get(), getFirstDevice(_))
            .WillByDefault(::testing::Invoke(pMockFwInterface.get(), &Mock<FirmwareInterface>::mockGetFirstDevice));
        for (const auto &handle : pSysmanDeviceImp->pFirmwareHandleContext->handleList) {
            delete handle;
        }
        pSysmanDeviceImp->pFirmwareHandleContext->handleList.clear();
        pSysmanDeviceImp->pFirmwareHandleContext->init();
    }
    void TearDown() override {
        SysmanDeviceFixture::TearDown();
        pLinuxSysmanImp->pFwUtilInterface = pFwUtilInterfaceOld;
    }

    std::vector<zes_firmware_handle_t> get_firmware_handles(uint32_t count) {
        std::vector<zes_firmware_handle_t> handles(count, nullptr);
        EXPECT_EQ(zesDeviceEnumFirmwares(device->toHandle(), &count, handles.data()), ZE_RESULT_SUCCESS);
        return handles;
    }
};

TEST_F(ZesFirmwareFixture, GivenComponentCountZeroWhenCallingzesFirmwareGetThenZeroCountIsReturnedAndVerifyzesFirmwareGetCallSucceeds) {
    std::vector<zes_firmware_handle_t> firmwareHandle{};
    uint32_t count = 0;

    ze_result_t result = zesDeviceEnumFirmwares(device->toHandle(), &count, nullptr);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(count, mockHandleCount);

    uint32_t testCount = count + 1;

    result = zesDeviceEnumFirmwares(device->toHandle(), &testCount, nullptr);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(testCount, count);

    firmwareHandle.resize(count);
    result = zesDeviceEnumFirmwares(device->toHandle(), &count, firmwareHandle.data());

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(count, mockHandleCount);

    FirmwareImp *ptestFirmwareImp = new FirmwareImp(pSysmanDeviceImp->pFirmwareHandleContext->pOsSysman);
    pSysmanDeviceImp->pFirmwareHandleContext->handleList.push_back(ptestFirmwareImp);
    result = zesDeviceEnumFirmwares(device->toHandle(), &count, nullptr);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(count, mockHandleCount);

    testCount = count;

    firmwareHandle.resize(testCount);
    result = zesDeviceEnumFirmwares(device->toHandle(), &testCount, firmwareHandle.data());

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, firmwareHandle.data());
    EXPECT_EQ(testCount, mockHandleCount);

    pSysmanDeviceImp->pFirmwareHandleContext->handleList.pop_back();
    delete ptestFirmwareImp;
}

TEST_F(ZesFirmwareFixture, GivenValidFirmwareHandleWhenGettingFirmwarePropertiesThenVersionIsReturned) {
    FirmwareImp *ptestFirmwareImp = new FirmwareImp(pSysmanDeviceImp->pFirmwareHandleContext->pOsSysman);
    pSysmanDeviceImp->pFirmwareHandleContext->handleList.push_back(ptestFirmwareImp);

    auto handles = get_firmware_handles(mockHandleCount);

    for (auto handle : handles) {
        zes_firmware_properties_t properties = {};
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesFirmwareGetProperties(handle, &properties));
        EXPECT_STREQ(mockFwVersion.c_str(), properties.name);
        EXPECT_STREQ(mockFwVersion.c_str(), properties.version);
    }
    pSysmanDeviceImp->pFirmwareHandleContext->handleList.pop_back();
    delete ptestFirmwareImp;
}
TEST_F(ZesFirmwareFixture, GivenValidFirmwareHandleWhenGettingFirmwarePropertiesThenUnknownIsReturned) {
    for (const auto &handle : pSysmanDeviceImp->pFirmwareHandleContext->handleList) {
        delete handle;
    }
    pSysmanDeviceImp->pFirmwareHandleContext->handleList.clear();
    ON_CALL(*pMockFwInterface.get(), fwDeviceInit())
        .WillByDefault(::testing::Invoke(pMockFwInterface.get(), &Mock<FirmwareInterface>::mockFwDeviceInitFail));
    FirmwareImp *ptestFirmwareImp = new FirmwareImp(pSysmanDeviceImp->pFirmwareHandleContext->pOsSysman);
    pSysmanDeviceImp->pFirmwareHandleContext->handleList.push_back(ptestFirmwareImp);

    auto handles = get_firmware_handles(mockHandleCount);

    for (auto handle : handles) {
        zes_firmware_properties_t properties = {};
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesFirmwareGetProperties(handle, &properties));
        EXPECT_STREQ("Unknown", properties.name);
        EXPECT_STREQ("Unknown", properties.version);
    }
    pSysmanDeviceImp->pFirmwareHandleContext->handleList.pop_back();
    delete ptestFirmwareImp;
}
} // namespace ult
} // namespace L0
