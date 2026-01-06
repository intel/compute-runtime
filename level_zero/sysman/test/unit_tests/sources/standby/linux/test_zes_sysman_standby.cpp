/*
 * Copyright (C) 2023-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/sysman/source/shared/linux/product_helper/sysman_product_helper_hw.h"
#include "level_zero/sysman/test/unit_tests/sources/linux/mock_sysman_fixture.h"
#include "level_zero/sysman/test/unit_tests/sources/shared/linux/kmd_interface/mock_sysman_kmd_interface_i915.h"
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
    uint32_t subDeviceCount;
    ze_bool_t onSubdevice;
    uint32_t subdeviceId = 0;
};

class ZesStandbyFixtureI915 : public ZesStandbyFixture {
  protected:
    zes_standby_handle_t hSysmanStandby = {};
    MockSysmanKmdInterfacePrelim *pSysmanKmdInterface = nullptr;
    MockStandbySysfsAccessInterface *pSysfsAccess = nullptr;

    void SetUp() override {
        SysmanDeviceFixture::SetUp();
        device = pSysmanDevice;
        pSysmanDeviceImp->pStandbyHandleContext->handleList.clear();
        subDeviceCount = pLinuxSysmanImp->getSubDeviceCount();
        onSubdevice = (subDeviceCount == 0) ? false : true;
    }
    void TearDown() override {
        SysmanDeviceFixture::TearDown();
    }

    void mockKMDInterfaceSetup() {
        pSysmanKmdInterface = new MockSysmanKmdInterfacePrelim(pLinuxSysmanImp->getSysmanProductHelper());
        pSysfsAccess = new MockStandbySysfsAccessInterface();
        pSysmanKmdInterface->pSysfsAccess.reset(pSysfsAccess);
        pLinuxSysmanImp->pSysmanKmdInterface.reset(pSysmanKmdInterface);
        pLinuxSysmanImp->pSysfsAccess = pSysmanKmdInterface->getSysFsAccess();
    }

    std::vector<zes_standby_handle_t> getStandbyHandles(uint32_t count) {
        std::vector<zes_standby_handle_t> handles(count, nullptr);
        EXPECT_EQ(zesDeviceEnumStandbyDomains(device, &count, handles.data()), ZE_RESULT_SUCCESS);
        return handles;
    }
};

TEST_F(ZesStandbyFixtureI915, GivenStandbyModeFilesNotAvailableWhenCallingEnumerateThenSuccessResultAndZeroCountIsReturned) {

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
        bool isStandbySupported(SysmanKmdInterface *pSysmanKmdInterface) override {
            return false;
        }
    };

    std::unique_ptr<SysmanProductHelper> pSysmanProductHelper = std::make_unique<MockSysmanProductHelperStandby>();
    std::swap(pLinuxSysmanImp->pSysmanProductHelper, pSysmanProductHelper);

    uint32_t count = 0;
    ze_result_t result = zesDeviceEnumStandbyDomains(device, &count, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(count, 0u);
}

HWTEST2_F(ZesStandbyFixtureI915, GivenComponentCountZeroWhenCallingzesStandbyGetThenNonZeroCountIsReturnedAndVerifyzesStandbyGetCallSucceeds, IsXeCore) {

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

HWTEST2_F(ZesStandbyFixtureI915, GivenValidStandbyHandleWhenCallingzesStandbyGetPropertiesThenVerifyzesStandbyGetPropertiesCallSucceeds, IsXeCore) {

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

HWTEST2_F(ZesStandbyFixtureI915, GivenValidStandbyHandleWhenCallingzesStandbyGetModeThenVerifyzesStandbyGetModeCallSucceedsForDefaultMode, IsXeCore) {

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

HWTEST2_F(ZesStandbyFixtureI915, GivenValidStandbyHandleWhenCallingzesStandbyGetModeThenVerifyzesStandbyGetModeCallSucceedsForNeverMode, IsXeCore) {

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

HWTEST2_F(ZesStandbyFixtureI915, GivenInvalidStandbyFileWhenReadisCalledThenExpectFailure, IsXeCore) {

    mockKMDInterfaceSetup();
    zes_standby_promo_mode_t mode = {};
    pSysfsAccess->setValReturnError(ZE_RESULT_ERROR_NOT_AVAILABLE);

    auto handles = getStandbyHandles(mockHandleCount);
    for (auto hSysmanStandby : handles) {
        ASSERT_NE(nullptr, hSysmanStandby);
        EXPECT_NE(ZE_RESULT_SUCCESS, zesStandbyGetMode(hSysmanStandby, &mode));
    }
}

HWTEST2_F(ZesStandbyFixtureI915, GivenValidStandbyHandleWhenCallingzesStandbyGetModeThenVerifyzesStandbyGetModeCallFailsForInvalidMode, IsXeCore) {

    mockKMDInterfaceSetup();
    zes_standby_promo_mode_t mode = {};
    pSysfsAccess->setVal(standbyModeFile, standbyModeInvalid);

    auto handles = getStandbyHandles(mockHandleCount);

    for (auto hSysmanStandby : handles) {
        ASSERT_NE(nullptr, hSysmanStandby);
        EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, zesStandbyGetMode(hSysmanStandby, &mode));
    }
}

HWTEST2_F(ZesStandbyFixtureI915, GivenValidStandbyHandleWhenCallingzesStandbyGetModeOnUnavailableFileThenVerifyzesStandbyGetModeCallFailsForUnsupportedFeature, IsXeCore) {

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

HWTEST2_F(ZesStandbyFixtureI915, GivenValidStandbyHandleWhenCallingzesStandbyGetModeWithInsufficientPermissionsThenVerifyzesStandbyGetModeCallFailsForInsufficientPermissions, IsXeCore) {

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

HWTEST2_F(ZesStandbyFixtureI915, GivenValidStandbyHandleWhenCallingzesStandbySetModeThenwithUnwritableFileVerifySysmanzesySetModeCallFailedWithInsufficientPermissions, IsXeCore) {

    mockKMDInterfaceSetup();
    auto handles = getStandbyHandles(mockHandleCount);
    pSysfsAccess->mockStandbyFileMode &= ~S_IWUSR;

    for (auto hSysmanStandby : handles) {
        ASSERT_NE(nullptr, hSysmanStandby);
        EXPECT_EQ(ZE_RESULT_ERROR_INSUFFICIENT_PERMISSIONS, zesStandbySetMode(hSysmanStandby, ZES_STANDBY_PROMO_MODE_NEVER));
    }
}

HWTEST2_F(ZesStandbyFixtureI915, GivenValidStandbyHandleWhenCallingzesStandbySetModeOnUnavailableFileThenVerifyzesStandbySetModeCallFailsForUnsupportedFeature, IsXeCore) {

    mockKMDInterfaceSetup();
    auto handles = getStandbyHandles(mockHandleCount);
    pSysfsAccess->isStandbyModeFileAvailable = false;

    for (auto hSysmanStandby : handles) {
        ASSERT_NE(nullptr, hSysmanStandby);
        EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesStandbySetMode(hSysmanStandby, ZES_STANDBY_PROMO_MODE_NEVER));
    }
}

HWTEST2_F(ZesStandbyFixtureI915, GivenValidStandbyHandleWhenCallingzesStandbySetModeNeverThenVerifySysmanzesSetModeCallSucceeds, IsXeCore) {

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

HWTEST2_F(ZesStandbyFixtureI915, GivenValidStandbyHandleWhenCallingzesStandbySetModeDefaultThenVerifySysmanzesSetModeCallSucceeds, IsXeCore) {

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

HWTEST2_F(ZesStandbyFixtureI915, GivenOnSubdeviceNotSetWhenValidatingosStandbyGetPropertiesThenSuccessIsReturned, IsXeCore) {

    mockKMDInterfaceSetup();
    zes_standby_properties_t properties = {};
    auto pSysfsAccess = std::make_unique<MockStandbySysfsAccessInterface>();
    std::unique_ptr<PublicLinuxStandbyImp> pLinuxStandbyImp = std::make_unique<PublicLinuxStandbyImp>(pOsSysman, onSubdevice, subdeviceId);
    pLinuxStandbyImp->pSysfsAccess = pSysfsAccess.get();
    EXPECT_EQ(ZE_RESULT_SUCCESS, pLinuxStandbyImp->osStandbyGetProperties(properties));
    EXPECT_EQ(properties.subdeviceId, subdeviceId);
    EXPECT_EQ(properties.onSubdevice, onSubdevice);
}

HWTEST2_F(ZesStandbyFixtureI915, GivenValidStandbyHandleWhenCallingzesStandbySetModeDefaultWithLegacyPathThenVerifySysmanzesSetModeCallSucceeds, IsXeCore) {

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

HWTEST2_F(ZesStandbyFixtureI915, GivenValidStandbyHandleWhenCallingzesStandbySetModeNeverWithLegacyPathThenVerifySysmanzesSetModeCallSucceeds, IsXeCore) {

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

class ZesStandbyFixtureXe : public ZesStandbyFixture {
  protected:
    zes_standby_handle_t hSysmanStandby = {};
    std::unique_ptr<SysmanKmdInterface> pSysmanKmdInterface;

    void SetUp() override {
        SysmanDeviceFixture::SetUp();

        pSysmanKmdInterface = std::make_unique<SysmanKmdInterfaceXe>(pLinuxSysmanImp->getSysmanProductHelper());
        device = pSysmanDevice;
        pSysmanDeviceImp->pStandbyHandleContext->handleList.clear();
        subDeviceCount = pLinuxSysmanImp->getSubDeviceCount();
        onSubdevice = (subDeviceCount == 0) ? false : true;
    }
    void TearDown() override {
        SysmanDeviceFixture::TearDown();
    }
};

TEST_F(ZesStandbyFixtureXe, GivenKmdInterfaceWhenQueryingSupportForStandbyForXeVersionThenProperStausIsReturned) {
    EXPECT_EQ(false, pSysmanKmdInterface->isStandbyModeControlAvailable());
}

TEST_F(ZesStandbyFixtureXe, GivenValidDeviceHandleAndStandbyControlNotAvailableWhenCallingEnumerateThenVerifyStandbyDomainsAreZero) {

    VariableBackup<decltype(NEO::SysCalls::sysCallsStat)> mockStat(&NEO::SysCalls::sysCallsStat, [](const std::string &filePath, struct stat *statbuf) -> int {
        statbuf->st_mode = S_IRUSR;
        return 0;
    });

    std::swap(pLinuxSysmanImp->pSysmanKmdInterface, pSysmanKmdInterface);
    auto pSysFsAccess = std::make_unique<MockStandbySysfsAccessInterface>();
    VariableBackup<L0::Sysman::SysFsAccessInterface *> sysfsBackup(&pLinuxSysmanImp->pSysfsAccess, pSysFsAccess.get());

    struct MockSysmanProductHelperStandby : L0::Sysman::SysmanProductHelperHw<IGFX_UNKNOWN> {
        MockSysmanProductHelperStandby() = default;
    };

    std::unique_ptr<SysmanProductHelper> pSysmanProductHelper = std::make_unique<MockSysmanProductHelperStandby>();
    std::swap(pLinuxSysmanImp->pSysmanProductHelper, pSysmanProductHelper);

    uint32_t count = 0;
    ze_result_t result = zesDeviceEnumStandbyDomains(device, &count, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(count, 0u);
}

class ZesStandbyMultiDeviceFixture : public SysmanMultiDeviceFixture {
  protected:
    L0::Sysman::SysmanDevice *device = nullptr;
    uint32_t subDeviceCount;
    ze_bool_t onSubdevice;
    uint32_t subdeviceId = 0;
    void SetUp() override {
        SysmanMultiDeviceFixture::SetUp();
        device = pSysmanDevice;
        mockSubDeviceHandleCount = pLinuxSysmanImp->getSubDeviceCount();
        pSysmanDeviceImp->pStandbyHandleContext->handleList.clear();
        subDeviceCount = pLinuxSysmanImp->getSubDeviceCount();
        onSubdevice = (subDeviceCount == 0) ? false : true;
    }
    void TearDown() override {
        SysmanMultiDeviceFixture::TearDown();
    }
};

HWTEST2_F(ZesStandbyMultiDeviceFixture, GivenComponentCountZeroWhenCallingzesStandbyGetThenNonZeroCountIsReturnedAndVerifyzesStandbyGetCallSucceeds, IsXeCore) {

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

TEST_F(ZesStandbyMultiDeviceFixture, GivenOnSubdeviceNotSetWhenValidatingosStandbyGetPropertiesThenSuccessIsReturned) {

    auto pSysfsAccess = std::make_unique<MockStandbySysfsAccessInterface>();
    std::unique_ptr<PublicLinuxStandbyImp> pLinuxStandbyImp = std::make_unique<PublicLinuxStandbyImp>(pOsSysman, onSubdevice, subdeviceId);
    pLinuxStandbyImp->pSysfsAccess = pSysfsAccess.get();

    zes_standby_properties_t properties = {};
    EXPECT_EQ(ZE_RESULT_SUCCESS, pLinuxStandbyImp->osStandbyGetProperties(properties));
    EXPECT_EQ(properties.subdeviceId, subdeviceId);
    EXPECT_EQ(properties.onSubdevice, onSubdevice);
}

} // namespace ult
} // namespace Sysman
} // namespace L0
