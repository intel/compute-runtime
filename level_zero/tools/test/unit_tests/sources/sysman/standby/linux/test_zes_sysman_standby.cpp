/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/test/unit_tests/sources/sysman/linux/mock_sysman_fixture.h"

#include "mock_sysfs_standby.h"

extern bool sysmanUltsEnable;

using ::testing::_;
using ::testing::Matcher;

namespace L0 {
namespace ult {

constexpr int standbyModeDefault = 1;
constexpr int standbyModeNever = 0;
constexpr int standbyModeInvalid = 0xff;
constexpr uint32_t mockHandleCount = 1u;
uint32_t mockSubDeviceHandleCount = 0u;
class ZesStandbyFixture : public SysmanDeviceFixture {
  protected:
    std::unique_ptr<Mock<StandbySysfsAccess>> ptestSysfsAccess;
    zes_standby_handle_t hSysmanStandby = {};
    SysfsAccess *pOriginalSysfsAccess = nullptr;
    std::vector<ze_device_handle_t> deviceHandles;

    void SetUp() override {
        if (!sysmanUltsEnable) {
            GTEST_SKIP();
        }
        SysmanDeviceFixture::SetUp();
        ptestSysfsAccess = std::make_unique<NiceMock<Mock<StandbySysfsAccess>>>();
        pOriginalSysfsAccess = pLinuxSysmanImp->pSysfsAccess;
        pLinuxSysmanImp->pSysfsAccess = ptestSysfsAccess.get();
        ptestSysfsAccess->setVal(standbyModeFile, standbyModeDefault);
        ON_CALL(*ptestSysfsAccess.get(), read(_, Matcher<int &>(_)))
            .WillByDefault(::testing::Invoke(ptestSysfsAccess.get(), &Mock<StandbySysfsAccess>::getVal));
        ON_CALL(*ptestSysfsAccess.get(), write(_, Matcher<int>(_)))
            .WillByDefault(::testing::Invoke(ptestSysfsAccess.get(), &Mock<StandbySysfsAccess>::setVal));
        ON_CALL(*ptestSysfsAccess.get(), canRead(_))
            .WillByDefault(::testing::Invoke(ptestSysfsAccess.get(), &Mock<StandbySysfsAccess>::getCanReadStatus));
        for (const auto &handle : pSysmanDeviceImp->pStandbyHandleContext->handleList) {
            delete handle;
        }
        pSysmanDeviceImp->pStandbyHandleContext->handleList.clear();
        uint32_t subDeviceCount = 0;
        // We received a device handle. Check for subdevices in this device
        Device::fromHandle(device->toHandle())->getSubDevices(&subDeviceCount, nullptr);
        if (subDeviceCount == 0) {
            deviceHandles.resize(1, device->toHandle());
        } else {
            deviceHandles.resize(subDeviceCount, nullptr);
            Device::fromHandle(device->toHandle())->getSubDevices(&subDeviceCount, deviceHandles.data());
        }
        pSysmanDeviceImp->pStandbyHandleContext->init(deviceHandles);
    }
    void TearDown() override {
        if (!sysmanUltsEnable) {
            GTEST_SKIP();
        }
        pLinuxSysmanImp->pSysfsAccess = pOriginalSysfsAccess;
        SysmanDeviceFixture::TearDown();
    }

    std::vector<zes_standby_handle_t> get_standby_handles(uint32_t count) {
        std::vector<zes_standby_handle_t> handles(count, nullptr);
        EXPECT_EQ(zesDeviceEnumStandbyDomains(device, &count, handles.data()), ZE_RESULT_SUCCESS);
        return handles;
    }
};

TEST_F(ZesStandbyFixture, GivenComponentCountZeroWhenCallingzesStandbyGetThenNonZeroCountIsReturnedAndVerifyzesStandbyGetCallSucceeds) {
    std::vector<zes_standby_handle_t> standbyHandle = {};
    uint32_t count = 0;

    ze_result_t result = zesDeviceEnumStandbyDomains(device, &count, nullptr);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(count, mockHandleCount);

    uint32_t testCount = count + 1;

    result = zesDeviceEnumStandbyDomains(device, &testCount, nullptr);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(testCount, count);

    standbyHandle.resize(count);
    result = zesDeviceEnumStandbyDomains(device, &count, standbyHandle.data());

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, standbyHandle.data());
    EXPECT_EQ(count, mockHandleCount);

    StandbyImp *ptestStandbyImp = new StandbyImp(pSysmanDeviceImp->pStandbyHandleContext->pOsSysman, device->toHandle());
    count = 0;
    pSysmanDeviceImp->pStandbyHandleContext->handleList.push_back(ptestStandbyImp);
    result = zesDeviceEnumStandbyDomains(device, &count, nullptr);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(count, mockHandleCount + 1);

    testCount = count - 1;

    standbyHandle.resize(testCount);
    result = zesDeviceEnumStandbyDomains(device, &testCount, standbyHandle.data());

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, standbyHandle.data());
    EXPECT_EQ(count, mockHandleCount + 1);

    pSysmanDeviceImp->pStandbyHandleContext->handleList.pop_back();
    delete ptestStandbyImp;
}

TEST_F(ZesStandbyFixture, GivenValidStandbyHandleWhenCallingzesStandbyGetPropertiesThenVerifyzesStandbyGetPropertiesCallSucceeds) {
    zes_standby_properties_t properties = {};
    auto handles = get_standby_handles(mockHandleCount);

    for (auto hSysmanStandby : handles) {
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesStandbyGetProperties(hSysmanStandby, &properties));
        EXPECT_EQ(nullptr, properties.pNext);
        EXPECT_EQ(ZES_STANDBY_TYPE_GLOBAL, properties.type);
        EXPECT_FALSE(properties.onSubdevice);
    }
}

TEST_F(ZesStandbyFixture, GivenValidStandbyHandleWhenCallingzesStandbyGetModeThenVerifyzesStandbyGetModeCallSucceedsForDefaultMode) {
    zes_standby_promo_mode_t mode = {};
    auto handles = get_standby_handles(mockHandleCount);

    for (auto hSysmanStandby : handles) {
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesStandbyGetMode(hSysmanStandby, &mode));
        EXPECT_EQ(ZES_STANDBY_PROMO_MODE_DEFAULT, mode);
    }
}

TEST_F(ZesStandbyFixture, GivenValidStandbyHandleWhenCallingzesStandbyGetModeThenVerifyzesStandbyGetModeCallSucceedsForNeverMode) {
    zes_standby_promo_mode_t mode = {};
    ptestSysfsAccess->setVal(standbyModeFile, standbyModeNever);
    auto handles = get_standby_handles(mockHandleCount);

    for (auto hSysmanStandby : handles) {
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesStandbyGetMode(hSysmanStandby, &mode));
        EXPECT_EQ(ZES_STANDBY_PROMO_MODE_NEVER, mode);
    }
}

TEST_F(ZesStandbyFixture, GivenInvalidStandbyFileWhenReadisCalledThenExpectFailure) {
    zes_standby_promo_mode_t mode = {};
    ON_CALL(*ptestSysfsAccess.get(), read(_, Matcher<int &>(_)))
        .WillByDefault(::testing::Invoke(ptestSysfsAccess.get(), &Mock<StandbySysfsAccess>::setValReturnError));

    auto handles = get_standby_handles(mockHandleCount);

    for (auto hSysmanStandby : handles) {
        EXPECT_NE(ZE_RESULT_SUCCESS, zesStandbyGetMode(hSysmanStandby, &mode));
    }
}

TEST_F(ZesStandbyFixture, GivenValidStandbyHandleWhenCallingzesStandbyGetModeThenVerifyzesStandbyGetModeCallFailsForInvalidMode) {
    zes_standby_promo_mode_t mode = {};
    ptestSysfsAccess->setVal(standbyModeFile, standbyModeInvalid);
    auto handles = get_standby_handles(mockHandleCount);

    for (auto hSysmanStandby : handles) {
        EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, zesStandbyGetMode(hSysmanStandby, &mode));
    }
}

TEST_F(ZesStandbyFixture, GivenValidStandbyHandleWhenCallingzesStandbyGetModeOnUnavailableFileThenVerifyzesStandbyGetModeCallFailsForUnsupportedFeature) {
    zes_standby_promo_mode_t mode = {};
    ptestSysfsAccess->setVal(standbyModeFile, standbyModeInvalid);
    auto handles = get_standby_handles(mockHandleCount);

    ptestSysfsAccess->isStandbyModeFileAvailable = false;

    for (auto hSysmanStandby : handles) {
        EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesStandbyGetMode(hSysmanStandby, &mode));
    }
}

TEST_F(ZesStandbyFixture, GivenValidStandbyHandleWhenCallingzesStandbyGetModeWithInsufficientPermissionsThenVerifyzesStandbyGetModeCallFailsForInsufficientPermissions) {
    zes_standby_promo_mode_t mode = {};
    ptestSysfsAccess->setVal(standbyModeFile, standbyModeInvalid);
    auto handles = get_standby_handles(mockHandleCount);

    ptestSysfsAccess->mockStandbyFileMode &= ~S_IRUSR;

    for (auto hSysmanStandby : handles) {
        EXPECT_EQ(ZE_RESULT_ERROR_INSUFFICIENT_PERMISSIONS, zesStandbyGetMode(hSysmanStandby, &mode));
    }
}

TEST_F(ZesStandbyFixture, GivenValidStandbyHandleWhenCallingzesStandbySetModeThenwithUnwritableFileVerifySysmanzesySetModeCallFailedWithInsufficientPermissions) {
    auto handles = get_standby_handles(mockHandleCount);
    ptestSysfsAccess->mockStandbyFileMode &= ~S_IWUSR;

    for (auto hSysmanStandby : handles) {
        EXPECT_EQ(ZE_RESULT_ERROR_INSUFFICIENT_PERMISSIONS, zesStandbySetMode(hSysmanStandby, ZES_STANDBY_PROMO_MODE_NEVER));
    }
}

TEST_F(ZesStandbyFixture, GivenValidStandbyHandleWhenCallingzesStandbySetModeOnUnavailableFileThenVerifyzesStandbySetModeCallFailsForUnsupportedFeature) {
    auto handles = get_standby_handles(mockHandleCount);

    ptestSysfsAccess->isStandbyModeFileAvailable = false;
    for (auto hSysmanStandby : handles) {
        EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesStandbySetMode(hSysmanStandby, ZES_STANDBY_PROMO_MODE_NEVER));
    }
}

TEST_F(ZesStandbyFixture, GivenValidStandbyHandleWhenCallingzesStandbySetModeNeverThenVerifySysmanzesySetModeCallSucceeds) {
    auto handles = get_standby_handles(mockHandleCount);

    for (auto hSysmanStandby : handles) {
        zes_standby_promo_mode_t mode;
        ptestSysfsAccess->setVal(standbyModeFile, standbyModeDefault);
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesStandbyGetMode(hSysmanStandby, &mode));
        EXPECT_EQ(ZES_STANDBY_PROMO_MODE_DEFAULT, mode);

        EXPECT_EQ(ZE_RESULT_SUCCESS, zesStandbySetMode(hSysmanStandby, ZES_STANDBY_PROMO_MODE_NEVER));

        EXPECT_EQ(ZE_RESULT_SUCCESS, zesStandbyGetMode(hSysmanStandby, &mode));
        EXPECT_EQ(ZES_STANDBY_PROMO_MODE_NEVER, mode);
    }
}

TEST_F(ZesStandbyFixture, GivenValidStandbyHandleWhenCallingzesStandbySetModeDefaultThenVerifySysmanzesySetModeCallSucceeds) {
    auto handles = get_standby_handles(mockHandleCount);

    for (auto hSysmanStandby : handles) {
        zes_standby_promo_mode_t mode;
        ptestSysfsAccess->setVal(standbyModeFile, standbyModeNever);
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesStandbyGetMode(hSysmanStandby, &mode));
        EXPECT_EQ(ZES_STANDBY_PROMO_MODE_NEVER, mode);

        EXPECT_EQ(ZE_RESULT_SUCCESS, zesStandbySetMode(hSysmanStandby, ZES_STANDBY_PROMO_MODE_DEFAULT));

        EXPECT_EQ(ZE_RESULT_SUCCESS, zesStandbyGetMode(hSysmanStandby, &mode));
        EXPECT_EQ(ZES_STANDBY_PROMO_MODE_DEFAULT, mode);
    }
}

TEST_F(ZesStandbyFixture, GivenOnSubdeviceNotSetWhenValidatingosStandbyGetPropertiesThenSuccessIsReturned) {
    zes_standby_properties_t properties = {};
    ze_device_properties_t deviceProperties = {};
    ze_bool_t isSubDevice = deviceProperties.flags & ZE_DEVICE_PROPERTY_FLAG_SUBDEVICE;
    Device::fromHandle(device)->getProperties(&deviceProperties);
    PublicLinuxStandbyImp *pLinuxStandbyImp = new PublicLinuxStandbyImp(pOsSysman, isSubDevice, deviceProperties.subdeviceId);
    EXPECT_EQ(ZE_RESULT_SUCCESS, pLinuxStandbyImp->osStandbyGetProperties(properties));
    EXPECT_EQ(properties.subdeviceId, deviceProperties.subdeviceId);
    EXPECT_EQ(properties.onSubdevice, isSubDevice);
    delete pLinuxStandbyImp;
}

TEST_F(ZesStandbyFixture, GivenValidStandbyHandleWhenCallingzesStandbySetModeDefaultWithLegacyPathThenVerifySysmanzesySetModeCallSucceeds) {
    for (auto handle : pSysmanDeviceImp->pStandbyHandleContext->handleList) {
        delete handle;
    }
    pSysmanDeviceImp->pStandbyHandleContext->handleList.clear();
    ptestSysfsAccess->directoryExistsResult = false;
    pSysmanDeviceImp->pStandbyHandleContext->init(deviceHandles);

    auto handles = get_standby_handles(mockHandleCount);

    for (auto hSysmanStandby : handles) {
        zes_standby_promo_mode_t mode;
        ptestSysfsAccess->setVal(standbyModeFile, standbyModeNever);
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesStandbyGetMode(hSysmanStandby, &mode));
        EXPECT_EQ(ZES_STANDBY_PROMO_MODE_NEVER, mode);

        EXPECT_EQ(ZE_RESULT_SUCCESS, zesStandbySetMode(hSysmanStandby, ZES_STANDBY_PROMO_MODE_DEFAULT));

        EXPECT_EQ(ZE_RESULT_SUCCESS, zesStandbyGetMode(hSysmanStandby, &mode));
        EXPECT_EQ(ZES_STANDBY_PROMO_MODE_DEFAULT, mode);
    }
}

TEST_F(ZesStandbyFixture, GivenValidStandbyHandleWhenCallingzesStandbySetModeNeverWithLegacyPathThenVerifySysmanzesySetModeCallSucceeds) {
    for (auto handle : pSysmanDeviceImp->pStandbyHandleContext->handleList) {
        delete handle;
    }
    pSysmanDeviceImp->pStandbyHandleContext->handleList.clear();
    ptestSysfsAccess->directoryExistsResult = false;
    pSysmanDeviceImp->pStandbyHandleContext->init(deviceHandles);

    auto handles = get_standby_handles(mockHandleCount);

    for (auto hSysmanStandby : handles) {
        zes_standby_promo_mode_t mode;
        ptestSysfsAccess->setVal(standbyModeFile, standbyModeDefault);
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesStandbyGetMode(hSysmanStandby, &mode));
        EXPECT_EQ(ZES_STANDBY_PROMO_MODE_DEFAULT, mode);

        EXPECT_EQ(ZE_RESULT_SUCCESS, zesStandbySetMode(hSysmanStandby, ZES_STANDBY_PROMO_MODE_NEVER));

        EXPECT_EQ(ZE_RESULT_SUCCESS, zesStandbyGetMode(hSysmanStandby, &mode));
        EXPECT_EQ(ZES_STANDBY_PROMO_MODE_NEVER, mode);
    }
}

class ZesStandbyMultiDeviceFixture : public SysmanMultiDeviceFixture {
    std::unique_ptr<Mock<StandbySysfsAccess>> ptestSysfsAccess;
    SysfsAccess *pOriginalSysfsAccess = nullptr;

  protected:
    void SetUp() override {
        if (!sysmanUltsEnable) {
            GTEST_SKIP();
        }
        SysmanMultiDeviceFixture::SetUp();
        mockSubDeviceHandleCount = subDeviceCount;
        ptestSysfsAccess = std::make_unique<NiceMock<Mock<StandbySysfsAccess>>>();
        pOriginalSysfsAccess = pLinuxSysmanImp->pSysfsAccess;
        pLinuxSysmanImp->pSysfsAccess = ptestSysfsAccess.get();
        ptestSysfsAccess->setVal(standbyModeFile, standbyModeDefault);
        ON_CALL(*ptestSysfsAccess.get(), read(_, Matcher<int &>(_)))
            .WillByDefault(::testing::Invoke(ptestSysfsAccess.get(), &Mock<StandbySysfsAccess>::getVal));
        ON_CALL(*ptestSysfsAccess.get(), canRead(_))
            .WillByDefault(::testing::Invoke(ptestSysfsAccess.get(), &Mock<StandbySysfsAccess>::getCanReadStatus));
        for (const auto &handle : pSysmanDeviceImp->pStandbyHandleContext->handleList) {
            delete handle;
        }
        pSysmanDeviceImp->pStandbyHandleContext->handleList.clear();
        uint32_t subDeviceCount = 0;
        std::vector<ze_device_handle_t> deviceHandles;
        Device::fromHandle(device->toHandle())->getSubDevices(&subDeviceCount, nullptr);
        if (subDeviceCount == 0) {
            deviceHandles.resize(1, device->toHandle());
        } else {
            deviceHandles.resize(subDeviceCount, nullptr);
            Device::fromHandle(device->toHandle())->getSubDevices(&subDeviceCount, deviceHandles.data());
        }
        pSysmanDeviceImp->pStandbyHandleContext->init(deviceHandles);
    }
    void TearDown() override {
        if (!sysmanUltsEnable) {
            GTEST_SKIP();
        }
        pLinuxSysmanImp->pSysfsAccess = pOriginalSysfsAccess;
        SysmanMultiDeviceFixture::TearDown();
    }

    std::vector<zes_standby_handle_t> get_standby_handles(uint32_t count) {
        std::vector<zes_standby_handle_t> handles(count, nullptr);
        EXPECT_EQ(zesDeviceEnumStandbyDomains(device, &count, handles.data()), ZE_RESULT_SUCCESS);
        return handles;
    }
};

TEST_F(ZesStandbyMultiDeviceFixture, GivenComponentCountZeroWhenCallingzesStandbyGetThenNonZeroCountIsReturnedAndVerifyzesStandbyGetCallSucceeds) {
    std::vector<zes_standby_handle_t> standbyHandle = {};
    uint32_t count = 0;

    ze_result_t result = zesDeviceEnumStandbyDomains(device, &count, nullptr);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(count, mockSubDeviceHandleCount);

    uint32_t testCount = count + 1;

    result = zesDeviceEnumStandbyDomains(device, &testCount, nullptr);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(testCount, count);

    standbyHandle.resize(count);
    result = zesDeviceEnumStandbyDomains(device, &count, standbyHandle.data());

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, standbyHandle.data());
    EXPECT_EQ(count, mockSubDeviceHandleCount);
}

TEST_F(ZesStandbyMultiDeviceFixture, GivenOnSubdeviceNotSetWhenValidatingosStandbyGetPropertiesThenSuccessIsReturned) {
    zes_standby_properties_t properties = {};
    ze_device_properties_t deviceProperties = {};
    ze_bool_t isSubDevice = deviceProperties.flags & ZE_DEVICE_PROPERTY_FLAG_SUBDEVICE;
    Device::fromHandle(device)->getProperties(&deviceProperties);
    PublicLinuxStandbyImp *pLinuxStandbyImp = new PublicLinuxStandbyImp(pOsSysman, isSubDevice, deviceProperties.subdeviceId);
    EXPECT_EQ(ZE_RESULT_SUCCESS, pLinuxStandbyImp->osStandbyGetProperties(properties));
    EXPECT_EQ(properties.subdeviceId, deviceProperties.subdeviceId);
    EXPECT_EQ(properties.onSubdevice, isSubDevice);
    delete pLinuxStandbyImp;
}

} // namespace ult
} // namespace L0
