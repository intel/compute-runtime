/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/test/unit_tests/sources/sysman/firmware/linux/mock_zes_sysman_firmware.h"

extern bool sysmanUltsEnable;

using ::testing::_;
namespace L0 {
namespace ult {

class ZesFirmwareFixture : public SysmanDeviceFixture {

  protected:
    zes_firmware_handle_t hSysmanFirmware = {};
    std::unique_ptr<Mock<FirmwareInterface>> pMockFwInterface;
    FirmwareUtil *pFwUtilInterfaceOld = nullptr;
    std::unique_ptr<Mock<FirmwareFsAccess>> pFsAccess;
    FsAccess *pFsAccessOriginal = nullptr;

    void SetUp() override {
        if (!sysmanUltsEnable) {
            GTEST_SKIP();
        }
        SysmanDeviceFixture::SetUp();
        pFsAccessOriginal = pLinuxSysmanImp->pFsAccess;
        pFsAccess = std::make_unique<NiceMock<Mock<FirmwareFsAccess>>>();
        pLinuxSysmanImp->pFsAccess = pFsAccess.get();

        pFwUtilInterfaceOld = pLinuxSysmanImp->pFwUtilInterface;
        pMockFwInterface = std::make_unique<NiceMock<Mock<FirmwareInterface>>>();
        pLinuxSysmanImp->pFwUtilInterface = pMockFwInterface.get();
        ON_CALL(*pMockFwInterface.get(), fwDeviceInit())
            .WillByDefault(::testing::Return(ZE_RESULT_SUCCESS));
        ON_CALL(*pMockFwInterface.get(), getFirstDevice(_))
            .WillByDefault(::testing::Return(ZE_RESULT_SUCCESS));
        ON_CALL(*pMockFwInterface.get(), getDeviceSupportedFwTypes(_))
            .WillByDefault(::testing::SetArgReferee<0>(mockSupportedFwTypes));
        ON_CALL(*pFsAccess.get(), read(_, _))
            .WillByDefault(::testing::Invoke(pFsAccess.get(), &Mock<FirmwareFsAccess>::readValSuccess));
        for (const auto &handle : pSysmanDeviceImp->pFirmwareHandleContext->handleList) {
            delete handle;
        }
        pSysmanDeviceImp->pFirmwareHandleContext->handleList.clear();
        pSysmanDeviceImp->pFirmwareHandleContext->init();
    }
    void TearDown() override {
        if (!sysmanUltsEnable) {
            GTEST_SKIP();
        }
        SysmanDeviceFixture::TearDown();
        pLinuxSysmanImp->pFwUtilInterface = pFwUtilInterfaceOld;
        pLinuxSysmanImp->pFsAccess = pFsAccessOriginal;
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

    FirmwareImp *ptestFirmwareImp = new FirmwareImp(pSysmanDeviceImp->pFirmwareHandleContext->pOsSysman, mockSupportedFwTypes[0]);
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
    FirmwareImp *ptestFirmwareImp = new FirmwareImp(pSysmanDeviceImp->pFirmwareHandleContext->pOsSysman, mockSupportedFwTypes[0]);
    pSysmanDeviceImp->pFirmwareHandleContext->handleList.push_back(ptestFirmwareImp);

    ON_CALL(*pMockFwInterface.get(), getFwVersion(_, _))
        .WillByDefault(::testing::Invoke(pMockFwInterface.get(), &Mock<FirmwareInterface>::mockGetFwVersion));

    auto handles = get_firmware_handles(mockHandleCount);

    zes_firmware_properties_t properties = {};
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesFirmwareGetProperties(handles[0], &properties));
    EXPECT_STREQ(mockSupportedFwTypes[0].c_str(), properties.name);
    EXPECT_STREQ(mockFwVersion.c_str(), properties.version);

    pSysmanDeviceImp->pFirmwareHandleContext->handleList.pop_back();
    delete ptestFirmwareImp;
}

TEST_F(ZesFirmwareFixture, GivenValidFirmwareHandleWhenGettingOpromPropertiesThenVersionIsReturned) {
    FirmwareImp *ptestFirmwareImp = new FirmwareImp(pSysmanDeviceImp->pFirmwareHandleContext->pOsSysman, mockSupportedFwTypes[1]);
    pSysmanDeviceImp->pFirmwareHandleContext->handleList.push_back(ptestFirmwareImp);

    ON_CALL(*pMockFwInterface.get(), getFwVersion(_, _))
        .WillByDefault(::testing::Invoke(pMockFwInterface.get(), &Mock<FirmwareInterface>::mockGetFwVersion));

    auto handles = get_firmware_handles(mockHandleCount);

    zes_firmware_properties_t properties = {};
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesFirmwareGetProperties(handles[1], &properties));
    EXPECT_STREQ(mockSupportedFwTypes[1].c_str(), properties.name);
    EXPECT_STREQ(mockOpromVersion.c_str(), properties.version);

    pSysmanDeviceImp->pFirmwareHandleContext->handleList.pop_back();
    delete ptestFirmwareImp;
}

TEST_F(ZesFirmwareFixture, GivenFailedFirmwareInitializationWhenInitializingFirmwareContextThenexpectNoHandles) {
    for (const auto &handle : pSysmanDeviceImp->pFirmwareHandleContext->handleList) {
        delete handle;
    }
    pSysmanDeviceImp->pFirmwareHandleContext->handleList.clear();
    ON_CALL(*pMockFwInterface.get(), fwDeviceInit())
        .WillByDefault(::testing::Return(ZE_RESULT_ERROR_UNKNOWN));

    pSysmanDeviceImp->pFirmwareHandleContext->init();

    EXPECT_EQ(0u, pSysmanDeviceImp->pFirmwareHandleContext->handleList.size());
}

TEST_F(ZesFirmwareFixture, GivenRepeatedFWTypesWhenInitializingFirmwareContextThenexpectNoHandles) {
    for (const auto &handle : pSysmanDeviceImp->pFirmwareHandleContext->handleList) {
        delete handle;
    }
    pSysmanDeviceImp->pFirmwareHandleContext->handleList.clear();
    ON_CALL(*pFsAccess.get(), read(_, _))
        .WillByDefault(::testing::Invoke(pFsAccess.get(), &Mock<FirmwareFsAccess>::readMtdValSuccess));

    pSysmanDeviceImp->pFirmwareHandleContext->init();

    EXPECT_EQ(1u, pSysmanDeviceImp->pFirmwareHandleContext->handleList.size());
}

TEST_F(ZesFirmwareFixture, GivenValidFirmwareHandleWhenFlashingGscFirmwareThenSuccessIsReturned) {
    FirmwareImp *ptestFirmwareImp = new FirmwareImp(pSysmanDeviceImp->pFirmwareHandleContext->pOsSysman, mockSupportedFwTypes[0]);
    pSysmanDeviceImp->pFirmwareHandleContext->handleList.push_back(ptestFirmwareImp);

    ON_CALL(*pMockFwInterface.get(), flashFirmware(_, _, _))
        .WillByDefault(::testing::Return(ZE_RESULT_SUCCESS));

    auto handles = get_firmware_handles(mockHandleCount);
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
    FirmwareImp *ptestFirmwareImp = new FirmwareImp(pSysmanDeviceImp->pFirmwareHandleContext->pOsSysman, mockUnsupportedFwTypes[0]);
    pSysmanDeviceImp->pFirmwareHandleContext->handleList.push_back(ptestFirmwareImp);

    ON_CALL(*pMockFwInterface.get(), flashFirmware(_, _, _))
        .WillByDefault(::testing::Return(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE));

    uint8_t testImage[ZES_STRING_PROPERTY_SIZE] = {};
    memset(testImage, 0xA, ZES_STRING_PROPERTY_SIZE);
    auto handle = pSysmanDeviceImp->pFirmwareHandleContext->handleList[0]->toHandle();
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesFirmwareFlash(handle, (void *)testImage, ZES_STRING_PROPERTY_SIZE));

    pSysmanDeviceImp->pFirmwareHandleContext->handleList.pop_back();
    delete ptestFirmwareImp;
}

TEST_F(ZesFirmwareFixture, GivenFirmwareInitializationFailureThenCreateHandleMustFail) {
    for (const auto &handle : pSysmanDeviceImp->pFirmwareHandleContext->handleList) {
        delete handle;
    }
    pSysmanDeviceImp->pFirmwareHandleContext->handleList.clear();
    ON_CALL(*pMockFwInterface.get(), fwDeviceInit())
        .WillByDefault(::testing::Return(ZE_RESULT_ERROR_UNKNOWN));
    pSysmanDeviceImp->pFirmwareHandleContext->init();
    EXPECT_EQ(0u, pSysmanDeviceImp->pFirmwareHandleContext->handleList.size());
}

TEST_F(ZesFirmwareFixture, GivenValidFirmwareHandleFirmwareLibraryCallFailureWhenGettingFirmwarePropertiesThenUnknownIsReturned) {
    for (const auto &handle : pSysmanDeviceImp->pFirmwareHandleContext->handleList) {
        delete handle;
    }
    pSysmanDeviceImp->pFirmwareHandleContext->handleList.clear();
    ON_CALL(*pMockFwInterface.get(), getFwVersion(_, _))
        .WillByDefault(::testing::Return(ZE_RESULT_ERROR_UNINITIALIZED));

    pSysmanDeviceImp->pFirmwareHandleContext->init();
    auto handles = get_firmware_handles(mockHandleCount);

    zes_firmware_properties_t properties = {};
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesFirmwareGetProperties(handles[0], &properties));
    EXPECT_STREQ(mockSupportedFwTypes[0].c_str(), properties.name);
    EXPECT_STREQ("unknown", properties.version);

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesFirmwareGetProperties(handles[1], &properties));
    EXPECT_STREQ(mockSupportedFwTypes[1].c_str(), properties.name);
    EXPECT_STREQ("unknown", properties.version);
}

class ZesFirmwareUninitializedFixture : public SysmanDeviceFixture {

  protected:
    zes_firmware_handle_t hSysmanFirmware = {};
    std::unique_ptr<Mock<FirmwareInterface>> pMockFwInterface;
    FirmwareUtil *pFwUtilInterfaceOld = nullptr;
    std::unique_ptr<Mock<FirmwareFsAccess>> pFsAccess;
    FsAccess *pFsAccessOriginal = nullptr;

    void SetUp() override {
        if (!sysmanUltsEnable) {
            GTEST_SKIP();
        }
        SysmanDeviceFixture::SetUp();
        pFsAccessOriginal = pLinuxSysmanImp->pFsAccess;
        pFsAccess = std::make_unique<NiceMock<Mock<FirmwareFsAccess>>>();
        pLinuxSysmanImp->pFsAccess = pFsAccess.get();

        pFwUtilInterfaceOld = pLinuxSysmanImp->pFwUtilInterface;
        pLinuxSysmanImp->pFwUtilInterface = nullptr;
        ON_CALL(*pFsAccess.get(), read(_, _))
            .WillByDefault(::testing::Invoke(pFsAccess.get(), &Mock<FirmwareFsAccess>::readValSuccess));
    }
    void TearDown() override {
        if (!sysmanUltsEnable) {
            GTEST_SKIP();
        }
        SysmanDeviceFixture::TearDown();
        pLinuxSysmanImp->pFwUtilInterface = pFwUtilInterfaceOld;
        pLinuxSysmanImp->pFsAccess = pFsAccessOriginal;
    }
};

} // namespace ult
} // namespace L0
