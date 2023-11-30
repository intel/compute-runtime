/*
 * Copyright (C) 2020-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/test/unit_tests/sources/sysman/linux/mock_sysman_fixture.h"

#include "mock_sysfs_standby.h"

extern bool sysmanUltsEnable;

namespace L0 {
namespace ult {

constexpr int standbyModeDefault = 1;
constexpr int standbyModeNever = 0;
constexpr int standbyModeInvalid = 0xff;
constexpr uint32_t mockHandleCount = 1u;
uint32_t mockSubDeviceHandleCount = 0u;
class ZesStandbyFixture : public SysmanDeviceFixture {
  protected:
    std::unique_ptr<MockStandbySysfsAccess> ptestSysfsAccess;
    zes_standby_handle_t hSysmanStandby = {};
    SysfsAccess *pOriginalSysfsAccess = nullptr;
    std::vector<ze_device_handle_t> deviceHandles;

    void SetUp() override {
        if (!sysmanUltsEnable) {
            GTEST_SKIP();
        }
        SysmanDeviceFixture::SetUp();
        ptestSysfsAccess = std::make_unique<MockStandbySysfsAccess>();
        pOriginalSysfsAccess = pLinuxSysmanImp->pSysfsAccess;
        pLinuxSysmanImp->pSysfsAccess = ptestSysfsAccess.get();
        ptestSysfsAccess->setVal(standbyModeFile, standbyModeDefault);
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
    }
    void TearDown() override {
        if (!sysmanUltsEnable) {
            GTEST_SKIP();
        }
        pLinuxSysmanImp->pSysfsAccess = pOriginalSysfsAccess;
        SysmanDeviceFixture::TearDown();
    }

    std::vector<zes_standby_handle_t> getStandbyHandles(uint32_t count) {
        std::vector<zes_standby_handle_t> handles(count, nullptr);
        EXPECT_EQ(zesDeviceEnumStandbyDomains(device, &count, handles.data()), ZE_RESULT_SUCCESS);
        return handles;
    }
};

TEST_F(ZesStandbyFixture, GivenStandbyModeFilesNotAvailableWhenCallingEnumerateThenSuccessResultAndZeroCountIsReturned) {
    uint32_t count = 0;
    ptestSysfsAccess->isStandbyModeFileAvailable = false;
    ze_result_t result = zesDeviceEnumStandbyDomains(device, &count, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(count, 0u);
}
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

    std::unique_ptr<StandbyImp> ptestStandbyImp = std::make_unique<StandbyImp>(pSysmanDeviceImp->pStandbyHandleContext->pOsSysman, device->toHandle());
    count = 0;
    pSysmanDeviceImp->pStandbyHandleContext->handleList.push_back(std::move(ptestStandbyImp));
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
}

TEST_F(ZesStandbyFixture, GivenValidStandbyHandleWhenCallingzesStandbyGetPropertiesThenVerifyzesStandbyGetPropertiesCallSucceeds) {
    zes_standby_properties_t properties = {};
    auto handles = getStandbyHandles(mockHandleCount);

    for (auto hSysmanStandby : handles) {
        ASSERT_NE(nullptr, hSysmanStandby);
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesStandbyGetProperties(hSysmanStandby, &properties));
        EXPECT_EQ(nullptr, properties.pNext);
        EXPECT_EQ(ZES_STANDBY_TYPE_GLOBAL, properties.type);
        EXPECT_FALSE(properties.onSubdevice);
    }
}

TEST_F(ZesStandbyFixture, GivenValidStandbyHandleWhenCallingzesStandbyGetModeThenVerifyzesStandbyGetModeCallSucceedsForDefaultMode) {
    zes_standby_promo_mode_t mode = {};
    auto handles = getStandbyHandles(mockHandleCount);

    for (auto hSysmanStandby : handles) {
        ASSERT_NE(nullptr, hSysmanStandby);
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesStandbyGetMode(hSysmanStandby, &mode));
        EXPECT_EQ(ZES_STANDBY_PROMO_MODE_DEFAULT, mode);
    }
}

TEST_F(ZesStandbyFixture, GivenValidStandbyHandleWhenCallingzesStandbyGetModeThenVerifyzesStandbyGetModeCallSucceedsForNeverMode) {
    zes_standby_promo_mode_t mode = {};
    ptestSysfsAccess->setVal(standbyModeFile, standbyModeNever);
    auto handles = getStandbyHandles(mockHandleCount);

    for (auto hSysmanStandby : handles) {
        ASSERT_NE(nullptr, hSysmanStandby);
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesStandbyGetMode(hSysmanStandby, &mode));
        EXPECT_EQ(ZES_STANDBY_PROMO_MODE_NEVER, mode);
    }
}

TEST_F(ZesStandbyFixture, GivenInvalidStandbyFileWhenReadisCalledThenExpectFailure) {
    zes_standby_promo_mode_t mode = {};
    ptestSysfsAccess->setValReturnError(ZE_RESULT_ERROR_NOT_AVAILABLE);

    auto handles = getStandbyHandles(mockHandleCount);

    for (auto hSysmanStandby : handles) {
        ASSERT_NE(nullptr, hSysmanStandby);
        EXPECT_NE(ZE_RESULT_SUCCESS, zesStandbyGetMode(hSysmanStandby, &mode));
    }
}

TEST_F(ZesStandbyFixture, GivenValidStandbyHandleWhenCallingzesStandbyGetModeThenVerifyzesStandbyGetModeCallFailsForInvalidMode) {
    zes_standby_promo_mode_t mode = {};
    ptestSysfsAccess->setVal(standbyModeFile, standbyModeInvalid);
    auto handles = getStandbyHandles(mockHandleCount);

    for (auto hSysmanStandby : handles) {
        ASSERT_NE(nullptr, hSysmanStandby);
        EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, zesStandbyGetMode(hSysmanStandby, &mode));
    }
}

TEST_F(ZesStandbyFixture, GivenValidStandbyHandleWhenCallingzesStandbyGetModeOnUnavailableFileThenVerifyzesStandbyGetModeCallFailsForUnsupportedFeature) {
    zes_standby_promo_mode_t mode = {};
    ptestSysfsAccess->setVal(standbyModeFile, standbyModeInvalid);
    auto handles = getStandbyHandles(mockHandleCount);

    ptestSysfsAccess->isStandbyModeFileAvailable = false;

    for (auto hSysmanStandby : handles) {
        ASSERT_NE(nullptr, hSysmanStandby);
        EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesStandbyGetMode(hSysmanStandby, &mode));
    }
}

TEST_F(ZesStandbyFixture, GivenValidStandbyHandleWhenCallingzesStandbyGetModeWithInsufficientPermissionsThenVerifyzesStandbyGetModeCallFailsForInsufficientPermissions) {
    zes_standby_promo_mode_t mode = {};
    ptestSysfsAccess->setVal(standbyModeFile, standbyModeInvalid);
    auto handles = getStandbyHandles(mockHandleCount);

    ptestSysfsAccess->mockStandbyFileMode &= ~S_IRUSR;

    for (auto hSysmanStandby : handles) {
        ASSERT_NE(nullptr, hSysmanStandby);
        EXPECT_EQ(ZE_RESULT_ERROR_INSUFFICIENT_PERMISSIONS, zesStandbyGetMode(hSysmanStandby, &mode));
    }
}

TEST_F(ZesStandbyFixture, GivenValidStandbyHandleWhenCallingzesStandbySetModeThenwithUnwritableFileVerifySysmanzesySetModeCallFailedWithInsufficientPermissions) {
    auto handles = getStandbyHandles(mockHandleCount);
    ptestSysfsAccess->mockStandbyFileMode &= ~S_IWUSR;

    for (auto hSysmanStandby : handles) {
        ASSERT_NE(nullptr, hSysmanStandby);
        EXPECT_EQ(ZE_RESULT_ERROR_INSUFFICIENT_PERMISSIONS, zesStandbySetMode(hSysmanStandby, ZES_STANDBY_PROMO_MODE_NEVER));
    }
}

TEST_F(ZesStandbyFixture, GivenValidStandbyHandleWhenCallingzesStandbySetModeOnUnavailableFileThenVerifyzesStandbySetModeCallFailsForUnsupportedFeature) {
    auto handles = getStandbyHandles(mockHandleCount);

    ptestSysfsAccess->isStandbyModeFileAvailable = false;
    for (auto hSysmanStandby : handles) {
        ASSERT_NE(nullptr, hSysmanStandby);
        EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesStandbySetMode(hSysmanStandby, ZES_STANDBY_PROMO_MODE_NEVER));
    }
}

TEST_F(ZesStandbyFixture, GivenValidStandbyHandleWhenCallingzesStandbySetModeNeverThenVerifySysmanzesySetModeCallSucceeds) {
    auto handles = getStandbyHandles(mockHandleCount);

    for (auto hSysmanStandby : handles) {
        ASSERT_NE(nullptr, hSysmanStandby);
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
    auto handles = getStandbyHandles(mockHandleCount);

    for (auto hSysmanStandby : handles) {
        ASSERT_NE(nullptr, hSysmanStandby);
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
    std::unique_ptr<PublicLinuxStandbyImp> pLinuxStandbyImp = std::make_unique<PublicLinuxStandbyImp>(pOsSysman, isSubDevice, deviceProperties.subdeviceId);
    EXPECT_EQ(ZE_RESULT_SUCCESS, pLinuxStandbyImp->osStandbyGetProperties(properties));
    EXPECT_EQ(properties.subdeviceId, deviceProperties.subdeviceId);
    EXPECT_EQ(properties.onSubdevice, isSubDevice);
}

TEST_F(ZesStandbyFixture, GivenValidStandbyHandleWhenCallingzesStandbySetModeDefaultWithLegacyPathThenVerifySysmanzesySetModeCallSucceeds) {
    pSysmanDeviceImp->pStandbyHandleContext->handleList.clear();
    ptestSysfsAccess->directoryExistsResult = false;
    pSysmanDeviceImp->pStandbyHandleContext->init(deviceHandles);

    auto handles = getStandbyHandles(mockHandleCount);

    for (auto hSysmanStandby : handles) {
        ASSERT_NE(nullptr, hSysmanStandby);
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
    pSysmanDeviceImp->pStandbyHandleContext->handleList.clear();
    ptestSysfsAccess->directoryExistsResult = false;
    pSysmanDeviceImp->pStandbyHandleContext->init(deviceHandles);

    auto handles = getStandbyHandles(mockHandleCount);

    for (auto hSysmanStandby : handles) {
        ASSERT_NE(nullptr, hSysmanStandby);
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
    std::unique_ptr<MockStandbySysfsAccess> ptestSysfsAccess;
    SysfsAccess *pOriginalSysfsAccess = nullptr;

  protected:
    void SetUp() override {
        if (!sysmanUltsEnable) {
            GTEST_SKIP();
        }
        SysmanMultiDeviceFixture::SetUp();
        mockSubDeviceHandleCount = subDeviceCount;
        ptestSysfsAccess = std::make_unique<MockStandbySysfsAccess>();
        pOriginalSysfsAccess = pLinuxSysmanImp->pSysfsAccess;
        pLinuxSysmanImp->pSysfsAccess = ptestSysfsAccess.get();
        ptestSysfsAccess->setVal(standbyModeFile, standbyModeDefault);
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
    }
    void TearDown() override {
        if (!sysmanUltsEnable) {
            GTEST_SKIP();
        }
        pLinuxSysmanImp->pSysfsAccess = pOriginalSysfsAccess;
        SysmanMultiDeviceFixture::TearDown();
    }

    std::vector<zes_standby_handle_t> getStandbyHandles(uint32_t count) {
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
    std::unique_ptr<PublicLinuxStandbyImp> pLinuxStandbyImp = std::make_unique<PublicLinuxStandbyImp>(pOsSysman, isSubDevice, deviceProperties.subdeviceId);
    EXPECT_EQ(ZE_RESULT_SUCCESS, pLinuxStandbyImp->osStandbyGetProperties(properties));
    EXPECT_EQ(properties.subdeviceId, deviceProperties.subdeviceId);
    EXPECT_EQ(properties.onSubdevice, isSubDevice);
}

class StandbyAffinityMaskFixture : public ZesStandbyMultiDeviceFixture {
    void SetUp() override {
        if (!sysmanUltsEnable) {
            GTEST_SKIP();
        }
        NEO::debugManager.flags.ZE_AFFINITY_MASK.set("0.1");
        ZesStandbyMultiDeviceFixture::SetUp();
    }

    void TearDown() override {
        if (!sysmanUltsEnable) {
            GTEST_SKIP();
        }
        ZesStandbyMultiDeviceFixture::TearDown();
    }
    DebugManagerStateRestore restorer;
};

TEST_F(StandbyAffinityMaskFixture, GivenAffinityMaskIsSetWhenCallingStandbyPropertiesThenProertiesAreReturnedForTheSubDevicesAccordingToAffinityMask) {
    uint32_t count = 0;

    ze_result_t result = zesDeviceEnumStandbyDomains(device, &count, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(count, mockHandleCount);
    zes_standby_properties_t properties = {};
    auto handles = getStandbyHandles(mockHandleCount);

    for (auto hSysmanStandby : handles) {
        ASSERT_NE(nullptr, hSysmanStandby);
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesStandbyGetProperties(hSysmanStandby, &properties));
        EXPECT_EQ(nullptr, properties.pNext);
        EXPECT_EQ(ZES_STANDBY_TYPE_GLOBAL, properties.type);
        EXPECT_TRUE(properties.onSubdevice);
        EXPECT_EQ(1u, properties.subdeviceId); // Affinity mask 0.1 is set which means only subdevice 1 is exposed
    }
}

} // namespace ult
} // namespace L0
