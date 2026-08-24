/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/sysman/source/shared/linux/product_helper/sysman_product_helper.h"
#include "level_zero/sysman/test/unit_tests/sources/standby/linux/mock_sysfs_standby.h"

namespace L0 {
namespace Sysman {
namespace ult {

using SysmanProductHelperStandbyTest = SysmanDeviceFixture;

HWTEST2_F(SysmanProductHelperStandbyTest, GivenProductHelperWithRenderStandbyNodeWhenQueryingStandbyModeFileThenNodeIsTilePrefixedWheneverBaseDirectoryIsAvailable, IsPVC) {
    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    auto pSysfsAccess = std::make_unique<MockStandbySysfsAccessInterface>();
    auto pSysmanKmdInterface = pLinuxSysmanImp->getSysmanKmdInterface();

    pSysfsAccess->directoryExistsResult = true;
    EXPECT_STREQ(standbyModeFile.c_str(), pSysmanProductHelper->getStandbyModeFile(pSysmanKmdInterface, pSysfsAccess.get(), 0).c_str());
    EXPECT_STREQ(standbyModeFile1.c_str(), pSysmanProductHelper->getStandbyModeFile(pSysmanKmdInterface, pSysfsAccess.get(), 1).c_str());

    pSysfsAccess->directoryExistsResult = false;
    EXPECT_STREQ(standbyModeFileLegacy.c_str(), pSysmanProductHelper->getStandbyModeFile(pSysmanKmdInterface, pSysfsAccess.get(), 0).c_str());
}

HWTEST2_F(SysmanProductHelperStandbyTest, GivenProductHelperWithRenderStandbyNodeWhenGettingAndSettingStandbyModeThenIntegerValuesAreUsed, IsPVC) {
    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    auto pSysfsAccess = std::make_unique<MockStandbySysfsAccessInterface>();
    zes_standby_promo_mode_t mode = {};

    pSysfsAccess->mockStandbyMode = standbyModeNever;
    EXPECT_EQ(ZE_RESULT_SUCCESS, pSysmanProductHelper->getStandbyMode(pSysfsAccess.get(), standbyModeFile, mode));
    EXPECT_EQ(ZES_STANDBY_PROMO_MODE_NEVER, mode);

    EXPECT_EQ(ZE_RESULT_SUCCESS, pSysmanProductHelper->setStandbyMode(pSysfsAccess.get(), standbyModeFile, ZES_STANDBY_PROMO_MODE_DEFAULT));
    EXPECT_EQ(standbyModeDefault, pSysfsAccess->mockStandbyMode);
    EXPECT_EQ(ZE_RESULT_SUCCESS, pSysmanProductHelper->getStandbyMode(pSysfsAccess.get(), standbyModeFile, mode));
    EXPECT_EQ(ZES_STANDBY_PROMO_MODE_DEFAULT, mode);

    pSysfsAccess->mockStandbyMode = standbyModeInvalid;
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, pSysmanProductHelper->getStandbyMode(pSysfsAccess.get(), standbyModeFile, mode));
}

} // namespace ult
} // namespace Sysman
} // namespace L0
