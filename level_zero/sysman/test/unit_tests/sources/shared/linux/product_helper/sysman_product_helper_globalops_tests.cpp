/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/sysman/source/shared/linux/product_helper/sysman_product_helper.h"
#include "level_zero/sysman/test/unit_tests/sources/global_operations/linux/mock_global_operations.h"
#include "level_zero/sysman/test/unit_tests/sources/linux/mock_sysman_fixture.h"

namespace L0 {
namespace Sysman {
namespace ult {

using SysmanProductHelperGlobalOperationsTest = SysmanDeviceFixture;
HWTEST2_F(SysmanProductHelperGlobalOperationsTest, GivenValidProductHelperHandleWhenQueryingRepairStatusSupportThenTrueIsReturned, IsPVC) {
    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    EXPECT_EQ(true, pSysmanProductHelper->isRepairStatusSupported());
}

HWTEST2_F(SysmanProductHelperGlobalOperationsTest, GivenValidProductHelperHandleWhenQueryingRepairStatusSupportThenFalseIsReturned, IsNotPVC) {
    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    EXPECT_EQ(false, pSysmanProductHelper->isRepairStatusSupported());
}

} // namespace ult
} // namespace Sysman
} // namespace L0