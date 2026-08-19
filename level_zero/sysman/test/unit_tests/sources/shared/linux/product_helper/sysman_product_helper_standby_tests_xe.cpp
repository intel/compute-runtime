/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/sysman/source/shared/linux/product_helper/sysman_product_helper.h"
#include "level_zero/sysman/test/unit_tests/sources/linux/mock_sysman_fixture.h"
#include "level_zero/sysman/test/unit_tests/sources/shared/linux/kmd_interface/mock_sysman_kmd_interface_xe.h"
#include "level_zero/sysman/test/unit_tests/sources/standby/linux/mock_sysfs_standby.h"

namespace L0 {
namespace Sysman {
namespace ult {

constexpr uint32_t mockStandbyHandleCount = 1u;

class SysmanProductHelperStandbyXeFixture : public SysmanDeviceFixture {
  protected:
    std::unique_ptr<SysmanProductHelper> pSysmanProductHelper;
    std::unique_ptr<MockSysmanKmdInterfaceXe> pSysmanKmdInterface;
    std::unique_ptr<MockStandbySysfsAccessInterface> pSysfsAccess;

    void SetUp() override {
        SysmanDeviceFixture::SetUp();
        pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
        pSysmanKmdInterface = std::make_unique<MockSysmanKmdInterfaceXe>(pSysmanProductHelper.get());
        pSysfsAccess = std::make_unique<MockStandbySysfsAccessInterface>();
    }
    void TearDown() override {
        SysmanDeviceFixture::TearDown();
    }
};

HWTEST2_F(SysmanProductHelperStandbyXeFixture, GivenValidProductHelperHandleWhenQueryingStandbySupportThenStandbyAndSetModeAreBothSupported, IsBmgOrCri) {
    EXPECT_TRUE(pSysmanProductHelper->isStandbySupported(pSysmanKmdInterface.get()));
    EXPECT_TRUE(pSysmanProductHelper->isSetStandbyModeSupported());
}

HWTEST2_F(SysmanProductHelperStandbyXeFixture, GivenValidProductHelperHandleWhenQueryingStandbyModeFileThenRuntimePowerManagementNodeIsReturnedWithoutTilePrefix, IsBmgOrCri) {
    for (uint32_t subDeviceId = 0; subDeviceId < 2u; subDeviceId++) {
        EXPECT_STREQ(standbyModeFileXe.c_str(),
                     pSysmanProductHelper->getStandbyModeFile(pSysmanKmdInterface.get(), pSysfsAccess.get(), subDeviceId).c_str());
    }
}

HWTEST2_F(SysmanProductHelperStandbyXeFixture, GivenValidProductHelperHandleWhenGettingStandbyModeThenStringNodeValuesAreTranslated, IsBmgOrCri) {
    zes_standby_promo_mode_t mode = {};

    pSysfsAccess->mockStandbyModeString = standbyModeXeDefault;
    EXPECT_EQ(ZE_RESULT_SUCCESS, pSysmanProductHelper->getStandbyMode(pSysfsAccess.get(), standbyModeFileXe, mode));
    EXPECT_EQ(ZES_STANDBY_PROMO_MODE_DEFAULT, mode);

    pSysfsAccess->mockStandbyModeString = standbyModeXeNever;
    EXPECT_EQ(ZE_RESULT_SUCCESS, pSysmanProductHelper->getStandbyMode(pSysfsAccess.get(), standbyModeFileXe, mode));
    EXPECT_EQ(ZES_STANDBY_PROMO_MODE_NEVER, mode);
}

HWTEST2_F(SysmanProductHelperStandbyXeFixture, GivenValidProductHelperHandleWhenSettingStandbyModeThenStringNodeValuesAreWritten, IsBmgOrCri) {
    EXPECT_EQ(ZE_RESULT_SUCCESS, pSysmanProductHelper->setStandbyMode(pSysfsAccess.get(), standbyModeFileXe, ZES_STANDBY_PROMO_MODE_NEVER));
    EXPECT_STREQ(standbyModeXeNever.c_str(), pSysfsAccess->mockStandbyModeString.c_str());

    EXPECT_EQ(ZE_RESULT_SUCCESS, pSysmanProductHelper->setStandbyMode(pSysfsAccess.get(), standbyModeFileXe, ZES_STANDBY_PROMO_MODE_DEFAULT));
    EXPECT_STREQ(standbyModeXeDefault.c_str(), pSysfsAccess->mockStandbyModeString.c_str());
}

HWTEST2_F(SysmanProductHelperStandbyXeFixture, GivenValidProductHelperHandleWhenGettingStandbyModeWithUnknownNodeValueThenUnknownErrorIsReturned, IsBmgOrCri) {
    zes_standby_promo_mode_t mode = {};

    pSysfsAccess->mockStandbyModeString = "unknown";
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, pSysmanProductHelper->getStandbyMode(pSysfsAccess.get(), standbyModeFileXe, mode));
}

HWTEST2_F(SysmanProductHelperStandbyXeFixture, GivenValidProductHelperHandleWhenGettingStandbyModeAndReadFailsThenErrorIsPropagated, IsBmgOrCri) {
    zes_standby_promo_mode_t mode = {};

    pSysfsAccess->setValReturnError(ZE_RESULT_ERROR_NOT_AVAILABLE);
    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, pSysmanProductHelper->getStandbyMode(pSysfsAccess.get(), standbyModeFileXe, mode));
}

class SysmanStandbyXeFixture : public SysmanDeviceFixture {
  protected:
    L0::Sysman::SysmanDevice *device = nullptr;
    MockStandbySysfsAccessInterface *pSysfsAccess = nullptr;

    void SetUp() override {
        SysmanDeviceFixture::SetUp();
        device = pSysmanDevice;
        pSysmanDeviceImp->pStandbyHandleContext->handleList.clear();
    }
    void TearDown() override {
        SysmanDeviceFixture::TearDown();
    }

    void mockKMDInterfaceSetup() {
        auto pSysmanKmdInterface = new MockSysmanKmdInterfaceXe(pLinuxSysmanImp->getSysmanProductHelper());
        pSysfsAccess = new MockStandbySysfsAccessInterface();
        pSysfsAccess->mockStandbyModeString = standbyModeXeDefault;
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

HWTEST2_F(SysmanStandbyXeFixture, GivenValidDeviceHandleWhenCallingEnumerateThenStandbyDomainIsReturned, IsBmgOrCri) {

    mockKMDInterfaceSetup();

    uint32_t count = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEnumStandbyDomains(device, &count, nullptr));
    EXPECT_EQ(count, mockStandbyHandleCount);

    auto handles = getStandbyHandles(count);
    for (auto hSysmanStandby : handles) {
        ASSERT_NE(nullptr, hSysmanStandby);
        zes_standby_properties_t properties = {};
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesStandbyGetProperties(hSysmanStandby, &properties));
        EXPECT_EQ(nullptr, properties.pNext);
        EXPECT_EQ(ZES_STANDBY_TYPE_GLOBAL, properties.type);
        EXPECT_FALSE(properties.onSubdevice);
        EXPECT_EQ(0u, properties.subdeviceId);
    }
}

HWTEST2_F(SysmanStandbyXeFixture, GivenValidStandbyHandleWhenCallingZesStandbyGetModeThenModeIsReadFromPowerControlNode, IsBmgOrCri) {

    mockKMDInterfaceSetup();
    auto handles = getStandbyHandles(mockStandbyHandleCount);

    for (auto hSysmanStandby : handles) {
        ASSERT_NE(nullptr, hSysmanStandby);
        zes_standby_promo_mode_t mode = {};
        pSysfsAccess->mockStandbyModeString = standbyModeXeDefault;
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesStandbyGetMode(hSysmanStandby, &mode));
        EXPECT_EQ(ZES_STANDBY_PROMO_MODE_DEFAULT, mode);

        pSysfsAccess->mockStandbyModeString = standbyModeXeNever;
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesStandbyGetMode(hSysmanStandby, &mode));
        EXPECT_EQ(ZES_STANDBY_PROMO_MODE_NEVER, mode);
    }
}

HWTEST2_F(SysmanStandbyXeFixture, GivenValidStandbyHandleWhenCallingZesStandbySetModeThenPowerControlNodeIsUpdated, IsBmgOrCri) {

    mockKMDInterfaceSetup();
    auto handles = getStandbyHandles(mockStandbyHandleCount);

    for (auto hSysmanStandby : handles) {
        ASSERT_NE(nullptr, hSysmanStandby);
        zes_standby_promo_mode_t mode = {};
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesStandbySetMode(hSysmanStandby, ZES_STANDBY_PROMO_MODE_NEVER));
        EXPECT_STREQ(standbyModeXeNever.c_str(), pSysfsAccess->mockStandbyModeString.c_str());
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesStandbyGetMode(hSysmanStandby, &mode));
        EXPECT_EQ(ZES_STANDBY_PROMO_MODE_NEVER, mode);

        EXPECT_EQ(ZE_RESULT_SUCCESS, zesStandbySetMode(hSysmanStandby, ZES_STANDBY_PROMO_MODE_DEFAULT));
        EXPECT_STREQ(standbyModeXeDefault.c_str(), pSysfsAccess->mockStandbyModeString.c_str());
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesStandbyGetMode(hSysmanStandby, &mode));
        EXPECT_EQ(ZES_STANDBY_PROMO_MODE_DEFAULT, mode);
    }
}

HWTEST2_F(SysmanStandbyXeFixture, GivenValidStandbyHandleWhenCallingZesStandbyGetModeWithUnknownNodeValueThenCallFailsWithUnknownError, IsBmgOrCri) {

    mockKMDInterfaceSetup();
    auto handles = getStandbyHandles(mockStandbyHandleCount);
    pSysfsAccess->mockStandbyModeString = "unknown";

    for (auto hSysmanStandby : handles) {
        ASSERT_NE(nullptr, hSysmanStandby);
        zes_standby_promo_mode_t mode = {};
        EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, zesStandbyGetMode(hSysmanStandby, &mode));
    }
}

HWTEST2_F(SysmanStandbyXeFixture, GivenValidStandbyHandleWhenCallingZesStandbyGetModeOnUnavailableFileThenCallFailsWithUnsupportedFeature, IsBmgOrCri) {

    mockKMDInterfaceSetup();
    auto handles = getStandbyHandles(mockStandbyHandleCount);
    pSysfsAccess->isStandbyModeFileAvailable = false;

    for (auto hSysmanStandby : handles) {
        ASSERT_NE(nullptr, hSysmanStandby);
        zes_standby_promo_mode_t mode = {};
        EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesStandbyGetMode(hSysmanStandby, &mode));
    }
}

HWTEST2_F(SysmanStandbyXeFixture, GivenValidStandbyHandleWhenCallingZesStandbyGetModeWithInsufficientPermissionsThenCallFailsWithInsufficientPermissions, IsBmgOrCri) {

    mockKMDInterfaceSetup();
    auto handles = getStandbyHandles(mockStandbyHandleCount);
    pSysfsAccess->mockStandbyFileMode &= ~S_IRUSR;

    for (auto hSysmanStandby : handles) {
        ASSERT_NE(nullptr, hSysmanStandby);
        zes_standby_promo_mode_t mode = {};
        EXPECT_EQ(ZE_RESULT_ERROR_INSUFFICIENT_PERMISSIONS, zesStandbyGetMode(hSysmanStandby, &mode));
    }
}

HWTEST2_F(SysmanStandbyXeFixture, GivenValidStandbyHandleWhenCallingZesStandbySetModeWithUnwritableFileThenCallFailsWithInsufficientPermissions, IsBmgOrCri) {

    mockKMDInterfaceSetup();
    auto handles = getStandbyHandles(mockStandbyHandleCount);
    pSysfsAccess->mockStandbyFileMode &= ~S_IWUSR;

    for (auto hSysmanStandby : handles) {
        ASSERT_NE(nullptr, hSysmanStandby);
        EXPECT_EQ(ZE_RESULT_ERROR_INSUFFICIENT_PERMISSIONS, zesStandbySetMode(hSysmanStandby, ZES_STANDBY_PROMO_MODE_NEVER));
    }
}

HWTEST2_F(SysmanStandbyXeFixture, GivenValidStandbyHandleWhenCallingZesStandbySetModeOnUnavailableFileThenCallFailsWithUnsupportedFeature, IsBmgOrCri) {

    mockKMDInterfaceSetup();
    auto handles = getStandbyHandles(mockStandbyHandleCount);
    pSysfsAccess->isStandbyModeFileAvailable = false;

    for (auto hSysmanStandby : handles) {
        ASSERT_NE(nullptr, hSysmanStandby);
        EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesStandbySetMode(hSysmanStandby, ZES_STANDBY_PROMO_MODE_NEVER));
    }
}

HWTEST2_F(SysmanStandbyXeFixture, GivenPowerControlNodeIsNotTilePrefixedWhenValidatingSupportForSubdeviceHandleThenSupportIsReportedForEverySubdevice, IsBmgOrCri) {

    mockKMDInterfaceSetup();

    for (uint32_t subdeviceId = 0; subdeviceId < 2u; subdeviceId++) {
        std::unique_ptr<PublicLinuxStandbyImp> pLinuxStandbyImp = std::make_unique<PublicLinuxStandbyImp>(pOsSysman, true, subdeviceId);
        EXPECT_TRUE(pLinuxStandbyImp->isStandbySupported());
    }
}

} // namespace ult
} // namespace Sysman
} // namespace L0
