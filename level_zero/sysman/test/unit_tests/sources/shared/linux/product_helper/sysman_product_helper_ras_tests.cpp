/*
 * Copyright (C) 2023-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/sysman/source/api/ras/linux/ras_util/sysman_ras_util.h"
#include "level_zero/sysman/source/shared/linux/product_helper/sysman_product_helper.h"

#include "gtest/gtest.h"

namespace L0 {
namespace Sysman {
namespace ult {

using SysmanProductHelperRasTest = ::testing::Test;

HWTEST2_F(SysmanProductHelperRasTest, GivenSysmanProductHelperInstanceWhenQueryingRasInterfaceThenVerifyProperInterfacesAreReturned, IsPVC) {
    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(NEO::defaultHwInfo->platform.eProductFamily);
    EXPECT_EQ(RasInterfaceType::pmu, pSysmanProductHelper->getGtRasUtilInterface());
    EXPECT_EQ(RasInterfaceType::gsc, pSysmanProductHelper->getHbmRasUtilInterface());
}

HWTEST2_F(SysmanProductHelperRasTest, GivenSysmanProductHelperInstanceWhenQueryingRasInterfaceThenVerifyProperInterfacesAreReturned, IsDG2) {
    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(NEO::defaultHwInfo->platform.eProductFamily);
    EXPECT_EQ(RasInterfaceType::pmu, pSysmanProductHelper->getGtRasUtilInterface());
    EXPECT_EQ(RasInterfaceType::none, pSysmanProductHelper->getHbmRasUtilInterface());
}

} // namespace ult
} // namespace Sysman
} // namespace L0
