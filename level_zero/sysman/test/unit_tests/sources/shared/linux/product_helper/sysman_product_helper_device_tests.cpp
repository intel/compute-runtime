/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/sysman/source/shared/linux/product_helper/sysman_product_helper.h"
#include "level_zero/sysman/test/unit_tests/sources/linux/mock_sysman_fixture.h"

namespace L0 {
namespace Sysman {
namespace ult {

using HasUpstreamPort = IsAnyProducts<IGFX_PVC,
                                      IGFX_BMG,
                                      IGFX_DG1,
                                      IGFX_DG2>;

using SysmanProductHelperDeviceTest = SysmanDeviceFixture;
HWTEST2_F(SysmanProductHelperDeviceTest, GivenValidProductHelperHandleWhenQueryingForUpstreamPortConnectivityThenVerifyUpstreamPortIsConnected, HasUpstreamPort) {
    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    EXPECT_TRUE(pSysmanProductHelper->isUpstreamPortConnected());
}

HWTEST2_F(SysmanProductHelperDeviceTest, GivenValidProductHelperHandleWhenQueryingForUpstreamPortConnectivityThenVerifyUpstreamPortIsNotConnected, IsMTL) {
    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    EXPECT_FALSE(pSysmanProductHelper->isUpstreamPortConnected());
}

} // namespace ult
} // namespace Sysman
} // namespace L0