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

HWTEST2_F(ZesStandbyFixtureXe, GivenProductHelperWithPciRuntimePowerManagementNodeWhenQueryingStandbyModeFileThenNodeIsNotTilePrefixedForAnySubdevice, IsBMG) {
    mockKMDInterfaceSetup();
    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);

    for (uint32_t subDeviceId = 0; subDeviceId < 2u; subDeviceId++) {
        EXPECT_STREQ(standbyModeFilePciControl.c_str(),
                     pSysmanProductHelper->getStandbyModeFile(pLinuxSysmanImp->getSysmanKmdInterface(), pSysfsAccess.get(), subDeviceId).c_str());
    }
}

HWTEST2_F(ZesStandbyFixtureXe, GivenProductHelperWithPciRuntimePowerManagementNodeWhenGettingAndSettingStandbyModeThenStringValuesAreUsed, IsBMG) {
    mockKMDInterfaceSetup();
    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    zes_standby_promo_mode_t mode = {};

    pSysfsAccess->mockStandbyModeString = standbyPowerControlNever;
    EXPECT_EQ(ZE_RESULT_SUCCESS, pSysmanProductHelper->getStandbyMode(pSysfsAccess.get(), standbyModeFilePciControl, mode));
    EXPECT_EQ(ZES_STANDBY_PROMO_MODE_NEVER, mode);

    EXPECT_EQ(ZE_RESULT_SUCCESS, pSysmanProductHelper->setStandbyMode(pSysfsAccess.get(), standbyModeFilePciControl, ZES_STANDBY_PROMO_MODE_DEFAULT));
    EXPECT_STREQ(standbyPowerControlDefault.c_str(), pSysfsAccess->mockStandbyModeString.c_str());
    EXPECT_EQ(ZE_RESULT_SUCCESS, pSysmanProductHelper->getStandbyMode(pSysfsAccess.get(), standbyModeFilePciControl, mode));
    EXPECT_EQ(ZES_STANDBY_PROMO_MODE_DEFAULT, mode);

    pSysfsAccess->mockStandbyModeString = "unknown";
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, pSysmanProductHelper->getStandbyMode(pSysfsAccess.get(), standbyModeFilePciControl, mode));
}

} // namespace ult
} // namespace Sysman
} // namespace L0
