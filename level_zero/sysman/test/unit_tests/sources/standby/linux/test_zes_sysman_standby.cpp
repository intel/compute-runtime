/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/sysman/source/shared/linux/sysman_kmd_interface.h"
#include "level_zero/sysman/test/unit_tests/sources/linux/mock_sysman_fixture.h"
#include "level_zero/sysman/test/unit_tests/sources/standby/linux/mock_sysfs_standby.h"

namespace L0 {
namespace Sysman {
namespace ult {

constexpr int standbyModeDefault = 1;
constexpr int standbyModeNever = 0;
constexpr int standbyModeInvalid = 0xff;
constexpr uint32_t mockHandleCount = 1u;
uint32_t mockSubDeviceHandleCount = 0u;
class ZesStandbyFixture : public SysmanDeviceFixture {
  protected:
    L0::Sysman::SysmanDevice *device = nullptr;
    std::unique_ptr<MockStandbySysfsAccess> ptestSysfsAccess;
    zes_standby_handle_t hSysmanStandby = {};
    L0::Sysman::SysfsAccess *pOriginalSysfsAccess = nullptr;

    void SetUp() override {
        SysmanDeviceFixture::SetUp();
        device = pSysmanDevice;
        ptestSysfsAccess = std::make_unique<MockStandbySysfsAccess>();
        pOriginalSysfsAccess = pLinuxSysmanImp->pSysfsAccess;
        pLinuxSysmanImp->pSysfsAccess = ptestSysfsAccess.get();
        ptestSysfsAccess->setVal(standbyModeFile, standbyModeDefault);
        pSysmanDeviceImp->pStandbyHandleContext->handleList.clear();
    }
    void TearDown() override {
        pLinuxSysmanImp->pSysfsAccess = pOriginalSysfsAccess;
        SysmanDeviceFixture::TearDown();
    }

    std::vector<zes_standby_handle_t> getStandbyHandles(uint32_t count) {
        std::vector<zes_standby_handle_t> handles(count, nullptr);
        EXPECT_EQ(zesDeviceEnumStandbyDomains(device, &count, handles.data()), ZE_RESULT_SUCCESS);
        return handles;
    }
};

TEST_F(ZesStandbyFixture, GivenKmdInterfaceWhenGettingFilenamesForStandbyForI915VersionAndBaseDirectoryExistsThenProperPathsAreReturned) {
    auto pSysmanKmdInterface = std::make_unique<SysmanKmdInterfaceI915>(pLinuxSysmanImp->getProductFamily());
    bool baseDirectoryExists = true;
    EXPECT_STREQ("gt/gt0/rc6_enable", pSysmanKmdInterface->getSysfsFilePath(SysfsName::sysfsNameStandbyModeControl, 0, baseDirectoryExists).c_str());
}

TEST_F(ZesStandbyFixture, GivenKmdInterfaceWhenGettingFilenamesForStandbyForI915VersionAndBaseDirectoryDoesntExistThenProperPathsAreReturned) {
    auto pSysmanKmdInterface = std::make_unique<SysmanKmdInterfaceI915>(pLinuxSysmanImp->getProductFamily());
    bool baseDirectoryExists = false;
    EXPECT_STREQ("power/rc6_enable", pSysmanKmdInterface->getSysfsFilePath(SysfsName::sysfsNameStandbyModeControl, 0, baseDirectoryExists).c_str());
}

TEST_F(ZesStandbyFixture, GivenKmdInterfaceWhenGettingFilenamesForStandbyForXeVersionThenUnsupportedFeatureIsReturned) {
    auto subDeviceCount = pLinuxSysmanImp->getSubDeviceCount();
    ze_bool_t onSubdevice = (subDeviceCount == 0) ? false : true;
    uint32_t subdeviceId = 0;
    std::unique_ptr<PublicLinuxStandbyImp> pLinuxStandbyImp = std::make_unique<PublicLinuxStandbyImp>(pOsSysman, onSubdevice, subdeviceId);
    auto pSysmanKmdInterface = std::make_unique<SysmanKmdInterfaceXe>(pLinuxSysmanImp->getProductFamily());
    pLinuxStandbyImp->pSysmanKmdInterface = pSysmanKmdInterface.get();
    EXPECT_FALSE(pLinuxStandbyImp->pSysmanKmdInterface->isStandbyModeControlAvailable());

    zes_standby_promo_mode_t mode = {};
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, pLinuxStandbyImp->getMode(mode));
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, pLinuxStandbyImp->setMode(mode));
}

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

    auto subDeviceCount = pLinuxSysmanImp->getSubDeviceCount();
    ze_bool_t onSubdevice = (subDeviceCount == 0) ? false : true;
    uint32_t subdeviceId = 0;

    std::unique_ptr<L0::Sysman::StandbyImp> ptestStandbyImp = std::make_unique<L0::Sysman::StandbyImp>(pSysmanDeviceImp->pStandbyHandleContext->pOsSysman, onSubdevice, subdeviceId);
    count = 0;
    pSysmanDeviceImp->pStandbyHandleContext->handleList.push_back(std::move(ptestStandbyImp));
    result = zesDeviceEnumStandbyDomains(device, &count, nullptr);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(count, mockHandleCount + 1);

    testCount = count + 1;

    standbyHandle.resize(testCount);
    result = zesDeviceEnumStandbyDomains(device, &testCount, standbyHandle.data());

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, standbyHandle.data());
    EXPECT_EQ(testCount, mockHandleCount + 1);

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
    auto subDeviceCount = pLinuxSysmanImp->getSubDeviceCount();
    zes_standby_properties_t properties = {};
    ze_bool_t onSubdevice = (subDeviceCount == 0) ? false : true;
    uint32_t subdeviceId = 0;
    std::unique_ptr<PublicLinuxStandbyImp> pLinuxStandbyImp = std::make_unique<PublicLinuxStandbyImp>(pOsSysman, onSubdevice, subdeviceId);
    EXPECT_EQ(ZE_RESULT_SUCCESS, pLinuxStandbyImp->osStandbyGetProperties(properties));
    EXPECT_EQ(properties.subdeviceId, subdeviceId);
    EXPECT_EQ(properties.onSubdevice, onSubdevice);
}

TEST_F(ZesStandbyFixture, GivenValidStandbyHandleWhenCallingzesStandbySetModeDefaultWithLegacyPathThenVerifySysmanzesySetModeCallSucceeds) {
    pSysmanDeviceImp->pStandbyHandleContext->handleList.clear();
    ptestSysfsAccess->directoryExistsResult = false;
    pSysmanDeviceImp->pStandbyHandleContext->init(pLinuxSysmanImp->getSubDeviceCount());

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
    pSysmanDeviceImp->pStandbyHandleContext->init(pLinuxSysmanImp->getSubDeviceCount());

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
    L0::Sysman::SysfsAccess *pOriginalSysfsAccess = nullptr;

  protected:
    L0::Sysman::SysmanDevice *device = nullptr;
    void SetUp() override {
        SysmanMultiDeviceFixture::SetUp();
        device = pSysmanDevice;
        mockSubDeviceHandleCount = pLinuxSysmanImp->getSubDeviceCount();
        ptestSysfsAccess = std::make_unique<MockStandbySysfsAccess>();
        pOriginalSysfsAccess = pLinuxSysmanImp->pSysfsAccess;
        pLinuxSysmanImp->pSysfsAccess = ptestSysfsAccess.get();
        ptestSysfsAccess->setVal(standbyModeFile, standbyModeDefault);
        pSysmanDeviceImp->pStandbyHandleContext->handleList.clear();
    }
    void TearDown() override {
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
    auto subDeviceCount = pLinuxSysmanImp->getSubDeviceCount();
    zes_standby_properties_t properties = {};
    ze_bool_t onSubdevice = (subDeviceCount == 0) ? false : true;
    uint32_t subdeviceId = 0;
    std::unique_ptr<PublicLinuxStandbyImp> pLinuxStandbyImp = std::make_unique<PublicLinuxStandbyImp>(pOsSysman, onSubdevice, subdeviceId);
    EXPECT_EQ(ZE_RESULT_SUCCESS, pLinuxStandbyImp->osStandbyGetProperties(properties));
    EXPECT_EQ(properties.subdeviceId, subdeviceId);
    EXPECT_EQ(properties.onSubdevice, onSubdevice);
}

} // namespace ult
} // namespace Sysman
} // namespace L0
