/*
 * Copyright (C) 2023-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/sysman/source/shared/linux/product_helper/sysman_product_helper.h"
#include "level_zero/sysman/test/unit_tests/sources/linux/mock_sysman_fixture.h"

namespace L0 {
namespace Sysman {
namespace ult {

using SysmanProductHelperPowerTest = SysmanDeviceFixture;
HWTEST2_F(SysmanProductHelperPowerTest, GivenValidProductHelperHandleWhenCallingGetPowerLimitValueThenCorrectValueIsReturned, IsPVC) {
    uint64_t testValue = 3000;
    int32_t expectedValue = 3000;
    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    EXPECT_EQ(expectedValue, pSysmanProductHelper->getPowerLimitValue(testValue));
}

HWTEST2_F(SysmanProductHelperPowerTest, GivenValidProductHelperHandleWhenCallingGetPowerLimitValueThenCorrectValueIsReturned, IsNotPVC) {
    uint64_t testValue = 3000;
    int32_t expectedValue = 3;
    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    EXPECT_EQ(expectedValue, pSysmanProductHelper->getPowerLimitValue(testValue));
}

HWTEST2_F(SysmanProductHelperPowerTest, GivenValidProductHelperHandleWhenCallingSetPowerLimitValueThenCorrectValueIsReturned, IsPVC) {
    int32_t testValue = 3000;
    uint64_t expectedValue = 3000;
    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    EXPECT_EQ(expectedValue, pSysmanProductHelper->setPowerLimitValue(testValue));
}

HWTEST2_F(SysmanProductHelperPowerTest, GivenValidProductHelperHandleWhenCallingSetPowerLimitValueThenCorrectValueIsReturned, IsNotPVC) {
    int32_t testValue = 3;
    uint64_t expectedValue = 3000;
    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    EXPECT_EQ(expectedValue, pSysmanProductHelper->setPowerLimitValue(testValue));
}

HWTEST2_F(SysmanProductHelperPowerTest, GivenValidProductHelperHandleWhenCallingGetPowerLimitUnitThenCorrectPowerLimitUnitIsReturned, IsPVC) {
    zes_limit_unit_t expectedPowerUnit = ZES_LIMIT_UNIT_CURRENT;
    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    EXPECT_EQ(expectedPowerUnit, pSysmanProductHelper->getPowerLimitUnit());
}

HWTEST2_F(SysmanProductHelperPowerTest, GivenValidProductHelperHandleWhenCallingGetPowerLimitUnitThenCorrectPowerLimitUnitIsReturned, IsNotPVC) {
    zes_limit_unit_t expectedPowerUnit = ZES_LIMIT_UNIT_POWER;
    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    EXPECT_EQ(expectedPowerUnit, pSysmanProductHelper->getPowerLimitUnit());
}

HWTEST2_F(SysmanProductHelperPowerTest, GivenValidProductHelperHandleWhenCallingIsPowerSetLimitSupportedThenVerifySetRequestIsSupported, IsXeHpOrXeHpcOrXeHpgCore) {
    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    EXPECT_TRUE(pSysmanProductHelper->isPowerSetLimitSupported());
}

} // namespace ult
} // namespace Sysman
} // namespace L0
