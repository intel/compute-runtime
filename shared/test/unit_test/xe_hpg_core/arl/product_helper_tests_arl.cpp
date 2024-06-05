/*
 * Copyright (C) 2022-2024 Intel Corporation
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

    auto arlSUDeviceIds = {0x7D67, 0x7D41};
    auto arlHDeviceIds = {0x7D51, 0x7DD1};

    for (auto &deviceId : arlSUDeviceIds) {
        hwInfo.platform.usDeviceID = deviceId;
        for (auto &revision : {0}) {
            hwInfo.platform.usRevId = revision;

            EXPECT_EQ(AOT::MTL_U_A0, compilerProductHelper->getHwIpVersion(hwInfo));
        }
        for (auto &revision : {3, 6}) {
            hwInfo.platform.usRevId = revision;

            EXPECT_EQ(AOT::MTL_U_B0, compilerProductHelper->getHwIpVersion(hwInfo));
        }
        hwInfo.platform.usRevId = 0xdead;

        EXPECT_EQ(compilerProductHelper->getDefaultHwIpVersion(), compilerProductHelper->getHwIpVersion(hwInfo));
    }

    for (auto &deviceId : arlHDeviceIds) {
        hwInfo.platform.usDeviceID = deviceId;
        for (auto &revision : {0, 3}) {
            hwInfo.platform.usRevId = revision;

            EXPECT_EQ(AOT::ARL_H_A0, compilerProductHelper->getHwIpVersion(hwInfo));
        }
        for (auto &revision : {6}) {
            hwInfo.platform.usRevId = revision;

            EXPECT_EQ(AOT::ARL_H_B0, compilerProductHelper->getHwIpVersion(hwInfo));
        }
        hwInfo.platform.usRevId = 0xdead;

        EXPECT_EQ(compilerProductHelper->getDefaultHwIpVersion(), compilerProductHelper->getHwIpVersion(hwInfo));
    }

    hwInfo.platform.usDeviceID = 0;
    hwInfo.platform.usRevId = 0xdead;

    EXPECT_EQ(compilerProductHelper->getDefaultHwIpVersion(), compilerProductHelper->getHwIpVersion(hwInfo));
}

ARLTEST_F(ArlProductHelper, givenPubliclyAvailableAcronymsForMtlDevicesWhenGetProductConfigThenCorrectValueIsReturned) {
    auto productConfigHelper = std::make_unique<ProductConfigHelper>();
    std::vector<std::string> arlSAcronyms = {"arl-s", "arl-u"};
    std::vector<std::string> arlHAcronyms = {"arl-h"};
    for (auto &acronym : arlSAcronyms) {
        ProductConfigHelper::adjustDeviceName(acronym);
        auto ret = productConfigHelper->getProductConfigFromDeviceName(acronym);
        EXPECT_EQ(ret, AOT::MTL_U_B0);
    }
    for (auto &acronym : arlHAcronyms) {
        ProductConfigHelper::adjustDeviceName(acronym);
        auto ret = productConfigHelper->getProductConfigFromDeviceName(acronym);
        EXPECT_EQ(ret, AOT::ARL_H_B0);
    }
}
