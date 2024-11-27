/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/sysman/source/shared/windows/product_helper/sysman_product_helper.h"
#include "level_zero/sysman/test/unit_tests/sources/windows/mock_sysman_fixture.h"

namespace L0 {
namespace Sysman {
namespace ult {
using SysmanProductHelperSysmanInitTest = SysmanDeviceFixture;

HWTEST2_F(SysmanProductHelperSysmanInitTest, GivenValidProductHelperHandleWhenQueryingForZesInitSupportThenTrueIsReturned, IsLNL) {
    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    EXPECT_TRUE(pSysmanProductHelper->isZesInitSupported());
}

HWTEST2_F(SysmanProductHelperSysmanInitTest, GivenValidProductHelperHandleWhenQueryingForZesInitSupportThenTrueIsReturned, IsBMG) {
    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    EXPECT_TRUE(pSysmanProductHelper->isZesInitSupported());
}

HWTEST2_F(SysmanProductHelperSysmanInitTest, GivenValidProductHelperHandleWhenQueryingForZesInitSupportThenFalseIsReturned, IsPVC) {
    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    EXPECT_FALSE(pSysmanProductHelper->isZesInitSupported());
}

} // namespace ult
} // namespace Sysman
} // namespace L0
