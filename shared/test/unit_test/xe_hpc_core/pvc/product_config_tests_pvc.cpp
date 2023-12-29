/*
 * Copyright (C) 2022-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/product_helper.h"
#include "shared/source/xe_hpc_core/hw_cmds_xe_hpc_core_base.h"
#include "shared/source/xe_hpc_core/pvc/device_ids_configs_pvc.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/test_macros/header/per_product_test_definitions.h"
#include "shared/test/common/test_macros/test.h"
#include "shared/test/unit_test/fixtures/product_config_fixture.h"

using namespace NEO;

PVCTEST_F(ProductConfigTests, givenPvcXlDeviceIdWhenDifferentRevisionIsPassedThenCorrectProductConfigIsReturned) {
    for (const auto &deviceId : pvcXlDeviceIds) {
        hwInfo.platform.usDeviceID = deviceId;

        hwInfo.platform.usRevId = 0x0;
        productConfig = compilerProductHelper->getHwIpVersion(hwInfo);
        EXPECT_EQ(productConfig, AOT::PVC_XL_A0);

        hwInfo.platform.usRevId = 0x1;
        productConfig = compilerProductHelper->getHwIpVersion(hwInfo);
        EXPECT_EQ(productConfig, AOT::PVC_XL_A0P);

        hwInfo.platform.usRevId = 0x6;
        productConfig = compilerProductHelper->getHwIpVersion(hwInfo);
        EXPECT_EQ(productConfig, AOT::PVC_XL_A0P);

        hwInfo.platform.usRevId = 0x10;
        productConfig = compilerProductHelper->getHwIpVersion(hwInfo);
        EXPECT_EQ(productConfig, AOT::PVC_XL_A0);

        hwInfo.platform.usRevId = 0x11;
        productConfig = compilerProductHelper->getHwIpVersion(hwInfo);
        EXPECT_EQ(productConfig, AOT::PVC_XL_A0P);

        hwInfo.platform.usRevId = 0x16;
        productConfig = compilerProductHelper->getHwIpVersion(hwInfo);
        EXPECT_EQ(productConfig, AOT::PVC_XL_A0P);
    }
}

PVCTEST_F(ProductConfigTests, givenPvcXtDeviceIdWhenDifferentRevisionIsPassedThenCorrectProductConfigIsReturned) {
    for (const auto &deviceId : pvcXtDeviceIds) {
        hwInfo.platform.usDeviceID = deviceId;

        hwInfo.platform.usRevId = 0x3;
        productConfig = compilerProductHelper->getHwIpVersion(hwInfo);
        EXPECT_EQ(productConfig, AOT::PVC_XT_A0);

        hwInfo.platform.usRevId = 0x5;
        productConfig = compilerProductHelper->getHwIpVersion(hwInfo);
        EXPECT_EQ(productConfig, AOT::PVC_XT_B0);

        hwInfo.platform.usRevId = 0x6;
        productConfig = compilerProductHelper->getHwIpVersion(hwInfo);
        EXPECT_EQ(productConfig, AOT::PVC_XT_B1);

        hwInfo.platform.usRevId = 0x7;
        productConfig = compilerProductHelper->getHwIpVersion(hwInfo);
        EXPECT_EQ(productConfig, AOT::PVC_XT_C0);

        hwInfo.platform.usRevId = 0x13;
        productConfig = compilerProductHelper->getHwIpVersion(hwInfo);
        EXPECT_EQ(productConfig, AOT::PVC_XT_A0);

        hwInfo.platform.usRevId = 0x15;
        productConfig = compilerProductHelper->getHwIpVersion(hwInfo);
        EXPECT_EQ(productConfig, AOT::PVC_XT_B0);

        hwInfo.platform.usRevId = 0x16;
        productConfig = compilerProductHelper->getHwIpVersion(hwInfo);
        EXPECT_EQ(productConfig, AOT::PVC_XT_B1);

        hwInfo.platform.usRevId = 0x17;
        productConfig = compilerProductHelper->getHwIpVersion(hwInfo);
        EXPECT_EQ(productConfig, AOT::PVC_XT_C0);
    }

    for (const auto &deviceId : pvcXtVgDeviceIds) {
        hwInfo.platform.usDeviceID = deviceId;

        hwInfo.platform.usRevId = 0x17;
        productConfig = compilerProductHelper->getHwIpVersion(hwInfo);
        EXPECT_EQ(productConfig, AOT::PVC_XT_C0_VG);
    }
}

PVCTEST_F(ProductConfigTests, givenDefaultDeviceAndRevisionIdWhenGetProductConfigThenPvcXtC0ConfigIsReturned) {
    hwInfo.platform.usRevId = 0;
    hwInfo.platform.usDeviceID = defaultHwInfo->platform.usDeviceID;

    productConfig = compilerProductHelper->getHwIpVersion(hwInfo);
    EXPECT_EQ(productConfig, AOT::PVC_XT_C0);
}

PVCTEST_F(ProductConfigTests, givenInvalidRevisionIdForPvcXtWhenGetProductConfigThenLatestPvcXtIsReturned) {
    for (const auto &deviceId : pvcXtDeviceIds) {
        hwInfo.platform.usDeviceID = deviceId;

        hwInfo.platform.usRevId = 0x2;
        productConfig = compilerProductHelper->getHwIpVersion(hwInfo);
        EXPECT_EQ(productConfig, AOT::PVC_XT_C0);

        hwInfo.platform.usRevId = 0x4;
        productConfig = compilerProductHelper->getHwIpVersion(hwInfo);
        EXPECT_EQ(productConfig, AOT::PVC_XT_C0);
    }
}

PVCTEST_F(ProductConfigTests, givenInvalidRevisionIdForPvcXlWhenGetProductConfigThenLatestPvcXlIsReturned) {
    for (const auto &deviceId : pvcXlDeviceIds) {
        hwInfo.platform.usDeviceID = deviceId;

        hwInfo.platform.usRevId = 0x2;
        productConfig = compilerProductHelper->getHwIpVersion(hwInfo);
        EXPECT_EQ(productConfig, AOT::PVC_XL_A0P);

        hwInfo.platform.usRevId = 0x4;
        productConfig = compilerProductHelper->getHwIpVersion(hwInfo);
        EXPECT_EQ(productConfig, AOT::PVC_XL_A0P);
    }
}

PVCTEST_F(ProductConfigTests, givenUnknownDeviceIdWhenGetProductConfigThenDefaultConfigIsReturned) {
    hwInfo.platform.usDeviceID = 0;
    productConfig = compilerProductHelper->getHwIpVersion(hwInfo);
    EXPECT_EQ(productConfig, AOT::PVC_XT_C0);
}
