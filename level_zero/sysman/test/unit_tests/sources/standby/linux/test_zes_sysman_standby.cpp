/*
 * Copyright (C) 2023-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/sysman/source/shared/linux/product_helper/sysman_product_helper_hw.h"
#include "level_zero/sysman/test/unit_tests/sources/standby/linux/mock_sysfs_standby.h"

namespace L0 {
namespace Sysman {
namespace ult {

TEST_F(ZesStandbyFixtureI915, GivenStandbyModeFilesNotAvailableWhenCallingEnumerateThenCallSucceedsAndZeroCountIsReturned) {

    VariableBackup<decltype(NEO::SysCalls::sysCallsStat)> mockStat(&NEO::SysCalls::sysCallsStat, [](const std::string &filePath, struct stat *statbuf) -> int {
        statbuf->st_mode = ~S_IRUSR;
        return 0;
    });

    uint32_t count = 0;
    ze_result_t result = zesDeviceEnumStandbyDomains(device, &count, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(count, 0u);
}

TEST_F(ZesStandbyFixtureI915, GivenValidDeviceHandleAndStandbyNotSupportedWhenCallingEnumerateThenVerifyStandbyDomainsAreZero) {

    VariableBackup<decltype(NEO::SysCalls::sysCallsStat)> mockStat(&NEO::SysCalls::sysCallsStat, [](const std::string &filePath, struct stat *statbuf) -> int {
        statbuf->st_mode = S_IRUSR;
        return 0;
    });

    struct MockSysmanProductHelperStandby : L0::Sysman::SysmanProductHelperHw<IGFX_UNKNOWN> {
        MockSysmanProductHelperStandby() = default;
        std::string getStandbyModeFile(SysmanKmdInterface *pSysmanKmdInterface, SysFsAccessInterface *pSysfsAccess, uint32_t subDeviceId) override {
            return {};
        }
    };

    std::unique_ptr<SysmanProductHelper> pSysmanProductHelper = std::make_unique<MockSysmanProductHelperStandby>();
    std::swap(pLinuxSysmanImp->pSysmanProductHelper, pSysmanProductHelper);

    uint32_t count = 0;
    ze_result_t result = zesDeviceEnumStandbyDomains(device, &count, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(count, 0u);
}

HWTEST2_F(ZesStandbyFixtureI915, GivenComponentCountZeroWhenCallingZesDeviceEnumStandbyDomainsThenNonZeroCountIsReturned, IsXeCore) {

    VariableBackup<decltype(NEO::SysCalls::sysCallsStat)> mockStat(&NEO::SysCalls::sysCallsStat, [](const std::string &filePath, struct stat *statbuf) -> int {
        statbuf->st_mode = S_IRUSR;
        return 0;
    });

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

HWTEST2_F(ZesStandbyFixtureI915, GivenValidStandbyHandleWhenCallingZesStandbyGetPropertiesThenCallSucceeds, IsXeCore) {

    mockKMDInterfaceSetup();
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

HWTEST2_F(ZesStandbyFixtureI915, GivenValidStandbyHandleWhenCallingZesStandbyGetModeThenCallSucceedsWithStandbyModeDefault, IsPVC) {

    mockKMDInterfaceSetup();
    pSysfsAccess->setVal(standbyModeFile, standbyModeDefault);
    zes_standby_promo_mode_t mode = {};

    auto handles = getStandbyHandles(mockHandleCount);

    for (auto hSysmanStandby : handles) {
        ASSERT_NE(nullptr, hSysmanStandby);
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesStandbyGetMode(hSysmanStandby, &mode));
        EXPECT_EQ(ZES_STANDBY_PROMO_MODE_DEFAULT, mode);
    }
}

HWTEST2_F(ZesStandbyFixtureI915, GivenValidStandbyHandleWhenCallingZesStandbyGetModeThenCallSucceedsWithStandbyModeNever, IsPVC) {

    zes_standby_promo_mode_t mode = {};
    mockKMDInterfaceSetup();
    pSysfsAccess->setVal(standbyModeFile, standbyModeNever);

    auto handles = getStandbyHandles(mockHandleCount);

    for (auto hSysmanStandby : handles) {
        ASSERT_NE(nullptr, hSysmanStandby);
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesStandbyGetMode(hSysmanStandby, &mode));
        EXPECT_EQ(ZES_STANDBY_PROMO_MODE_NEVER, mode);
    }
}

HWTEST2_F(ZesStandbyFixtureI915, GivenInvalidStandbyFileWhenReadIsCalledThenExpectFailure, IsXeCore) {

    mockKMDInterfaceSetup();
    zes_standby_promo_mode_t mode = {};
    pSysfsAccess->setValReturnError(ZE_RESULT_ERROR_NOT_AVAILABLE);

    auto handles = getStandbyHandles(mockHandleCount);
    for (auto hSysmanStandby : handles) {
        ASSERT_NE(nullptr, hSysmanStandby);
        EXPECT_NE(ZE_RESULT_SUCCESS, zesStandbyGetMode(hSysmanStandby, &mode));
    }
}

HWTEST2_F(ZesStandbyFixtureI915, GivenValidStandbyHandleWhenCallingZesStandbyGetModeThenCallFailsWithInvalidMode, IsXeCore) {

    mockKMDInterfaceSetup();
    zes_standby_promo_mode_t mode = {};
    pSysfsAccess->setVal(standbyModeFile, standbyModeInvalid);

    auto handles = getStandbyHandles(mockHandleCount);

    for (auto hSysmanStandby : handles) {
        ASSERT_NE(nullptr, hSysmanStandby);
        EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, zesStandbyGetMode(hSysmanStandby, &mode));
    }
}

HWTEST2_F(ZesStandbyFixtureI915, GivenValidStandbyHandleWhenCallingZesStandbyGetModeOnUnavailableFileThenCallFailsWithUnsupportedFeature, IsXeCore) {

    mockKMDInterfaceSetup();
    zes_standby_promo_mode_t mode = {};
    pSysfsAccess->setVal(standbyModeFile, standbyModeInvalid);

    auto handles = getStandbyHandles(mockHandleCount);

    pSysfsAccess->isStandbyModeFileAvailable = false;

    for (auto hSysmanStandby : handles) {
        ASSERT_NE(nullptr, hSysmanStandby);
        EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesStandbyGetMode(hSysmanStandby, &mode));
    }
}

HWTEST2_F(ZesStandbyFixtureI915, GivenValidStandbyHandleWhenCallingZesStandbyGetModeWithInsufficientPermissionsThenCallFailsWithInsufficientPermissions, IsXeCore) {

    mockKMDInterfaceSetup();
    zes_standby_promo_mode_t mode = {};
    pSysfsAccess->setVal(standbyModeFile, standbyModeInvalid);

    auto handles = getStandbyHandles(mockHandleCount);
    pSysfsAccess->mockStandbyFileMode &= ~S_IRUSR;

    for (auto hSysmanStandby : handles) {
        ASSERT_NE(nullptr, hSysmanStandby);
        EXPECT_EQ(ZE_RESULT_ERROR_INSUFFICIENT_PERMISSIONS, zesStandbyGetMode(hSysmanStandby, &mode));
    }
}

HWTEST2_F(ZesStandbyFixtureI915, GivenValidStandbyHandleWhenCallingZesStandbySetModeWithUnwritableFileThenCallFailsWithInsufficientPermissions, IsPVC) {

    mockKMDInterfaceSetup();
    auto handles = getStandbyHandles(mockHandleCount);
    pSysfsAccess->mockStandbyFileMode &= ~S_IWUSR;

    for (auto hSysmanStandby : handles) {
        ASSERT_NE(nullptr, hSysmanStandby);
        EXPECT_EQ(ZE_RESULT_ERROR_INSUFFICIENT_PERMISSIONS, zesStandbySetMode(hSysmanStandby, ZES_STANDBY_PROMO_MODE_NEVER));
    }
}

HWTEST2_F(ZesStandbyFixtureI915, GivenValidStandbyHandleWhenCallingZesStandbySetModeOnUnavailableFileThenCallFailsWithUnsupportedFeature, IsXeCore) {

    mockKMDInterfaceSetup();
    auto handles = getStandbyHandles(mockHandleCount);
    pSysfsAccess->isStandbyModeFileAvailable = false;

    for (auto hSysmanStandby : handles) {
        ASSERT_NE(nullptr, hSysmanStandby);
        EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesStandbySetMode(hSysmanStandby, ZES_STANDBY_PROMO_MODE_NEVER));
    }
}

HWTEST2_F(ZesStandbyFixtureI915, GivenValidStandbyHandleWhenCallingZesStandbySetModeWithStandbyModeNeverThenCallSucceeds, IsPVC) {

    mockKMDInterfaceSetup();
    auto handles = getStandbyHandles(mockHandleCount);

    for (auto hSysmanStandby : handles) {
        ASSERT_NE(nullptr, hSysmanStandby);
        zes_standby_promo_mode_t mode;
        pSysfsAccess->setVal(standbyModeFile, standbyModeNever);
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesStandbyGetMode(hSysmanStandby, &mode));
        EXPECT_EQ(ZES_STANDBY_PROMO_MODE_NEVER, mode);
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesStandbySetMode(hSysmanStandby, ZES_STANDBY_PROMO_MODE_DEFAULT));
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesStandbyGetMode(hSysmanStandby, &mode));
        EXPECT_EQ(ZES_STANDBY_PROMO_MODE_DEFAULT, mode);
    }
}

HWTEST2_F(ZesStandbyFixtureI915, GivenValidStandbyHandleWhenCallingZesStandbySetModeWithDefaultModeThenCallSucceeds, IsPVC) {

    mockKMDInterfaceSetup();
    auto handles = getStandbyHandles(mockHandleCount);

    for (auto hSysmanStandby : handles) {
        ASSERT_NE(nullptr, hSysmanStandby);
        zes_standby_promo_mode_t mode;
        pSysfsAccess->setVal(standbyModeFile, standbyModeNever);
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesStandbyGetMode(hSysmanStandby, &mode));
        EXPECT_EQ(ZES_STANDBY_PROMO_MODE_NEVER, mode);
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesStandbySetMode(hSysmanStandby, ZES_STANDBY_PROMO_MODE_DEFAULT));
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesStandbyGetMode(hSysmanStandby, &mode));
        EXPECT_EQ(ZES_STANDBY_PROMO_MODE_DEFAULT, mode);
    }
}

HWTEST2_F(ZesStandbyFixtureI915, GivenOnSubdeviceNotSetWhenValidatingOsStandbyGetPropertiesThenSuccessIsReturned, IsXeCore) {

    mockKMDInterfaceSetup();
    zes_standby_properties_t properties = {};
    auto pSysfsAccess = std::make_unique<MockStandbySysfsAccessInterface>();
    std::unique_ptr<PublicLinuxStandbyImp> pLinuxStandbyImp = std::make_unique<PublicLinuxStandbyImp>(pOsSysman, onSubdevice, subdeviceId);
    pLinuxStandbyImp->pSysfsAccess = pSysfsAccess.get();
    EXPECT_EQ(ZE_RESULT_SUCCESS, pLinuxStandbyImp->osStandbyGetProperties(properties));
    EXPECT_EQ(properties.subdeviceId, subdeviceId);
    EXPECT_EQ(properties.onSubdevice, onSubdevice);
}

HWTEST2_F(ZesStandbyFixtureI915, GivenValidStandbyHandleWhenCallingZesStandbySetModeWithDefaultModeWithLegacyPathThenCallSucceeds, IsPVC) {

    mockKMDInterfaceSetup();
    pSysmanDeviceImp->pStandbyHandleContext->handleList.clear();
    pSysfsAccess->directoryExistsResult = false;
    pSysmanDeviceImp->pStandbyHandleContext->init(pLinuxSysmanImp->getSubDeviceCount());

    auto handles = getStandbyHandles(mockHandleCount);

    for (auto hSysmanStandby : handles) {
        ASSERT_NE(nullptr, hSysmanStandby);
        zes_standby_promo_mode_t mode;
        pSysfsAccess->setVal(standbyModeFile, standbyModeNever);
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesStandbyGetMode(hSysmanStandby, &mode));
        EXPECT_EQ(ZES_STANDBY_PROMO_MODE_NEVER, mode);
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesStandbySetMode(hSysmanStandby, ZES_STANDBY_PROMO_MODE_DEFAULT));
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesStandbyGetMode(hSysmanStandby, &mode));
        EXPECT_EQ(ZES_STANDBY_PROMO_MODE_DEFAULT, mode);
    }
}

HWTEST2_F(ZesStandbyFixtureI915, GivenValidStandbyHandleWhenCallingZesStandbySetModeWithStandbyModeNeverWithLegacyPathThenCallSucceeds, IsPVC) {

    mockKMDInterfaceSetup();
    pSysmanDeviceImp->pStandbyHandleContext->handleList.clear();
    pSysfsAccess->directoryExistsResult = false;
    pSysmanDeviceImp->pStandbyHandleContext->init(pLinuxSysmanImp->getSubDeviceCount());
    auto handles = getStandbyHandles(mockHandleCount);

    for (auto hSysmanStandby : handles) {
        ASSERT_NE(nullptr, hSysmanStandby);
        zes_standby_promo_mode_t mode;
        pSysfsAccess->setVal(standbyModeFile, standbyModeDefault);
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesStandbyGetMode(hSysmanStandby, &mode));
        EXPECT_EQ(ZES_STANDBY_PROMO_MODE_DEFAULT, mode);
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesStandbySetMode(hSysmanStandby, ZES_STANDBY_PROMO_MODE_NEVER));
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesStandbyGetMode(hSysmanStandby, &mode));
        EXPECT_EQ(ZES_STANDBY_PROMO_MODE_NEVER, mode);
    }
}

TEST_F(ZesStandbyFixtureXe, GivenValidDeviceHandleWhenCallingEnumerateThenOneStandbyDomainIsReturned) {
    mockKMDInterfaceSetup();

    uint32_t count = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEnumStandbyDomains(device, &count, nullptr));
    EXPECT_EQ(count, mockHandleCount);

    auto handles = getStandbyHandles(count);
    for (auto hSysmanStandby : handles) {
        ASSERT_NE(nullptr, hSysmanStandby);
        zes_standby_properties_t properties = {};
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesStandbyGetProperties(hSysmanStandby, &properties));
        EXPECT_EQ(ZES_STANDBY_TYPE_GLOBAL, properties.type);
        EXPECT_FALSE(properties.onSubdevice);
        EXPECT_EQ(0u, properties.subdeviceId);
    }
}

TEST_F(ZesStandbyFixtureXe, GivenValidStandbyHandleWhenCallingZesStandbyGetModeThenModeIsReadFromPciRuntimePowerManagementNode) {
    mockKMDInterfaceSetup();
    auto handles = getStandbyHandles(mockHandleCount);

    for (auto hSysmanStandby : handles) {
        ASSERT_NE(nullptr, hSysmanStandby);
        zes_standby_promo_mode_t mode = {};
        pSysfsAccess->mockStandbyModeString = standbyPowerControlDefault;
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesStandbyGetMode(hSysmanStandby, &mode));
        EXPECT_EQ(ZES_STANDBY_PROMO_MODE_DEFAULT, mode);

        pSysfsAccess->mockStandbyModeString = standbyPowerControlNever;
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesStandbyGetMode(hSysmanStandby, &mode));
        EXPECT_EQ(ZES_STANDBY_PROMO_MODE_NEVER, mode);
    }
}

TEST_F(ZesStandbyFixtureXe, GivenValidStandbyHandleWhenCallingZesStandbySetModeThenPciRuntimePowerManagementNodeIsUpdated) {
    mockKMDInterfaceSetup();
    auto handles = getStandbyHandles(mockHandleCount);

    for (auto hSysmanStandby : handles) {
        ASSERT_NE(nullptr, hSysmanStandby);
        zes_standby_promo_mode_t mode = {};
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesStandbySetMode(hSysmanStandby, ZES_STANDBY_PROMO_MODE_NEVER));
        EXPECT_STREQ(standbyPowerControlNever.c_str(), pSysfsAccess->mockStandbyModeString.c_str());
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesStandbyGetMode(hSysmanStandby, &mode));
        EXPECT_EQ(ZES_STANDBY_PROMO_MODE_NEVER, mode);

        EXPECT_EQ(ZE_RESULT_SUCCESS, zesStandbySetMode(hSysmanStandby, ZES_STANDBY_PROMO_MODE_DEFAULT));
        EXPECT_STREQ(standbyPowerControlDefault.c_str(), pSysfsAccess->mockStandbyModeString.c_str());
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesStandbyGetMode(hSysmanStandby, &mode));
        EXPECT_EQ(ZES_STANDBY_PROMO_MODE_DEFAULT, mode);
    }
}

TEST_F(ZesStandbyFixtureXe, GivenValidStandbyHandleWhenCallingZesStandbyGetModeWithUnknownNodeValueThenCallFailsWithUnknownError) {
    mockKMDInterfaceSetup();
    auto handles = getStandbyHandles(mockHandleCount);
    pSysfsAccess->mockStandbyModeString = "unknown";

    for (auto hSysmanStandby : handles) {
        ASSERT_NE(nullptr, hSysmanStandby);
        zes_standby_promo_mode_t mode = {};
        EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, zesStandbyGetMode(hSysmanStandby, &mode));
    }
}

TEST_F(ZesStandbyFixtureXe, GivenValidStandbyHandleWhenCallingZesStandbyGetModeOnUnavailableFileThenCallFailsWithUnsupportedFeature) {
    mockKMDInterfaceSetup();
    auto handles = getStandbyHandles(mockHandleCount);
    pSysfsAccess->isStandbyModeFileAvailable = false;

    for (auto hSysmanStandby : handles) {
        ASSERT_NE(nullptr, hSysmanStandby);
        zes_standby_promo_mode_t mode = {};
        EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesStandbyGetMode(hSysmanStandby, &mode));
    }
}

TEST_F(ZesStandbyFixtureXe, GivenValidStandbyHandleWhenCallingZesStandbyGetModeWithInsufficientPermissionsThenCallFailsWithInsufficientPermissions) {
    mockKMDInterfaceSetup();
    auto handles = getStandbyHandles(mockHandleCount);
    pSysfsAccess->mockStandbyFileMode &= ~S_IRUSR;

    for (auto hSysmanStandby : handles) {
        ASSERT_NE(nullptr, hSysmanStandby);
        zes_standby_promo_mode_t mode = {};
        EXPECT_EQ(ZE_RESULT_ERROR_INSUFFICIENT_PERMISSIONS, zesStandbyGetMode(hSysmanStandby, &mode));
    }
}

TEST_F(ZesStandbyFixtureXe, GivenValidStandbyHandleWhenCallingZesStandbySetModeWithUnwritableFileThenCallFailsWithInsufficientPermissions) {
    mockKMDInterfaceSetup();
    auto handles = getStandbyHandles(mockHandleCount);
    pSysfsAccess->mockStandbyFileMode &= ~S_IWUSR;

    for (auto hSysmanStandby : handles) {
        ASSERT_NE(nullptr, hSysmanStandby);
        EXPECT_EQ(ZE_RESULT_ERROR_INSUFFICIENT_PERMISSIONS, zesStandbySetMode(hSysmanStandby, ZES_STANDBY_PROMO_MODE_NEVER));
    }
}

TEST_F(ZesStandbyFixtureXe, GivenValidStandbyHandleWhenCallingZesStandbySetModeOnUnavailableFileThenCallFailsWithUnsupportedFeature) {
    mockKMDInterfaceSetup();
    auto handles = getStandbyHandles(mockHandleCount);
    pSysfsAccess->isStandbyModeFileAvailable = false;

    for (auto hSysmanStandby : handles) {
        ASSERT_NE(nullptr, hSysmanStandby);
        EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesStandbySetMode(hSysmanStandby, ZES_STANDBY_PROMO_MODE_NEVER));
    }
}

TEST_F(ZesStandbyFixtureXe, GivenStandbyModeControlFileIsNotTilePrefixedWhenValidatingSupportForSubdeviceHandleThenSupportIsReportedForEverySubdevice) {
    mockKMDInterfaceSetup();

    for (uint32_t subdeviceId = 0; subdeviceId < 2u; subdeviceId++) {
        std::unique_ptr<PublicLinuxStandbyImp> pLinuxStandbyImp = std::make_unique<PublicLinuxStandbyImp>(pOsSysman, true, subdeviceId);
        EXPECT_TRUE(pLinuxStandbyImp->isStandbySupported());
    }
}

HWTEST2_F(ZesStandbyMultiDeviceFixture, GivenComponentCountZeroWhenCallingzesDeviceEnumStandbyDomainsThenCallSucceedsAndNonZeroCountIsReturned, IsXeCore) {

    VariableBackup<decltype(NEO::SysCalls::sysCallsStat)> mockStat(&NEO::SysCalls::sysCallsStat, [](const std::string &filePath, struct stat *statbuf) -> int {
        statbuf->st_mode = S_IRUSR;
        return 0;
    });

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

TEST_F(ZesStandbyMultiDeviceFixture, GivenOnSubdeviceNotSetWhenValidatingOsStandbyGetPropertiesThenSuccessIsReturned) {

    auto pSysfsAccess = std::make_unique<MockStandbySysfsAccessInterface>();
    std::unique_ptr<PublicLinuxStandbyImp> pLinuxStandbyImp = std::make_unique<PublicLinuxStandbyImp>(pOsSysman, onSubdevice, subdeviceId);
    pLinuxStandbyImp->pSysfsAccess = pSysfsAccess.get();

    zes_standby_properties_t properties = {};
    EXPECT_EQ(ZE_RESULT_SUCCESS, pLinuxStandbyImp->osStandbyGetProperties(properties));
    EXPECT_EQ(properties.subdeviceId, subdeviceId);
    EXPECT_EQ(properties.onSubdevice, onSubdevice);
}

HWTEST2_F(ZesStandbyFixtureI915, GivenStandbyHandleContextWhenCallingStandbyGetThenStandbyInitDoneFlagIsSet, IsXeCore) {
    mockKMDInterfaceSetup();
    EXPECT_FALSE(pSysmanDeviceImp->pStandbyHandleContext->isStandbyInitDone());

    uint32_t count = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEnumStandbyDomains(device, &count, nullptr));
    EXPECT_EQ(count, mockHandleCount);

    EXPECT_TRUE(pSysmanDeviceImp->pStandbyHandleContext->isStandbyInitDone());
}

HWTEST2_F(ZesStandbyFixtureI915, GivenValidStandbyHandlesWhenCallingReInitOnStandbyHandleContextThenHandlesRemainValidAndPropertiesCanStillBeQueried, IsXeCore) {
    mockKMDInterfaceSetup();
    auto handles = getStandbyHandles(mockHandleCount);
    ASSERT_EQ(mockHandleCount, static_cast<uint32_t>(handles.size()));

    pSysmanDeviceImp->pStandbyHandleContext->reInit();

    EXPECT_EQ(mockHandleCount, static_cast<uint32_t>(pSysmanDeviceImp->pStandbyHandleContext->handleList.size()));
    for (auto &handle : pSysmanDeviceImp->pStandbyHandleContext->handleList) {
        ASSERT_NE(nullptr, handle);
        zes_standby_properties_t properties = {};
        EXPECT_EQ(ZE_RESULT_SUCCESS, handle->standbyGetProperties(&properties));
        EXPECT_EQ(ZES_STANDBY_TYPE_GLOBAL, properties.type);
    }
}

TEST_F(ZesStandbyMultiDeviceFixture, GivenXeKmdInterfaceWhenCallingEnumerateThenOneHandlePerSubdeviceIsReturned) {

    std::unique_ptr<SysmanKmdInterface> pSysmanKmdInterfaceXe = std::make_unique<SysmanKmdInterfaceXe>(pLinuxSysmanImp->getSysmanProductHelper());
    std::swap(pLinuxSysmanImp->pSysmanKmdInterface, pSysmanKmdInterfaceXe);
    std::unique_ptr<SysmanProductHelper> pSysmanProductHelper = std::make_unique<SysmanProductHelperHw<IGFX_UNKNOWN>>();
    std::swap(pLinuxSysmanImp->pSysmanProductHelper, pSysmanProductHelper);
    auto pSysfsAccess = std::make_unique<MockStandbySysfsAccessInterface>();
    pSysfsAccess->mockStandbyModeString = standbyPowerControlDefault;
    VariableBackup<L0::Sysman::SysFsAccessInterface *> sysfsBackup(&pLinuxSysmanImp->pSysfsAccess, pSysfsAccess.get());

    uint32_t count = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEnumStandbyDomains(device, &count, nullptr));
    EXPECT_EQ(count, mockSubDeviceHandleCount);

    std::vector<zes_standby_handle_t> handles(count, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEnumStandbyDomains(device, &count, handles.data()));

    for (uint32_t subdeviceId = 0; subdeviceId < count; subdeviceId++) {
        ASSERT_NE(nullptr, handles[subdeviceId]);
        zes_standby_properties_t properties = {};
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesStandbyGetProperties(handles[subdeviceId], &properties));
        EXPECT_EQ(ZES_STANDBY_TYPE_GLOBAL, properties.type);
        EXPECT_TRUE(properties.onSubdevice);
        EXPECT_EQ(subdeviceId, properties.subdeviceId);
    }

    pSysmanDeviceImp->pStandbyHandleContext->handleList.clear();
    std::swap(pLinuxSysmanImp->pSysmanKmdInterface, pSysmanKmdInterfaceXe);
    std::swap(pLinuxSysmanImp->pSysmanProductHelper, pSysmanProductHelper);
}

} // namespace ult
} // namespace Sysman
} // namespace L0
