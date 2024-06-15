/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/sysman/source/shared/windows/product_helper/sysman_product_helper.h"
#include "level_zero/sysman/test/unit_tests/sources/windows/mock_sysman_fixture.h"

#include "gtest/gtest.h"

namespace L0 {
namespace Sysman {
namespace ult {

using SysmanProductHelperTest = SysmanDeviceFixture;

TEST_F(SysmanProductHelperTest, GivenValidWddmSysmanImpObjectWhenCallingGetSysmanProductHelperThenNotNullObjectIsReturned) {
    auto pSysmanProductHelper = pWddmSysmanImp->getSysmanProductHelper();
    EXPECT_NE(nullptr, pSysmanProductHelper);
}

TEST_F(SysmanProductHelperTest, GivenInvalidProductFamilyWhenCallingProductHelperCreateThenNullPtrIsReturned) {
    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(IGFX_UNKNOWN);
    EXPECT_EQ(nullptr, pSysmanProductHelper);
}

} // namespace ult
} // namespace Sysman
} // namespace L0