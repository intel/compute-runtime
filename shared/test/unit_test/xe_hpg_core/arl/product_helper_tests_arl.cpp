/*
 * Copyright (C) 2022-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/compiler_product_helper.h"
#include "shared/source/xe_hpg_core/hw_cmds_arl.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/test_macros/header/per_product_test_definitions.h"
#include "shared/test/common/test_macros/test.h"
#include "shared/test/unit_test/os_interface/product_helper_tests.h"

#include "platforms.h"

using namespace NEO;

using ArlProductHelper = ProductHelperTest;

ARLTEST_F(ArlProductHelper, givenArlWithoutHwIpVersionInHwInfoWhenGettingIpVersionThenCorrectValueIsReturnedBasedOnDeviceIdAndRevId) {
    auto hwInfo = *defaultHwInfo;
    hwInfo.ipVersion = {};

    auto arlSDeviceIds = {0x7D67};
    auto arlHDeviceIds = {0x7D51, 0x7DD1, 0x7D41};

    for (auto &deviceId : arlSDeviceIds) {
        hwInfo.platform.usDeviceID = deviceId;
        for (auto &revision : {0}) {
            hwInfo.platform.usRevId = revision;

            EXPECT_EQ(AOT::MTL_M_A0, compilerProductHelper->getHwIpVersion(hwInfo));
        }
        for (auto &revision : {3, 6}) {
            hwInfo.platform.usRevId = revision;

            EXPECT_EQ(AOT::MTL_M_B0, compilerProductHelper->getHwIpVersion(hwInfo));
        }
        hwInfo.platform.usRevId = 0xdead;

        EXPECT_EQ(compilerProductHelper->getDefaultHwIpVersion(), compilerProductHelper->getHwIpVersion(hwInfo));
    }

    for (auto &deviceId : arlHDeviceIds) {
        hwInfo.platform.usDeviceID = deviceId;
        for (auto &revision : {0, 3}) {
            hwInfo.platform.usRevId = revision;

            EXPECT_EQ(AOT::XE_LPGPLUS_A0, compilerProductHelper->getHwIpVersion(hwInfo));
        }
        for (auto &revision : {6}) {
            hwInfo.platform.usRevId = revision;

            EXPECT_EQ(AOT::XE_LPGPLUS_B0, compilerProductHelper->getHwIpVersion(hwInfo));
        }
        hwInfo.platform.usRevId = 0xdead;

        EXPECT_EQ(compilerProductHelper->getDefaultHwIpVersion(), compilerProductHelper->getHwIpVersion(hwInfo));
    }

    hwInfo.platform.usDeviceID = 0;
    hwInfo.platform.usRevId = 0xdead;

    EXPECT_EQ(compilerProductHelper->getDefaultHwIpVersion(), compilerProductHelper->getHwIpVersion(hwInfo));
}
